#include "ads_psf/trackers/console_tracker.h"
#include "ads_psf/processor_info.h"
#include "ads_psf/process_status.h"
#include <iostream>

namespace ads_psf {

void ConsoleTracker::TrackEnter(const ProcessorInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (info.id == ProcessorId::Root()) {
        std::cout << "\n============== Schedule Start =============\n";
    } 
    std::cout << info << " enter..." << "\n";
}

void ConsoleTracker::TrackExit(const ProcessorInfo& info, ProcessStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << info << " exit with status: " << status << "!" << "\n";
}

} // namespace ads_psf