#include  "ads_psf/data_context.h"
#include  "ads_psf/process_context.h"
#include  "ads_psf/process_status.h"

#include <condition_variable>
#include <atomic>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <list>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <algorithm>

using namespace ads_psf;

// 任务ID类型
using TaskId = uint64_t;

// 请求结构
struct ProcessReq {
    TaskId taskId;
    std::string taskName;
    std::function<ProcessStatus(ProcessContext&)> taskFunction;
    ProcessContext* context;
};

// 响应结构
struct ProcessRsp {
    TaskId taskId;
    std::string taskName;
    ProcessStatus status;
};

//=============================================================================
// 改进的线程安全队列实现 - 使用std::list
//=============================================================================

template<typename T>
class ThreadSafeQueue {
public:
    void Push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(item));
        condition_.notify_all(); // 通知所有等待中的Take和Pop操作
    }

    // 无超时的Pop操作
    bool Pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        
        if (shutdown_ && queue_.empty())
            return false;
            
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    // 带超时的Pop操作
    bool Pop(T& item, const std::chrono::milliseconds& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        bool hasItem = condition_.wait_for(lock, timeout, 
            [this] { return !queue_.empty() || shutdown_; });
        
        if (!hasItem || (shutdown_ && queue_.empty()))
            return false;
            
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    // 尝试不阻塞地Pop
    bool TryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;
            
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    // 无超时的Take操作（根据谓词筛选）
    template<typename Predicate>
    bool Take(Predicate pred, T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 持续等待直到找到满足条件的元素或队列关闭
        while (true) {
            // 在当前队列中查找
            auto it = std::find_if(queue_.begin(), queue_.end(), pred);
            
            // 如果找到了匹配的元素
            if (it != queue_.end()) {
                item = std::move(*it);
                queue_.erase(it);
                return true;
            }
            
            // 如果没有找到并且队列已关闭
            if (shutdown_) {
                return false;
            }
            
            // 等待新元素
            condition_.wait(lock);
        }
    }

    // 带超时的Take操作
    template<typename Predicate>
    bool Take(Predicate pred, T& item, const std::chrono::milliseconds& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto endTime = std::chrono::steady_clock::now() + timeout;
        
        // 持续尝试查找直到超时
        while (std::chrono::steady_clock::now() < endTime) {
            // 在当前队列中查找
            auto it = std::find_if(queue_.begin(), queue_.end(), pred);
            
            // 如果找到了匹配的元素
            if (it != queue_.end()) {
                item = std::move(*it);
                queue_.erase(it);
                return true;
            }
            
            // 如果没有找到并且队列已关闭
            if (shutdown_) {
                return false;
            }
            
            // 计算剩余等待时间
            auto remainingTime = endTime - std::chrono::steady_clock::now();
            if (remainingTime <= std::chrono::milliseconds::zero()) {
                return false; // 超时
            }
            
            // 等待新元素，但最多等待剩余时间
            condition_.wait_for(lock, remainingTime);
        }
        
        return false; // 超时
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
    std::list<T> queue_; // 使用list来支持高效的中间元素删除
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool shutdown_{false};
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
    void Initialize(uint32_t threadCount, std::function<void(ProcessReq*)> requestCallback) {
        if (initialized_) return;
        
        requestCallback_ = std::move(requestCallback);
        
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
                           std::function<void(ProcessReq*)> requestCallback) {
        std::string threadKey = BuildThreadKey(name, index);
        
        std::lock_guard<std::mutex> lock(namedThreadsMutex_);
        if (namedThreads_.find(threadKey) != namedThreads_.end()) {
            return; // 线程已存在
        }
        
        // 创建工作队列
        auto inQueue = std::make_shared<ThreadSafeQueue<ProcessReq*>>();
        
        // 创建并启动线程
        std::thread worker([this, inQueue, requestCallback] {
            NamedWorkerThread(inQueue, requestCallback);
        });
        
        // 保存线程信息
        NamedThreadInfo info;
        info.thread = std::move(worker);
        info.inQueue = inQueue;
        
        namedThreads_[threadKey] = std::move(info);
    }
    
    // 向共享队列提交任务
    void SubmitToSharedQueue(ProcessReq* request) {
        sharedTaskQueue_.Push(request);
    }
    
    // 向命名线程提交任务
    void SubmitToNamedThread(const std::string& name, uint32_t index, ProcessReq* request) {
        std::string threadKey = BuildThreadKey(name, index);
        
        std::lock_guard<std::mutex> lock(namedThreadsMutex_);
        auto it = namedThreads_.find(threadKey);
        if (it == namedThreads_.end()) {
            // 线程不存在，作为错误处理
            ProcessRsp* resp = new ProcessRsp();
            resp->taskId = request->taskId;
            resp->taskName = request->taskName;
            resp->status = ProcessStatus::ERROR;
            responseQueue_.Push(resp);
            delete request;
            return;
        }
        
        it->second.inQueue->Push(request);
    }
    
    // 将响应放入响应队列
    void QueueResponse(ProcessRsp* response) {
        responseQueue_.Push(response);
    }
    
    // 等待特定任务ID的响应（无超时）
    ProcessRsp* WaitForResponse(TaskId taskId) {
        ProcessRsp* resp = nullptr;
        bool success = responseQueue_.Take(
            [taskId](const ProcessRsp* r) { return r->taskId == taskId; }, 
            resp);
            
        return success ? resp : nullptr;
    }
    
    // 等待特定任务ID的响应（带超时）
    ProcessRsp* WaitForResponse(TaskId taskId, const std::chrono::milliseconds& timeout) {
        ProcessRsp* resp = nullptr;
        bool success = responseQueue_.Take(
            [taskId](const ProcessRsp* r) { return r->taskId == taskId; }, 
            resp, timeout);
            
        return success ? resp : nullptr;
    }
    
    // 等待一组任务中任意一个的响应（无超时）
    ProcessRsp* WaitForAnyResponse(const std::vector<TaskId>& taskIds) {
        ProcessRsp* resp = nullptr;
        bool success = responseQueue_.Take(
            [&taskIds](const ProcessRsp* r) { 
                return std::find(taskIds.begin(), taskIds.end(), r->taskId) != taskIds.end(); 
            }, 
            resp);
            
        return success ? resp : nullptr;
    }
    
    // 等待一组任务中任意一个的响应（带超时）
    ProcessRsp* WaitForAnyResponse(const std::vector<TaskId>& taskIds, 
                               const std::chrono::milliseconds& timeout) {
        ProcessRsp* resp = nullptr;
        bool success = responseQueue_.Take(
            [&taskIds](const ProcessRsp* r) { 
                return std::find(taskIds.begin(), taskIds.end(), r->taskId) != taskIds.end(); 
            }, 
            resp, timeout);
            
        return success ? resp : nullptr;
    }
    
    // 等待一组任务全部完成（无超时）
    std::vector<ProcessRsp*> WaitForAllResponses(const std::vector<TaskId>& taskIds) {
        std::vector<ProcessRsp*> results;
        std::vector<TaskId> remainingIds(taskIds);
        
        while (!remainingIds.empty()) {
            ProcessRsp* resp = nullptr;
            bool success = responseQueue_.Take(
                [&remainingIds](const ProcessRsp* r) { 
                    return std::find(remainingIds.begin(), remainingIds.end(), r->taskId) != remainingIds.end(); 
                }, 
                resp);
                
            if (!success) {
                break; // 队列关闭或其他错误
            }
            
            // 添加结果并从待等待列表中移除
            results.push_back(resp);
            remainingIds.erase(std::find(remainingIds.begin(), remainingIds.end(), resp->taskId));
        }
        
        return results;
    }
    
    // 等待一组任务全部完成（带超时）
    std::vector<ProcessRsp*> WaitForAllResponses(const std::vector<TaskId>& taskIds, 
                                           const std::chrono::milliseconds& timeout) {
        std::vector<ProcessRsp*> results;
        std::vector<TaskId> remainingIds(taskIds);
        
        auto endTime = std::chrono::steady_clock::now() + timeout;
        
        while (!remainingIds.empty()) {
            auto remainingTime = endTime - std::chrono::steady_clock::now();
            if (remainingTime <= std::chrono::milliseconds::zero()) {
                break; // 超时
            }
            
            ProcessRsp* resp = nullptr;
            bool success = responseQueue_.Take(
                [&remainingIds](const ProcessRsp* r) { 
                    return std::find(remainingIds.begin(), remainingIds.end(), r->taskId) != remainingIds.end(); 
                }, 
                resp, std::chrono::duration_cast<std::chrono::milliseconds>(remainingTime));
                
            if (!success) {
                break; // 超时或队列关闭
            }
            
            // 添加结果并从待等待列表中移除
            results.push_back(resp);
            remainingIds.erase(std::find(remainingIds.begin(), remainingIds.end(), resp->taskId));
        }
        
        return results;
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
            for (auto& pair : namedThreads_) {
                pair.second.inQueue->Shutdown();
                if (pair.second.thread.joinable()) {
                    pair.second.thread.join();
                }
            }
            namedThreads_.clear();
        }
        
        initialized_ = false;
    }

private:
    // 工作线程函数
    void WorkerThread() {
        ProcessReq* req = nullptr;
        while (sharedTaskQueue_.Pop(req)) {
            if (req) {
                requestCallback_(req);
            }
        }
    }
    
    // 命名工作线程函数
    void NamedWorkerThread(std::shared_ptr<ThreadSafeQueue<ProcessReq*>> inQueue, 
                           std::function<void(ProcessReq*)> callback) {
        ProcessReq* req = nullptr;
        while (inQueue->Pop(req)) {
            if (req) {
                callback(req);
            }
        }
    }
    
    std::string BuildThreadKey(const std::string& name, uint32_t index) {
        return name + "_" + std::to_string(index);
    }

private:
    struct NamedThreadInfo {
        std::thread thread;
        std::shared_ptr<ThreadSafeQueue<ProcessReq*>> inQueue;
    };

    bool initialized_{false};
    std::vector<std::thread> threads_;
    ThreadSafeQueue<ProcessReq*> sharedTaskQueue_;
    ThreadSafeQueue<ProcessRsp*> responseQueue_;
    
    std::mutex namedThreadsMutex_;
    std::unordered_map<std::string, NamedThreadInfo> namedThreads_;
    
    std::function<void(ProcessReq*)> requestCallback_;
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
        threadPool_.Initialize(threadCount, [this](ProcessReq* req) {
            this->ProcessRequest(req);
        });
    }
    
    TaskId SubmitTask(
        const std::string& taskName,
        std::function<ProcessStatus(ProcessContext&)> task,
        ProcessContext& ctx) {
        
        TaskId taskId = GenerateTaskId();
        
        // 创建请求
        ProcessReq* req = new ProcessReq();
        req->taskId = taskId;
        req->taskName = taskName;
        req->taskFunction = task;
        req->context = &ctx;
        
        // 提交到线程池
        threadPool_.SubmitToSharedQueue(req);
        
        return taskId;
    }
    
    void CreateNamedThread(const std::string& name, uint32_t index) {
        threadPool_.CreateNamedThread(name, index, [this](ProcessReq* req) {
            this->ProcessRequest(req);
        });
    }
    
    TaskId SubmitTaskToNamedThread(
        const std::string& threadName,
        uint32_t threadIndex,
        const std::string& taskName,
        std::function<ProcessStatus(ProcessContext&)> task,
        ProcessContext& ctx) {
        
        TaskId taskId = GenerateTaskId();
        
        // 创建请求
        ProcessReq* req = new ProcessReq();
        req->taskId = taskId;
        req->taskName = taskName;
        req->taskFunction = task;
        req->context = &ctx;
        
        // 提交到命名线程
        threadPool_.SubmitToNamedThread(threadName, threadIndex, req);
        
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
        ProcessRsp* resp = threadPool_.WaitForResponse(taskId);
        if (!resp) {
            return AsyncResult("unknown", ProcessStatus::ERROR, taskId);
        }
        
        AsyncResult result(resp->taskName, resp->status, resp->taskId);
        delete resp;
        return result;
    }
    
    // 带超时的等待特定任务
    AsyncResult WaitForTaskWithTimeout(TaskId taskId, const std::chrono::milliseconds& timeout) {
        ProcessRsp* resp = threadPool_.WaitForResponse(taskId, timeout);
        if (!resp) {
            return AsyncResult("timeout", ProcessStatus::ERROR, taskId);
        }
        
        AsyncResult result(resp->taskName, resp->status, resp->taskId);
        delete resp;
        return result;
    }
    
    // 等待一组任务全部完成
    std::vector<AsyncResult> WaitForAllTasks(const std::vector<TaskId>& taskIds) {
        std::vector<ProcessRsp*> responses = threadPool_.WaitForAllResponses(taskIds);
        
        std::vector<AsyncResult> results;
        results.reserve(responses.size());
        
        for (ProcessRsp* resp : responses) {
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
        const std::chrono::milliseconds& timeout) {
            
        std::vector<ProcessRsp*> responses = 
            threadPool_.WaitForAllResponses(taskIds, timeout);
        
        std::vector<AsyncResult> results;
        results.reserve(responses.size());
        
        for (ProcessRsp* resp : responses) {
            if (resp) {
                results.emplace_back(resp->taskName, resp->status, resp->taskId);
                delete resp;
            }
        }
        
        return results;
    }
    
    // 等待任意一个任务完成
    AsyncResult WaitForAnyTask(const std::vector<TaskId>& taskIds) {
        ProcessRsp* resp = threadPool_.WaitForAnyResponse(taskIds);
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
        const std::chrono::milliseconds& timeout) {
            
        ProcessRsp* resp = threadPool_.WaitForAnyResponse(taskIds, timeout);
        if (!resp) {
            return AsyncResult("timeout", ProcessStatus::ERROR, 0);
        }
        
        AsyncResult result(resp->taskName, resp->status, resp->taskId);
        delete resp;
        return result;
    }

private:
    void ProcessRequest(ProcessReq* req) {
        ProcessStatus status = ProcessStatus::ERROR;
        
        try {
            status = req->taskFunction(*req->context);
        } catch (const std::exception& e) {
            std::cerr << "Exception in task " << req->taskName 
                      << ": " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in task " << req->taskName << std::endl;
        }
        
        // 创建响应
        ProcessRsp* resp = new ProcessRsp();
        resp->taskId = req->taskId;
        resp->taskName = req->taskName;
        resp->status = status;
        
        // 将响应放入响应队列
        threadPool_.QueueResponse(resp);
        
        delete req;
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

// 演示使用示例
void DemoThreadPoolUsage() {
    ThreadPoolExecutor executor;
    executor.Initialize(4); // 初始化4个工作线程
    
    // 创建测试上下文
    DataContext dctx;
    ProcessContext ctx{dctx};
    
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
    std::vector<TaskId> leftTasks = {race2};
    executor.WaitForAllTasks(leftTasks);
    
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