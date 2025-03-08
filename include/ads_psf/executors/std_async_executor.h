/**
* Copyright (c) wangbo@joycode.art 2024
*/

#ifndef STD_ASYNC_EXECUTOR_H
#define STD_ASYNC_EXECUTOR_H

#include "ads_psf/async_executor.h"
#include <memory>

namespace ads_psf {

struct StdAsyncExecutor : AsyncExecutor  {
    explicit StdAsyncExecutor(uint32_t threadCount = 0);
    ~StdAsyncExecutor();
    
private:
    void CreateDedicatedThread(const ProcessTaskId&) override;

    bool Submit(const ProcessTaskId&, ProcessTask) override;
    bool SubmitDedicated(const ProcessTaskId&, ProcessTask) override;

    ProcessResult WaitFor(const ProcessTaskId&) override;
    ProcessResult WaitFor(const ProcessTaskId&, const TimeMs&) override;

    ProcessResults WaitForAll(const ProcessTaskIds&) override;
    ProcessResults WaitForAll(const ProcessTaskIds&, const TimeMs&) override;

    ProcessResult WaitForAny(const ProcessTaskIds&) override;
    ProcessResult WaitForAny(const ProcessTaskIds&, const TimeMs&) override;

private:
    class ThreadPool;
    std::unique_ptr<ThreadPool> threadPool_;
};

}

#endif
