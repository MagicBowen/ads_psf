#include "ads_psf/processor.h"
#include "ads_psf/process_status.h"
#include "ads_psf/process_context.h"
#include "ads_psf/processor_info.h"

namespace ads_psf {

ProcessStatus Processor::Process(ProcessContext& ctx) {
    ProcessorInfo info{name_, id_};

    ctx.EnterProcess(info);

    if (ctx.IsStopped()) {
        ctx.ExitProcess(info, ProcessStatus::CANCELLED);
        return ProcessStatus::CANCELLED;
    }
    auto status = Execute(ctx);
    ctx.ExitProcess(info, status);
    return status;
}

void Processor::Init(const ProcessorInfo& parentInfo, uint32_t childIndex, AsyncExecutor& executor) {
    if (parentInfo.id == ProcessorId::Root() && childIndex == 0) {
        id_ = ProcessorId::Root();
    } else {
        id_ = ProcessorId::CreateChild(parentInfo.id, childIndex);
    }
    name_ = parentInfo.name + "/" + name_;
    executor_ = &executor;
}

} // namespace ads_psf