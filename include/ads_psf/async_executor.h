/**
* Copyright (c) wangbo@joycode.art 2024
*/

#ifndef ASYNC_EXECUTOR_H
#define ASYNC_EXECUTOR_H

#include "ads_psf/process_result.h"
#include <future>

namespace ads_psf {

enum class ProcessStatus;

using AsyncResult = std::future<ProcessResult>;
using AsyncTask = std::function<ProcessStatus()>;

struct AsyncExecutor {
    virtual AsyncResult Submit(const ProcessorId&, AsyncTask) = 0;
    virtual ~AsyncExecutor() = default;
};

}

#endif
