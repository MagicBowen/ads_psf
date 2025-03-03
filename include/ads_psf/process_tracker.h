#ifndef ADS_PSF_PROCESS_TRACKER_H
#define ADS_PSF_PROCESS_TRACKER_H

namespace ads_psf {

struct ProcessorInfo;
enum class ProcessStatus;

struct ProcessTracker {
    virtual void TrackEnter(const ProcessorInfo&) = 0;
    virtual void TrackExit(const ProcessorInfo&, ProcessStatus) = 0;
    virtual void Dump() const {}
    virtual ~ProcessTracker() = default;
};

} // namespace ads_psf

#endif // ADS_PSF_PROCESS_TRACKER_H