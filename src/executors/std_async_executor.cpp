#include "ads_psf/executors/std_async_executor.h"
#include <condition_variable>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <list>

namespace ads_psf {

template<typename T>
struct SyncQueue {
    void Push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(item));
        condition_.notify_all();
    }

    bool Pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        
        if (shutdown_ && queue_.empty())
            return false;
            
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    bool Pop(T& item, const TimeMs& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        bool hasItem = condition_.wait_for(lock, timeout, 
            [this] { return !queue_.empty() || shutdown_; });
        
        if (!hasItem || (shutdown_ && queue_.empty()))
            return false;
            
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    bool TryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;
            
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    template<typename PRED>
    bool Take(PRED pred, T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (true) {
            auto it = std::find_if(queue_.begin(), queue_.end(), pred);
            if (it != queue_.end()) {
                item = std::move(*it);
                queue_.erase(it);
                return true;
            }
            
            if (shutdown_) {
                return false;
            }
            
            condition_.wait(lock);
        }
    }

    template<typename Predicate>
    bool Take(Predicate pred, T& item, const TimeMs& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        auto endTime = std::chrono::steady_clock::now() + timeout;
        
        while (std::chrono::steady_clock::now() < endTime) {
            auto it = std::find_if(queue_.begin(), queue_.end(), pred);
            if (it != queue_.end()) {
                item = std::move(*it);
                queue_.erase(it);
                return true;
            }
            
            if (shutdown_) {
                return false;
            }
            
            auto remainingTime = endTime - std::chrono::steady_clock::now();
            if (remainingTime <= TimeMs::zero()) {
                return false;
            }
            
            condition_.wait_for(lock, remainingTime);
        }
        return false;
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
    std::list<T> queue_; 
    bool shutdown_{false};
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

struct StdAsyncExecutor::ThreadPool {
    using ThreadName = std::string;

    struct ProcessRequest {
        static ProcessRequest CreateDefault() {
            return ProcessRequest{ProcessTaskId{0}, [] { return ProcessStatus::ERROR; }};
        }
    
        ProcessRequest(const ProcessTaskId& id, ProcessTask task)
        : taskId(id), task(std::move(task)) {}
    
        ProcessTaskId taskId;
        ProcessTask task;
    };

    using ProcessReqHandler = std::function<void(ProcessRequest&)>;
    
    ~ThreadPool() {
        Shutdown();
    }
    
    void Initialize(uint32_t threadCount, ProcessReqHandler reqHandler) {
        if (initialized_) return;
        
        reqHandler_ = std::move(reqHandler);
        
        for (uint32_t i = 0; i < threadCount; ++i) {
            threads_.emplace_back([this] {
                WorkerThread();
            });
        }
        
        initialized_ = true;
    }
    
    void CreateNamedThread(const ThreadName& name, ProcessReqHandler reqHandler) {
        
        std::lock_guard<std::mutex> lock(namedThreadsMutex_);
        if (namedThreads_.find(name) != namedThreads_.end()) {
            return;
        }
        
        auto inQueue = std::make_shared<SyncQueue<ProcessRequest>>();
        
        std::thread worker([this, inQueue, reqHandler = std::move(reqHandler)] {
            NamedWorkerThread(inQueue, std::move(reqHandler));
        });
        
        NamedThreadInfo info;
        info.thread = std::move(worker);
        info.inQueue = inQueue;
        
        namedThreads_[name] = std::move(info);
    }
    
    void Submit(ProcessRequest req) {
        sharedTaskQueue_.Push(std::move(req));
    }
    
    void Submit(const ThreadName& name, ProcessRequest req) {
        std::lock_guard<std::mutex> lock(namedThreadsMutex_);
        auto it = namedThreads_.find(name);
        if (it == namedThreads_.end()) {
            responseQueue_.Push(ProcessResult{req.taskId, ProcessStatus::ERROR});
            return;
        }
        
        it->second.inQueue->Push(std::move(req));
    }

    void QueueResponse(ProcessResult rsp) {
        responseQueue_.Push(std::move(rsp));
    }
    
    ProcessResult WaitFor(ProcessTaskId taskId) {
        ProcessResult rsp{taskId, ProcessStatus::ERROR};

        bool success = responseQueue_.Take(
            [taskId](const ProcessResult& r) { return r.id == taskId; }, 
            rsp);
            
        return success ? rsp : ProcessResult{taskId, ProcessStatus::ERROR};
    }
    
    ProcessResult WaitFor(ProcessTaskId taskId, const TimeMs& timeout) {
        ProcessResult rsp{taskId, ProcessStatus::ERROR};

        bool success = responseQueue_.Take(
            [taskId](const ProcessResult& r) { return r.id == taskId; }, 
            rsp, timeout);
            
        return success ? rsp : ProcessResult{taskId, ProcessStatus::ERROR};
    }
    
    ProcessResult WaitForAny(const ProcessTaskIds& taskIds) {
        ProcessResult rsp{ProcessTaskId{0}, ProcessStatus::ERROR};

        bool success = responseQueue_.Take(
            [&taskIds](const ProcessResult& r) { 
                return std::find(taskIds.begin(), taskIds.end(), r.id) != taskIds.end(); 
            }, 
            rsp);
            
        return success ? rsp : ProcessResult{rsp.id, ProcessStatus::ERROR};
    }
    
    ProcessResult WaitForAny(const ProcessTaskIds& taskIds, const TimeMs& timeout) {
        ProcessResult rsp{ProcessTaskId{0}, ProcessStatus::ERROR};

        bool success = responseQueue_.Take(
            [&taskIds](const ProcessResult& r) { 
                return std::find(taskIds.begin(), taskIds.end(), r.id) != taskIds.end(); 
            }, 
            rsp, timeout);
            
        return success ? rsp : ProcessResult{rsp.id, ProcessStatus::ERROR};
    }
    
    ProcessResults WaitForAll(const ProcessTaskIds& taskIds) {
        ProcessResults results;
        ProcessTaskIds remainingIds(taskIds);
        
        while (!remainingIds.empty()) {
            ProcessResult rsp{ProcessTaskId{0}, ProcessStatus::ERROR};

            bool success = responseQueue_.Take(
                [&remainingIds](const ProcessResult& r) { 
                    return std::find(remainingIds.begin(), remainingIds.end(), r.id) != remainingIds.end(); 
                }, 
                rsp);
                
            if (!success) {
                break;
            }
            
            remainingIds.erase(std::find(remainingIds.begin(), remainingIds.end(), rsp.id));
            results.emplace_back(std::move(rsp));
        }
        
        return results;
    }
    
    ProcessResults WaitForAll(const ProcessTaskIds& taskIds, const TimeMs& timeout) {
        ProcessResults results;
        ProcessTaskIds remainingIds(taskIds);
        
        auto endTime = std::chrono::steady_clock::now() + timeout;
        
        while (!remainingIds.empty()) {
            auto remainingTime = endTime - std::chrono::steady_clock::now();
            if (remainingTime <= TimeMs::zero()) {
                break;
            }
            
            ProcessResult rsp{ProcessTaskId{0}, ProcessStatus::ERROR};
            bool success = responseQueue_.Take(
                [&remainingIds](const ProcessResult& r) { 
                    return std::find(remainingIds.begin(), remainingIds.end(), r.id) != remainingIds.end(); 
                }, 
                rsp, std::chrono::duration_cast<TimeMs>(remainingTime));
                
            if (!success) {
                break;
            }
            
            remainingIds.erase(std::find(remainingIds.begin(), remainingIds.end(), rsp.id));
            results.emplace_back(std::move(rsp));
        }
        
        return results;
    }
    
    void Shutdown() {
        if (!initialized_) return;
        
        sharedTaskQueue_.Shutdown();
        responseQueue_.Shutdown();
        
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
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
    void WorkerThread() {
        ProcessRequest req = ProcessRequest::CreateDefault();
        while (sharedTaskQueue_.Pop(req)) {
            reqHandler_(req);
        }
    }
    
    void NamedWorkerThread(std::shared_ptr<SyncQueue<ProcessRequest>> inQueue, 
                           ProcessReqHandler reqHandler) {
        ProcessRequest req = ProcessRequest::CreateDefault();
        while (inQueue->Pop(req)) {
            reqHandler(req);
        }
    }

private:
    struct NamedThreadInfo {
        std::thread thread;
        std::shared_ptr<SyncQueue<ProcessRequest>> inQueue;
    };

    bool initialized_{false};
    std::vector<std::thread> threads_;
    SyncQueue<ProcessRequest> sharedTaskQueue_;
    SyncQueue<ProcessResult> responseQueue_;
    
    std::mutex namedThreadsMutex_;
    std::unordered_map<std::string, NamedThreadInfo> namedThreads_;
    
    ProcessReqHandler reqHandler_;
};

StdAsyncExecutor::StdAsyncExecutor(uint32_t threadCount)
: threadPool_(std::make_unique<ThreadPool>()) {
    if (threadCount == 0) return;
    threadPool_->Initialize(threadCount, [this](ThreadPool::ProcessRequest& req) {
        threadPool_->QueueResponse(ProcessResult{req.taskId, req.task()});
    });
}

StdAsyncExecutor::~StdAsyncExecutor() {
    threadPool_->Shutdown();
}

void StdAsyncExecutor::CreateDedicatedThread(const ProcessTaskId& id) {
    threadPool_->CreateNamedThread(id.ToString(), [this](ThreadPool::ProcessRequest& req) {
        threadPool_->QueueResponse(ProcessResult{req.taskId, req.task()});
    });
}

bool StdAsyncExecutor::Submit(const ProcessorId& id, ProcessTask task) {
    threadPool_->Submit(ThreadPool::ProcessRequest{id, std::move(task)});
    return true;
}

bool StdAsyncExecutor::SubmitDedicated(const ProcessorId& id, ProcessTask task) {
    threadPool_->Submit(id.ToString(), ThreadPool::ProcessRequest{id, std::move(task)});
    return true;
}

ProcessResult StdAsyncExecutor::WaitFor(const ProcessorId& id) {
    return threadPool_->WaitFor(id);
}

ProcessResult StdAsyncExecutor::WaitFor(const ProcessorId& id, const TimeMs& timeout) {
    return threadPool_->WaitFor(id, timeout);
}

ProcessResults StdAsyncExecutor::WaitForAll(const ProcessTaskIds& ids) {
    return threadPool_->WaitForAll(ids);
}

ProcessResults StdAsyncExecutor::WaitForAll(const ProcessTaskIds& ids, const TimeMs& timeout) {
    return threadPool_->WaitForAll(ids, timeout);
}

ProcessResult StdAsyncExecutor::WaitForAny(const ProcessTaskIds& ids) {
    return threadPool_->WaitForAny(ids);
}

ProcessResult StdAsyncExecutor::WaitForAny(const ProcessTaskIds& ids, const TimeMs& timeout) {
    return threadPool_->WaitForAny(ids, timeout);
}

}
