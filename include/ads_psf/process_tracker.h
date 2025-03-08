#ifndef ADS_PSF_PROCESS_TRACKER_H
#define ADS_PSF_PROCESS_TRACKER_H

namespace ads_psf {

struct ProcessorInfo;
enum class ProcessStatus;

struct ProcessTracker {
    virtual void ScheduleEnter() = 0;
    virtual void ScheduleExit(ProcessStatus) = 0;
    virtual void ProcessEnter(const ProcessorInfo&) = 0;
    virtual void ProcessExit(const ProcessorInfo&, ProcessStatus) = 0;
    virtual ~ProcessTracker() = default;
};

} // namespace ads_psf

#endif // ADS_PSF_PROCESS_TRACKER_H