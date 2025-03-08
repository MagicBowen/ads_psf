#include "ads_psf/trackers/console_tracker.h"
#include "ads_psf/processor_info.h"
#include "ads_psf/process_status.h"
#include <iostream>

namespace ads_psf {

void ConsoleTracker::ScheduleEnter() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n============== Schedule Start =============\n";
}

void ConsoleTracker::ScheduleExit(ProcessStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "============== Schedule End with status: " << status << " =============\n";
}

void ConsoleTracker::ProcessEnter(const ProcessorInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << info << " enter..." << "\n";
}

void ConsoleTracker::ProcessExit(const ProcessorInfo& info, ProcessStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << info << " exit with status: " << status << "!" << "\n";
}

} // namespace ads_psf