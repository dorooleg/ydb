#include "read_context.h"

#include <ydb/core/tx/columnshard/engines/reader/common/conveyor_task.h>

#include <ydb/core/base/appdata.h>
#include <ydb/core/resource_pools/resource_pool_settings.h>
#include <ydb/core/tx/columnshard/engines/reader/common_reader/constructor/resolver.h>
#include <ydb/core/tx/conveyor_composite/usage/service.h>

#include <ydb/library/actors/core/actor.h>

#include <algorithm>
#include <util/generic/utility.h>

namespace NKikimr::NOlap::NReader {

IDataReader::IDataReader(const std::shared_ptr<TReadContext>& context)
    : Context(context)
{
}

TReadContext::TReadContext(const std::shared_ptr<IStoragesManager>& storagesManager,
    const std::shared_ptr<NDataAccessorControl::IDataAccessorsManager>& dataAccessorsManager,
    const std::shared_ptr<NColumnFetching::TColumnDataManager>& columnDataManager, const NColumnShard::TConcreteScanCounters& counters,
    const TReadMetadataBase::TConstPtr& readMetadata, const TActorId& scanActorId, const TActorId& resourceSubscribeActorId,
    const TComputeShardingPolicy& computeShardingPolicy, const ui64 scanId, const NConveyorComposite::TCPULimitsConfig& cpuLimits,
    const std::shared_ptr<NLWTrace::TOrbit>& scanOrbit)
    : StoragesManager(storagesManager)
    , DataAccessorsManager(dataAccessorsManager)
    , ColumnDataManager(columnDataManager)
    , Counters(counters)
    , ReadMetadata(readMetadata)
    , ResourcesTaskContext("CS::SCAN_READ", counters.ResourcesSubscriberCounters)
    , ScanId(scanId)
    , ScanActorId(scanActorId)
    , ResourceSubscribeActorId(resourceSubscribeActorId)
    , ComputeShardingPolicy(computeShardingPolicy)
    , ConveyorProcessGuard(
          NConveyorComposite::TScanServiceOperator::StartProcess(ScanId, cpuLimits.GetCPUGroupNameDef(NResourcePool::DEFAULT_POOL_ID), cpuLimits,
              true))
    , ScanOrbit(scanOrbit)
{
    Y_ABORT_UNLESS(ReadMetadata);
    if (ReadMetadata->HasResultSchema()) {
        Resolver = std::make_shared<NCommon::TIndexColumnResolver>(ReadMetadata->GetResultSchema()->GetIndexInfo());
    }
}

void TReadContext::EnqueueEmptyApply(std::unique_ptr<TEmptyApplyItem>&& item) {
    AFL_VERIFY(item);
    bool sendFlush = false;
    {
        TGuard<TMutex> g(EmptyApplyMutex);
        EmptyApplies.emplace_back(std::move(item));
        if (!EmptyApplyFlushScheduled) {
            EmptyApplyFlushScheduled = true;
            sendFlush = true;
        }
    }
    if (sendFlush) {
        NActors::TActivationContext::Send(ScanActorId, std::make_unique<NColumnShard::TEvPrivate::TEvFlushEmptySourceApplies>());
    }
}

std::vector<std::unique_ptr<TEmptyApplyItem>> TReadContext::ExtractEmptyApplies(const ui32 maxCount) {
    std::vector<std::unique_ptr<TEmptyApplyItem>> result;
    {
        TGuard<TMutex> g(EmptyApplyMutex);
        const ui32 n = Min<ui32>(maxCount, EmptyApplies.size());
        result.reserve(n);
        for (ui32 i = 0; i < n; ++i) {
            result.emplace_back(std::move(EmptyApplies.front()));
            EmptyApplies.pop_front();
        }
        EmptyApplyFlushScheduled = !EmptyApplies.empty();
    }
    std::sort(result.begin(), result.end(), [](const std::unique_ptr<TEmptyApplyItem>& a, const std::unique_ptr<TEmptyApplyItem>& b) {
        return a->SourceIdx < b->SourceIdx;
    });
    return result;
}

bool TReadContext::HasPendingEmptyApplies() const {
    TGuard<TMutex> g(EmptyApplyMutex);
    return !EmptyApplies.empty();
}

}   // namespace NKikimr::NOlap::NReader
