/**
* Copyright (c) wangbo@joycode.art 2024
*/

#ifndef ASYNC_EXECUTOR_H
#define ASYNC_EXECUTOR_H

#include "ads_psf/process_result.h"
#include <functional>
#include <string>
#include <chrono>
#include <vector>

namespace ads_psf {

enum class ProcessStatus;
using ProcessTask = std::function<ProcessStatus()>;
using ProcessTaskId = ProcessorId;
using ProcessTaskIds = std::vector<ProcessTaskId>;
using ProcessResults = std::vector<ProcessResult>;
using TimeMs = std::chrono::milliseconds;

struct AsyncExecutor {
    virtual void CreateDedicatedThread(const ProcessTaskId&) = 0;

    virtual bool Submit(const ProcessTaskId&, ProcessTask) = 0;
    virtual bool SubmitDedicated(const ProcessTaskId&, ProcessTask) = 0;
    
    virtual ProcessResult WaitFor(const ProcessTaskId&) = 0;
    virtual ProcessResult WaitFor(const ProcessTaskId&, const TimeMs&) = 0;
    
    virtual ProcessResults WaitForAll(const ProcessTaskIds&) = 0;
    virtual ProcessResults WaitForAll(const ProcessTaskIds&, const TimeMs&) = 0;
    
    virtual ProcessResult WaitForAny(const ProcessTaskIds&) = 0;
    virtual ProcessResult WaitForAny(const ProcessTaskIds&, const TimeMs&) = 0;

    virtual ~AsyncExecutor() = default;
};

}

#endif
