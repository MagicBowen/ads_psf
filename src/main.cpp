#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <optional>
#include <algorithm>

// 任务ID类型
using TaskId = uint64_t;

// 处理状态枚举
enum class ProcessStatus {
    OK,
    ERROR,
    PENDING
};

// 前向声明ProcessContext类
class ProcessContext;

// 消息结构
struct Message {
    TaskId taskId;
    std::string taskName;
    std::function<ProcessStatus(ProcessContext&)> taskFunction;
    ProcessContext* context;
};

// 响应结构
struct Response {
    TaskId taskId;
    std::string taskName;
    ProcessStatus status;
};

//=============================================================================
// 通用线程安全队列实现 - 不再区分SPSC/SPMC/MPSC
//=============================================================================

template<typename T>
class ThreadSafeQueue {
public:
    void Push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        condition_.notify_one();
    }

    bool Pop(T& item, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (timeout) {
            // 有超时的等待
            bool hasItem = condition_.wait_for(lock, *timeout, 
                [this] { return !queue_.empty() || shutdown_; });
            
            if (!hasItem || (shutdown_ && queue_.empty()))
                return false;
        } else {
            // 无限期等待
            condition_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
            
            if (shutdown_ && queue_.empty())
                return false;
        }
            
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool TryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;
            
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        condition_.notify_all();
    }

    bool IsEmpty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool shutdown_{false};
};

//=============================================================================
// 响应等待器 - 处理阻塞等待接口
//=============================================================================

class ResponseWaiter {
public:
    ResponseWaiter(ThreadSafeQueue<Response*>& responseQueue)
        : responseQueue_(responseQueue) {}
    
    // 等待特定任务ID的响应（无限期等待）
    Response* WaitForTask(TaskId taskId) {
        return WaitForTaskWithTimeout(taskId, std::nullopt);
    }
    
    // 等待特定任务ID的响应（支持超时）
    Response* WaitForTaskWithTimeout(TaskId taskId, 
                                    std::optional<std::chrono::milliseconds> timeout) {
        // 首先检查已收集的响应
        {
            std::lock_guard<std::mutex> lock(collectedMutex_);
            auto it = std::find_if(collectedResponses_.begin(), collectedResponses_.end(),
                [taskId](Response* resp) { return resp->taskId == taskId; });
            
            if (it != collectedResponses_.end()) {
                Response* result = *it;
                collectedResponses_.erase(it);
                return result;
            }
        }
        
        // 决定等待时间
        auto startTime = std::chrono::steady_clock::now();
        auto remainingTime = timeout;
        
        // 持续尝试获取响应
        while (true) {
            Response* response = nullptr;
            bool success = responseQueue_.Pop(response, remainingTime);
            
            if (!success) {
                // 超时或队列关闭
                return nullptr;
            }
            
            if (response->taskId == taskId) {
                return response;
            } else {
                // 不是我们要等的任务，保存起来
                std::lock_guard<std::mutex> lock(collectedMutex_);
                collectedResponses_.push_back(response);
            }
            
            // 更新剩余时间
            if (timeout) {
                auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime);
                
                if (elapsedTime >= *timeout) {
                    return nullptr; // 超时
                }
                
                remainingTime = std::chrono::milliseconds(*timeout - elapsedTime);
            }
        }
    }
    
    // 等待一组任务的所有响应
    std::vector<Response*> WaitForAllTasks(const std::vector<TaskId>& taskIds, 
                                          std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        if (taskIds.empty()) {
            return {};
        }
        
        std::vector<Response*> results;
        std::vector<TaskId> remainingIds(taskIds);
        
        // 首先检查已收集的响应
        {
            std::lock_guard<std::mutex> lock(collectedMutex_);
            
            auto it = collectedResponses_.begin();
            while (it != collectedResponses_.end()) {
                auto idIt = std::find(remainingIds.begin(), remainingIds.end(), (*it)->taskId);
                if (idIt != remainingIds.end()) {
                    results.push_back(*it);
                    remainingIds.erase(idIt);
                    it = collectedResponses_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        // 如果已经收集到所有任务，直接返回
        if (remainingIds.empty()) {
            return results;
        }
        
        // 决定等待时间
        auto startTime = std::chrono::steady_clock::now();
        auto remainingTime = timeout;
        
        // 等待剩余的任务响应
        while (!remainingIds.empty()) {
            Response* response = nullptr;
            bool success = responseQueue_.Pop(response, remainingTime);
            
            if (!success) {
                // 超时或队列关闭
                break;
            }
            
            auto idIt = std::find(remainingIds.begin(), remainingIds.end(), response->taskId);
            if (idIt != remainingIds.end()) {
                results.push_back(response);
                remainingIds.erase(idIt);
            } else {
                // 不是我们要等的任务，保存起来
                std::lock_guard<std::mutex> lock(collectedMutex_);
                collectedResponses_.push_back(response);
            }
            
            // 更新剩余时间
            if (timeout) {
                auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime);
                
                if (elapsedTime >= *timeout) {
                    break; // 超时
                }
                
                remainingTime = std::chrono::milliseconds(*timeout - elapsedTime);
            }
        }
        
        return results;
    }
    
    // 等待一组任务中的任意一个响应
    Response* WaitForAnyTask(const std::vector<TaskId>& taskIds,
                           std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        if (taskIds.empty()) {
            return nullptr;
        }
        
        // 首先检查已收集的响应
        {
            std::lock_guard<std::mutex> lock(collectedMutex_);
            
            for (TaskId id : taskIds) {
                auto it = std::find_if(collectedResponses_.begin(), collectedResponses_.end(),
                    [id](Response* resp) { return resp->taskId == id; });
                
                if (it != collectedResponses_.end()) {
                    Response* result = *it;
                    collectedResponses_.erase(it);
                    return result;
                }
            }
        }
        
        // 决定等待时间
        auto startTime = std::chrono::steady_clock::now();
        auto remainingTime = timeout;
        
        // 等待任意一个匹配的响应
        while (true) {
            Response* response = nullptr;
            bool success = responseQueue_.Pop(response, remainingTime);
            
            if (!success) {
                // 超时或队列关闭
                return nullptr;
            }
            
            auto idIt = std::find(taskIds.begin(), taskIds.end(), response->taskId);
            if (idIt != taskIds.end()) {
                return response;
            } else {
                // 不是我们要等的任务，保存起来
                std::lock_guard<std::mutex> lock(collectedMutex_);
                collectedResponses_.push_back(response);
            }
            
            // 更新剩余时间
            if (timeout) {
                auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime);
                
                if (elapsedTime >= *timeout) {
                    return nullptr; // 超时
                }
                
                remainingTime = std::chrono::milliseconds(*timeout - elapsedTime);
            }
        }
    }
    
    // 清理收集的响应
    void Cleanup() {
        std::lock_guard<std::mutex> lock(collectedMutex_);
        for (auto* resp : collectedResponses_) {
            delete resp;
        }
        collectedResponses_.clear();
    }

private:
    ThreadSafeQueue<Response*>& responseQueue_;
    std::vector<Response*> collectedResponses_;
    std::mutex collectedMutex_;
};

//=============================================================================
// 线程池实现
//=============================================================================

class ThreadPool {
public:
    ThreadPool() = default;
    
    ~ThreadPool() {
        Shutdown();
    }
    
    // 初始化线程池
    void Initialize(uint32_t threadCount, std::function<void(Message*)> messageCallback) {
        if (initialized_) return;
        
        messageCallback_ = std::move(messageCallback);
        
        // 创建工作线程
        for (uint32_t i = 0; i < threadCount; ++i) {
            threads_.emplace_back([this] {
                WorkerThread();
            });
        }
        
        initialized_ = true;
    }
    
    // 添加具名线程
    void CreateNamedThread(const std::string& name, uint32_t index, 
                           std::function<void(Message*)> messageCallback) {
        std::string threadKey = BuildThreadKey(name, index);
        
        std::lock_guard<std::mutex> lock(namedThreadsMutex_);
        if (namedThreads_.find(threadKey) != namedThreads_.end()) {
            return; // 线程已存在
        }
        
        // 创建工作队列
        auto inQueue = std::make_shared<ThreadSafeQueue<Message*>>();
        
        // 创建并启动线程
        std::thread worker([this, inQueue, messageCallback] {
            NamedWorkerThread(inQueue, messageCallback);
        });
        
        // 保存线程信息
        NamedThreadInfo info;
        info.thread = std::move(worker);
        info.inQueue = inQueue;
        
        namedThreads_[threadKey] = std::move(info);
    }
    
    // 向共享队列提交任务
    void SubmitToSharedQueue(Message* message) {
        sharedTaskQueue_.Push(message);
    }
    
    // 向命名线程提交任务
    void SubmitToNamedThread(const std::string& name, uint32_t index, Message* message) {
        std::string threadKey = BuildThreadKey(name, index);
        
        std::lock_guard<std::mutex> lock(namedThreadsMutex_);
        auto it = namedThreads_.find(threadKey);
        if (it == namedThreads_.end()) {
            // 线程不存在，作为错误处理
            Response* resp = new Response();
            resp->taskId = message->taskId;
            resp->taskName = message->taskName;
            resp->status = ProcessStatus::ERROR;
            responseQueue_.Push(resp);
            delete message;
            return;
        }
        
        it->second.inQueue->Push(message);
    }
    
    // 将响应放入响应队列
    void QueueResponse(Response* response) {
        responseQueue_.Push(response);
    }
    
    // 获取响应等待器
    ResponseWaiter& GetResponseWaiter() {
        return *responseWaiter_;
    }
    
    // 关闭线程池
    void Shutdown() {
        if (!initialized_) return;
        
        sharedTaskQueue_.Shutdown();
        responseQueue_.Shutdown();
        
        // 关闭所有工作线程
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        // 关闭所有命名线程
        {
            std::lock_guard<std::mutex> lock(namedThreadsMutex_);
            for (auto& [key, info] : namedThreads_) {
                info.inQueue->Shutdown();
                if (info.thread.joinable()) {
                    info.thread.join();
                }
            }
            namedThreads_.clear();
        }
        
        // 清理响应等待器
        responseWaiter_->Cleanup();
        
        initialized_ = false;
    }

private:
    // 工作线程函数
    void WorkerThread() {
        Message* msg = nullptr;
        while (sharedTaskQueue_.Pop(msg)) {
            if (msg) {
                messageCallback_(msg);
            }
        }
    }
    
    // 命名工作线程函数
    void NamedWorkerThread(std::shared_ptr<ThreadSafeQueue<Message*>> inQueue, 
                           std::function<void(Message*)> callback) {
        Message* msg = nullptr;
        while (inQueue->Pop(msg)) {
            if (msg) {
                callback(msg);
            }
        }
    }
    
    std::string BuildThreadKey(const std::string& name, uint32_t index) {
        return name + "_" + std::to_string(index);
    }

private:
    struct NamedThreadInfo {
        std::thread thread;
        std::shared_ptr<ThreadSafeQueue<Message*>> inQueue;
    };

    bool initialized_{false};
    std::vector<std::thread> threads_;
    ThreadSafeQueue<Message*> sharedTaskQueue_;
    ThreadSafeQueue<Response*> responseQueue_;
    std::unique_ptr<ResponseWaiter> responseWaiter_{std::make_unique<ResponseWaiter>(responseQueue_)};
    
    std::mutex namedThreadsMutex_;
    std::unordered_map<std::string, NamedThreadInfo> namedThreads_;
    
    std::function<void(Message*)> messageCallback_;
};

//=============================================================================
// ThreadPoolExecutor 实现
//=============================================================================

class ThreadPoolExecutor {
public:
    ThreadPoolExecutor() = default;
    
    ~ThreadPoolExecutor() {
        threadPool_.Shutdown();
    }
    
    void Initialize(uint32_t threadCount) {
        threadPool_.Initialize(threadCount, [this](Message* msg) {
            this->ProcessMessage(msg);
        });
    }
    
    TaskId SubmitTask(
        const std::string& taskName,
        std::function<ProcessStatus(ProcessContext&)> task,
        ProcessContext& ctx) {
        
        TaskId taskId = GenerateTaskId();
        
        // 创建消息
        Message* msg = new Message();
        msg->taskId = taskId;
        msg->taskName = taskName;
        msg->taskFunction = task;
        msg->context = &ctx;
        
        // 提交到线程池
        threadPool_.SubmitToSharedQueue(msg);
        
        return taskId;
    }
    
    void CreateNamedThread(const std::string& name, uint32_t index) {
        threadPool_.CreateNamedThread(name, index, [this](Message* msg) {
            this->ProcessMessage(msg);
        });
    }
    
    TaskId SubmitTaskToNamedThread(
        const std::string& threadName,
        uint32_t threadIndex,
        const std::string& taskName,
        std::function<ProcessStatus(ProcessContext&)> task,
        ProcessContext& ctx) {
        
        TaskId taskId = GenerateTaskId();
        
        // 创建消息
        Message* msg = new Message();
        msg->taskId = taskId;
        msg->taskName = taskName;
        msg->taskFunction = task;
        msg->context = &ctx;
        
        // 提交到命名线程
        threadPool_.SubmitToNamedThread(threadName, threadIndex, msg);
        
        return taskId;
    }
    
    // 封装AsyncResult的响应结构
    struct AsyncResult {
        AsyncResult(const std::string& name, ProcessStatus status, TaskId id = 0)
        : name(name), status(status), taskId(id) {}

        std::string name;
        ProcessStatus status;
        TaskId taskId;
    };
    
    // 无限期等待特定任务
    AsyncResult WaitForTask(TaskId taskId) {
        Response* resp = threadPool_.GetResponseWaiter().WaitForTask(taskId);
        if (!resp) {
            return AsyncResult("unknown", ProcessStatus::ERROR, taskId);
        }
        
        AsyncResult result(resp->taskName, resp->status, resp->taskId);
        delete resp;
        return result;
    }
    
    // 带超时的等待特定任务
    AsyncResult WaitForTaskWithTimeout(TaskId taskId, std::chrono::milliseconds timeout) {
        Response* resp = threadPool_.GetResponseWaiter().WaitForTaskWithTimeout(taskId, timeout);
        if (!resp) {
            return AsyncResult("unknown", ProcessStatus::ERROR, taskId);
        }
        
        AsyncResult result(resp->taskName, resp->status, resp->taskId);
        delete resp;
        return result;
    }
    
    // 等待一组任务全部完成
    std::vector<AsyncResult> WaitForAllTasks(const std::vector<TaskId>& taskIds) {
        std::vector<Response*> responses = threadPool_.GetResponseWaiter().WaitForAllTasks(taskIds);
        
        std::vector<AsyncResult> results;
        results.reserve(responses.size());
        
        for (Response* resp : responses) {
            if (resp) {
                results.emplace_back(resp->taskName, resp->status, resp->taskId);
                delete resp;
            }
        }
        
        return results;
    }
    
    // 带超时的等待一组任务全部完成
    std::vector<AsyncResult> WaitForAllTasksWithTimeout(
        const std::vector<TaskId>& taskIds, 
        std::chrono::milliseconds timeout) {
            
        std::vector<Response*> responses = 
            threadPool_.GetResponseWaiter().WaitForAllTasks(taskIds, timeout);
        
        std::vector<AsyncResult> results;
        results.reserve(responses.size());
        
        for (Response* resp : responses) {
            if (resp) {
                results.emplace_back(resp->taskName, resp->status, resp->taskId);
                delete resp;
            }
        }
        
        return results;
    }
    
    // 等待任意一个任务完成
    AsyncResult WaitForAnyTask(const std::vector<TaskId>& taskIds) {
        Response* resp = threadPool_.GetResponseWaiter().WaitForAnyTask(taskIds);
        if (!resp) {
            return AsyncResult("unknown", ProcessStatus::ERROR, 0);
        }
        
        AsyncResult result(resp->taskName, resp->status, resp->taskId);
        delete resp;
        return result;
    }
    
    // 带超时的等待任意一个任务完成
    AsyncResult WaitForAnyTaskWithTimeout(
        const std::vector<TaskId>& taskIds, 
        std::chrono::milliseconds timeout) {
            
        Response* resp = threadPool_.GetResponseWaiter().WaitForAnyTask(taskIds, timeout);
        if (!resp) {
            return AsyncResult("timeout", ProcessStatus::ERROR, 0);
        }
        
        AsyncResult result(resp->taskName, resp->status, resp->taskId);
        delete resp;
        return result;
    }

private:
    void ProcessMessage(Message* msg) {
        ProcessStatus status = ProcessStatus::ERROR;
        
        try {
            status = msg->taskFunction(*msg->context);
        } catch (const std::exception& e) {
            std::cerr << "Exception in task " << msg->taskName 
                      << ": " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in task " << msg->taskName << std::endl;
        }
        
        // 创建响应
        Response* resp = new Response();
        resp->taskId = msg->taskId;
        resp->taskName = msg->taskName;
        resp->status = status;
        
        // 将响应放入响应队列
        threadPool_.QueueResponse(resp);
        
        delete msg;
    }
    
    static TaskId GenerateTaskId() {
        static std::atomic<TaskId> nextId{1};
        return nextId.fetch_add(1, std::memory_order_relaxed);
    }

private:
    ThreadPool threadPool_;
};

//=============================================================================
// 示例用法
//=============================================================================

// 模拟ProcessContext类
class ProcessContext {
public:
    // 模拟上下文实现
    bool IsStopped() const { return stopped_; }
    void Stop() { stopped_ = true; }
    void TryStop() { 
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stopped_) {
            stopped_ = true;
        }
    }

private:
    bool stopped_{false};
    std::mutex mutex_;
};

// 演示使用示例
void DemoThreadPoolUsage() {
    ThreadPoolExecutor executor;
    executor.Initialize(4); // 初始化4个工作线程
    
    // 创建测试上下文
    ProcessContext ctx;
    
    // 创建具名线程
    executor.CreateNamedThread("DemoThread", 0);
    executor.CreateNamedThread("DemoThread", 1);
    
    std::cout << "=== 基本任务提交和等待 ===" << std::endl;
    
    // 提交任务到共享线程池
    TaskId task1 = executor.SubmitTask("SharedTask1", [](ProcessContext& ctx) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "SharedTask1 executed\n";
        return ProcessStatus::OK;
    }, ctx);
    
    // 等待单个任务
    auto result1 = executor.WaitForTask(task1);
    std::cout << "Task " << result1.name << " completed with status: " 
              << (result1.status == ProcessStatus::OK ? "OK" : "ERROR") << std::endl;
    
    std::cout << "\n=== 具名线程任务提交和等待 ===" << std::endl;
    
    // 提交任务到具名线程
    TaskId task2 = executor.SubmitTaskToNamedThread("DemoThread", 0, "NamedTask1", [](ProcessContext& ctx) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "NamedTask1 executed on DemoThread_0\n";
        return ProcessStatus::OK;
    }, ctx);
    
    TaskId task3 = executor.SubmitTaskToNamedThread("DemoThread", 1, "NamedTask2", [](ProcessContext& ctx) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        std::cout << "NamedTask2 executed on DemoThread_1\n";
        return ProcessStatus::OK;
    }, ctx);
    
    // 等待所有任务
    std::vector<TaskId> allTasks = {task2, task3};
    auto results = executor.WaitForAllTasks(allTasks);
    
    for (const auto& result : results) {
        std::cout << "Task " << result.name << " completed with status: " 
                  << (result.status == ProcessStatus::OK ? "OK" : "ERROR") << std::endl;
    }
    
    std::cout << "\n=== 竞争任务和超时等待 ===" << std::endl;
    
    // 提交一些竞争任务
    TaskId race1 = executor.SubmitTask("RaceTask1", [](ProcessContext& ctx) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::cout << "RaceTask1 finished first\n";
        return ProcessStatus::OK;
    }, ctx);
    
    TaskId race2 = executor.SubmitTask("RaceTask2", [](ProcessContext& ctx) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "RaceTask2 finished\n";
        return ProcessStatus::OK;
    }, ctx);
    
    // 等待任何一个竞争任务完成
    std::vector<TaskId> raceTasks = {race1, race2};
    auto raceResult = executor.WaitForAnyTask(raceTasks);
    
    std::cout << "Race winner: " << raceResult.name << " with status: " 
              << (raceResult.status == ProcessStatus::OK ? "OK" : "ERROR") << std::endl;
    
    // 等待所有竞争任务完成（清理）
    executor.WaitForAllTasks(raceTasks);
    
    std::cout << "\n=== 超时等待演示 ===" << std::endl;
    
    // 测试超时
    TaskId slowTask = executor.SubmitTask("SlowTask", [](ProcessContext& ctx) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "SlowTask completed\n";
        return ProcessStatus::OK;
    }, ctx);
    
    auto timeoutResult = executor.WaitForTaskWithTimeout(slowTask, std::chrono::milliseconds(100));
    if (timeoutResult.status == ProcessStatus::ERROR) {
        std::cout << "Timeout waiting for SlowTask (as expected)\n";
    }
    
    // 等待慢任务完成（清理）
    executor.WaitForTask(slowTask);
}

int main() {
    DemoThreadPoolUsage();
    return 0;
}