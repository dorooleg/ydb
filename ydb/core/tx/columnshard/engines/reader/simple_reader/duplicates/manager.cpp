#include "executor.h"
#include "manager.h"
#include "splitter.h"

#include <ydb/core/tx/columnshard/column_fetching/cache_policy.h>
#include <ydb/core/tx/columnshard/engines/reader/simple_reader/duplicates/merge.h>
#include <ydb/core/tx/columnshard/engines/reader/simple_reader/iterator/context.h>
#include <ydb/core/tx/columnshard/engines/reader/simple_reader/iterator/scanner.h>
#include <ydb/core/tx/columnshard/engines/reader/simple_reader/iterator/source.h>
#include <ydb/core/tx/conveyor/usage/service.h>
#include <ydb/core/tx/limiter/grouped_memory/usage/service.h>

namespace NKikimr::NOlap::NReader::NSimple::NDuplicateFiltering {

namespace {

class TPortionIntersectionsAllocation: public NGroupedMemoryManager::IAllocation {
private:
    TActorId Owner;
    std::shared_ptr<TFilterAccumulator> Request;
    YDB_READONLY_DEF(std::unique_ptr<TFilterBuildingGuard>, RequestGuard);

private:
    virtual void DoOnAllocationImpossible(const TString& errorMessage) override {
        Request->Abort(TStringBuilder() << "cannot allocate memory: " << errorMessage);
        Request->AddStepLatency(NColumnShard::TDeduplicationStep::INTERSECTION_ALLOCATION);
    }
    virtual bool DoOnAllocated(std::shared_ptr<NGroupedMemoryManager::TAllocationGuard>&& guard,
        const std::shared_ptr<NGroupedMemoryManager::IAllocation>& /*allocation*/) override {
        TActorContext::AsActorContext().Send(Owner, new NPrivate::TEvFilterRequestResourcesAllocated(Request, guard, std::move(RequestGuard)));
        Request->AddStepLatency(NColumnShard::TDeduplicationStep::INTERSECTION_ALLOCATION);
        return true;
    }

public:
    TPortionIntersectionsAllocation(const TActorId& owner, const std::shared_ptr<TFilterAccumulator>& request, const ui64 mem,
        std::unique_ptr<TFilterBuildingGuard>&& requestGuard)
        : NGroupedMemoryManager::IAllocation(mem)
        , Owner(owner)
        , Request(request)
        , RequestGuard(std::move(requestGuard))
    {
    }
};
}   // namespace

#define LOCAL_LOG_TRACE \
    AFL_TRACE(NKikimrServices::TX_COLUMNSHARD_SCAN)("component", "duplicates_manager")("self", TActivationContext::AsActorContext().SelfID)

TDuplicateManager::TDuplicateManager(const TSpecialReadContext& context, const std::deque<std::shared_ptr<TPortionInfo>>& portions)
    : TActor(&TDuplicateManager::StateMain)
    , LastSchema(context.GetCommonContext()->GetReadMetadata()->GetIndexVersions().GetLastSchema())
    , PKColumns(context.GetPKColumns())
    , PKSchema(context.GetCommonContext()->GetReadMetadata()->GetIndexVersions().GetPrimaryKey())
    , Counters(context.GetCommonContext()->GetCounters().GetDuplicateFilteringCounters())
    , Intervals(MakeIntervalTree(portions))
    , Portions(MakePortionsIndex(Intervals))
    , DataAccessorsManager(context.GetCommonContext()->GetDataAccessorsManager())
    , ColumnDataManager(context.GetCommonContext()->GetColumnDataManager())
    , MaterializedBordersCache(BORDER_CACHE_SIZE_COUNT)
    , AbortionFlag(std::make_shared<TAtomicCounter>(0))
{
    for (const auto& portion : portions) {
        Borders.insert(NArrow::TSimpleRow {portion->IndexKeyStart()});
        Borders.insert(NArrow::TSimpleRow {portion->IndexKeyEnd()});
    }

    if (!Borders.empty()) {
        Left = *Borders.begin();
        Right = *Borders.begin();
    }
    
    NextRange();
}

bool TDuplicateManager::IsExclusiveInterval(const NArrow::TSimpleRow& begin, const NArrow::TSimpleRow& end) const {
    ui64 intersectionsCount = 0;
    return Intervals.EachIntersection(TPortionIntervalTree::TRange(begin, true, end, true),
        [&intersectionsCount](const TPortionIntervalTree::TRange& /*interval*/, const std::shared_ptr<TPortionInfo>& /*portion*/) {
            ++intersectionsCount;
            return intersectionsCount == 1;
        });
}

void TDuplicateManager::Handle(const TEvRequestFilter::TPtr& ev) {
    TPortionInfo::TConstPtr mainPortion = Portions->GetPortionVerified(ev->Get()->GetPortionId());
    auto constructor = std::make_shared<TFilterAccumulator>(ev, mainPortion->GetRecordsCount(), Counters);
    
    if (constructor->GetRequest()->Get()->MinPK == mainPortion->IndexKeyStart() && constructor->GetRequest()->Get()->MaxPK == mainPortion->IndexKeyEnd() && IsExclusiveInterval(mainPortion->IndexKeyStart(), mainPortion->IndexKeyEnd())) {
        auto filter = NArrow::TColumnFilter::BuildAllowFilter();
        filter.Add(true, mainPortion->GetRecordsCount());
        constructor->SetIntervalsCount(1);
        constructor->AddFilter(0, std::move(TPortionColumnFilter{0, std::move(filter)}));
        AFL_VERIFY(constructor->IsDone());
        Counters->OnFilterRequest(1);
        Counters->OnRowsMerged(0, 0, mainPortion->GetRecordsCount());
        constructor->AddStepLatency(NColumnShard::TDeduplicationStep::PREPARE_REQUEST);
        return;
    }

    auto task = std::make_shared<TPortionIntersectionsAllocation>(
        SelfId(), constructor, TBuildFilterContext::GetApproximateDataSize(ExpectedIntersectionCount), std::make_unique<TFilterBuildingGuard>());
    NGroupedMemoryManager::TDeduplicationMemoryLimiterOperator::SendToAllocation(task->GetRequestGuard()->GetMemoryProcessId(),
        task->GetRequestGuard()->GetMemoryScopeId(), task->GetRequestGuard()->GetMemoryGroupId(), { task },
        (ui64)TFilterAccumulator::EFetchingStage::INTERSECTIONS);
    constructor->AddStepLatency(NColumnShard::TDeduplicationStep::PREPARE_REQUEST);
    return;
}

NArrow::TSimpleRow TDuplicateManager::GetLeft(const NArrow::TSimpleRow& minPK, const NArrow::TSimpleRow&) {
    if (Left.value_or(minPK) <= minPK) {
        return Left.value_or(minPK);
    }
    return minPK;
}

NArrow::TSimpleRow TDuplicateManager::GetRight(const NArrow::TSimpleRow& minPK, const NArrow::TSimpleRow& maxPK) {
    if (Left.value_or(minPK) < maxPK) {
        return Right.value_or(maxPK);
    }
    return maxPK;
}

// ^   L R    ^   L R      ^   LR     ^

TIntervalsIterator TDuplicateManager::StartIntervalProcessing(
    const THashSet<ui64>& intersectingPortions, const std::shared_ptr<TFilterAccumulator>& constructor) {
    const std::shared_ptr<const TPortionInfo>& mainPortion = Portions->GetPortionVerified(constructor->GetRequest()->Get()->GetPortionId());
    THashMap<ui64, TSortableBorders> materializedBorders;
    for (const auto& portionId : intersectingPortions) {
        if (portionId == mainPortion->GetPortionId()) {
            materializedBorders.emplace(std::numeric_limits<ui64>::max(),
            TSortableBorders(std::make_shared<NArrow::NMerger::TSortableBatchPosition>(GetLeft(constructor->GetRequest()->Get()->MinPK, constructor->GetRequest()->Get()->MaxPK).BuildSortablePosition()),
                std::make_shared<NArrow::NMerger::TSortableBatchPosition>(GetRight(constructor->GetRequest()->Get()->MinPK, constructor->GetRequest()->Get()->MaxPK).BuildSortablePosition())));
                
            materializedBorders.emplace(portionId,
            TSortableBorders(std::make_shared<NArrow::NMerger::TSortableBatchPosition>(constructor->GetRequest()->Get()->MinPK.BuildSortablePosition()),
                std::make_shared<NArrow::NMerger::TSortableBatchPosition>(constructor->GetRequest()->Get()->MaxPK.BuildSortablePosition())));
        } else {
            materializedBorders.emplace(portionId, GetBorders(portionId));
        }
    }
    TColumnDataSplitter splitter(materializedBorders);
    LOCAL_LOG_TRACE("event", "split_portion")
    ("source", constructor->GetRequest()->Get()->GetPortionId())("splitter", splitter.DebugString())(
        "intersection_portions", intersectingPortions.size());
    THashMap<ui32, TPortionColumnFilter> readyFilters;
    std::vector<ui32> intervalsToBuild;
    int sharedIntervals = 0;
    int realIntervalIdx = 0;
    {
        ui64 nextIntervalIdx = 0;
        auto it = FiltersCache.find(mainPortion->GetPortionId());
        auto scheduleInterval = [&](const TIntervalBorder& begin, const TIntervalBorder& end, const THashSet<ui64>& /*portions*/) {
            ++nextIntervalIdx;
            bool isSuitable = false;
            if (constructor->GetRequest()->Get()->MinPK.BuildSortablePosition().Compare(*begin.GetKey()) != std::partial_ordering::less && 
    constructor->GetRequest()->Get()->MaxPK.BuildSortablePosition().Compare(*end.GetKey()) != std::partial_ordering::greater) {
                realIntervalIdx++;
                isSuitable = true;
            }
            TIntervalBorders intervalBorder(begin.GetKey(), end.GetKey());
            TIntervalBordersView intervalView(begin.MakeView(), end.MakeView());
            if (isSuitable) {
                if (it != FiltersCache.end()) {
                    if (auto findCached = it->second.find(
                            TDuplicateMapInfo(constructor->GetRequest()->Get()->GetMaxVersion(), intervalView, intervalBorder, mainPortion->GetPortionId()));
                        findCached != it->second.end()) {
                        AFL_VERIFY(readyFilters.emplace(realIntervalIdx - 1, findCached->second).second);
                        Counters->OnFilterCacheHit();
                        return true;
                    }
                }
            }

            auto [inFlight, emplaced] = IntervalsInFlight.emplace(intervalBorder, TIntervalInFlightInfo());
            if (isSuitable) {
                inFlight->second.AddSubscriber(mainPortion->GetPortionId(), TIntervalFilterCallback(realIntervalIdx - 1, constructor));
            }
            if (emplaced) {
                intervalsToBuild.emplace_back(nextIntervalIdx - 1);
                Counters->OnFilterCacheMiss();
            } else {
                Counters->OnFilterCacheHit();
                sharedIntervals++;
            }
            return true;
        };
        splitter.ForEachIntersectingInterval(std::move(scheduleInterval), std::numeric_limits<ui64>::max());
        if (realIntervalIdx) {
            constructor->SetIntervalsCount(realIntervalIdx);
        } else {
            constructor->SetIntervalsCount(1);
            TPortionColumnFilter filter{0, NArrow::TColumnFilter::BuildConstFilter(false, {0})};
            constructor->AddFilter(0, std::move(filter));
        }
        
    }
    for (auto&& [idx, filter] : std::move(readyFilters)) {
        constructor->AddFilter(idx, std::move(filter));
    }
    auto iterators = TIntervalsIteratorBuilder::BuildFromSplitter(splitter, intervalsToBuild, std::numeric_limits<ui64>::max());
    Counters->OnIntervalsRequest(iterators.GetIntervalsMutable().size());
    Counters->OnSharedIntervalsRequest(sharedIntervals);
    if (!iterators.GetIntervalsMutable().empty()) {
        Counters->OnRequestCacheMiss();
        iterators.GetIntervalsMutable().back().GetEnd().SetIsLast(constructor->GetRequest()->Get()->MaxPK == mainPortion->IndexKeyEnd());
    } else {
        Counters->OnRequestCacheHit();
    }
    
    return iterators;
}

void TDuplicateManager::Handle(const NPrivate::TEvFilterRequestResourcesAllocated::TPtr& ev) {
    TInstant start = TInstant::Now();
    std::shared_ptr<TFilterAccumulator> constructor = ev->Get()->GetRequest();
    std::shared_ptr<NGroupedMemoryManager::TAllocationGuard> memoryGuard = ev->Get()->ExtractAllocationGuard();
    auto requestGuard = ev->Get()->ExtractRequestGuard();

    THashSet<ui64> intersectingPortions;
    const std::shared_ptr<const TPortionInfo>& mainPortion = Portions->GetPortionVerified(constructor->GetRequest()->Get()->GetPortionId());
    {
        const auto collector = [&intersectingPortions](
                                   const TPortionIntervalTree::TRange& /*interval*/, const std::shared_ptr<TPortionInfo>& portion) {
            AFL_VERIFY(intersectingPortions.insert(portion->GetPortionId()).second);
            return true;
        };
        Intervals.EachIntersection(
            TPortionIntervalTree::TRange(GetLeft(constructor->GetRequest()->Get()->MinPK, constructor->GetRequest()->Get()->MaxPK), true, GetRight(constructor->GetRequest()->Get()->MinPK, constructor->GetRequest()->Get()->MaxPK), true), collector);
    }
    Counters->OnFilterRequest(intersectingPortions.size());
    ExpectedIntersectionCount = intersectingPortions.size();

    LOCAL_LOG_TRACE("event", "request_filter")
    ("source", constructor->GetRequest()->Get()->GetPortionId())("intersecting_portions", intersectingPortions.size());
    AFL_VERIFY(intersectingPortions.size());

    TIntervalsIterator intervalsIterator = StartIntervalProcessing(intersectingPortions, constructor);

    if (!intervalsIterator.IsDone()) {
        THashMap<ui64, TPortionInfo::TConstPtr> portionsToFetch;
        for (const auto& id : intervalsIterator.GetNeededPortions()) {
            portionsToFetch.emplace(id, Portions->GetPortionVerified(id));
        }

        if (!constructor->IsDone()) {
            TBuildFilterContext columnFetchingRequest(SelfId(), AbortionFlag, constructor->GetRequest()->Get()->GetMaxVersion(),
                std::move(portionsToFetch), GetFetchingColumns(), PKSchema, LastSchema, ColumnDataManager, DataAccessorsManager, Counters,
                std::move(requestGuard), memoryGuard);
            memoryGuard->Update(columnFetchingRequest.GetDataSize());

            for (const auto& interval : intervalsIterator.GetIntervals()) {
                auto findInFlight = IntervalsInFlight.FindPtr(interval.MakeInterval());
                AFL_VERIFY(findInFlight);
                findInFlight->SetJob(columnFetchingRequest.GetStatus());
            }

            std::shared_ptr<TBuildFilterTaskExecutor> executor = std::make_shared<TBuildFilterTaskExecutor>(std::move(intervalsIterator));
            AFL_VERIFY(executor->ScheduleNext(std::move(columnFetchingRequest)));
        }
    }

    ValidateInFlightProgress();
    Counters->OnStartIntervalProcessing((TInstant::Now() - start).MilliSeconds());
    constructor->AddStepLatency(NColumnShard::TDeduplicationStep::PREPARE_INTERVALS);
    
    auto minPK = constructor->GetRequest()->Get()->MinPK;
    auto maxPK = constructor->GetRequest()->Get()->MaxPK;
    Cerr << "HERE: LEFT: " << Left.value_or(minPK).DebugString() << " RIGHT: " << Right.value_or(maxPK).DebugString() << " min: " << minPK.DebugString() << " max: " << maxPK.DebugString() << " get left:" << GetLeft(minPK, maxPK).DebugString() << " get right:" << GetRight(minPK, maxPK).DebugString() << " Borders: " << Borders.size() << Endl;
    if (Left.value_or(minPK) <= minPK || Left.value_or(minPK) < maxPK) {
        Cerr << "HERE (move next before): LEFT: " << Left.value_or(minPK).DebugString() << " RIGHT: " << Right.value_or(maxPK).DebugString() << " min: " << minPK.DebugString() << " max: " << maxPK.DebugString() << " get left:" << GetLeft(minPK, maxPK).DebugString() << " get right:" << GetRight(minPK, maxPK).DebugString() << " Borders: " << Borders.size() << Endl;
        NextRange();
        Cerr << "HERE (move next after): LEFT: " << Left.value_or(minPK).DebugString() << " RIGHT: " << Right.value_or(maxPK).DebugString() << " min: " << minPK.DebugString() << " max: " << maxPK.DebugString() << " get left:" << GetLeft(minPK, maxPK).DebugString() << " get right:" << GetRight(minPK, maxPK).DebugString() << " Borders: " << Borders.size() << Endl;
        Counters->OnNextRange();
    }
}

void TDuplicateManager::Handle(const NPrivate::TEvFilterConstructionResult::TPtr& ev) {
    TInstant start = TInstant::Now();
    int count = 0;
    if (ev->Get()->GetConclusion().IsFail()) {
        LOCAL_LOG_TRACE("event", "filter_construction_error")("error", ev->Get()->GetConclusion().GetErrorMessage());
        AbortAndPassAway(ev->Get()->GetConclusion().GetErrorMessage());
        return;
    }
    LOCAL_LOG_TRACE("event", "filters_constructed")("filters", ev->Get()->GetConclusion().GetResult().size());
    AFL_VERIFY(ev->Get()->GetConclusion().GetResult().size());
    for (auto&& [mapInfo, filter] : ev->Get()->ExtractResult()) {
        count++;
        if (auto findInterval = IntervalsInFlight.find(mapInfo.GetInterval()); findInterval != IntervalsInFlight.end()) {
            if (!findInterval->second.OnFilterReady(mapInfo.GetPortionId(), filter)) {
                FiltersCache[mapInfo.GetPortionId()].insert_or_assign(mapInfo, std::move(filter));
            }
            if (findInterval->second.IsDone()) {
                IntervalsInFlight.erase(findInterval);
            }
        }
        LOCAL_LOG_TRACE("event", "extract_constructed_filter")("range", mapInfo.DebugString());
    }

    ValidateInFlightProgress();
    Counters->OnProcessIntervalResult((TInstant::Now() - start).MilliSeconds());
    Counters->OnCountFilters(count);
}

}   // namespace NKikimr::NOlap::NReader::NSimple::NDuplicateFiltering
