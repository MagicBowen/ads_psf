/**
* Copyright (c) wangbo@joycode.art 2024
*/

#ifndef STD_ASYNC_EXECUTOR_H
#define STD_ASYNC_EXECUTOR_H

#include "ads_psf/async_executor.h"

namespace ads_psf {

struct StdAsyncExecutor : AsyncExecutor {
private:
    AsyncResult Submit(const ProcessorId& id, AsyncTask task) override {
        return std::async(std::launch::async, [&id, task = std::move(task)]() {
            return ProcessResult{id, task()};
        });
    }
};

}

#endif
