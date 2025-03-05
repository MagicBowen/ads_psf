/**
* Copyright (c) wangbo@joycode.art 2024
*/

#ifndef ASYNC_EXECUTOR_H
#define ASYNC_EXECUTOR_H

#include "ads_psf/process_result.h"
#include <future>

namespace ads_psf {

struct ProcessContext;

struct AsyncExecutor {
    virtual std::future<ProcessResult> Submit(const ProcessorId&, 
                                            std::function<ProcessStatus(ProcessContext&)> task, 
                                            ProcessContext&) = 0;
    virtual ~AsyncExecutor() = default;
};

}

#endif
