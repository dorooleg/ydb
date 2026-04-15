#pragma once

#include <ydb/library/signals/histogram.h>
#include <ydb/library/signals/owner.h>

#include <library/cpp/monlib/dynamic_counters/counters.h>

namespace NKikimr::NColumnShard {
    
enum TDeduplicationStep {
    PREPARE_REQUEST = 0,
    INTERSECTION_ALLOCATION = 1,
    PREPARE_INTERVALS = 2,
    DATA_ACCESSOR_ALLOCATION = 3,
    FINISH = 4
};

static TString ToStringStep(TDeduplicationStep step) {
    switch (step) {
        case PREPARE_REQUEST:
            return "PrepareRequest";
        case INTERSECTION_ALLOCATION:
            return "IntersectionAllocation";
        case PREPARE_INTERVALS:
            return "PrepareIntervals";
        case DATA_ACCESSOR_ALLOCATION:
            return "DataAccessorAllocation";
        case FINISH:
            return "Finish";
    }
    return "Unknown";
}

class TDuplicateFilteringCounters: public TCommonCountersOwner {
private:
    using TBase = TCommonCountersOwner;

    NMonitoring::TDynamicCounters::TCounterPtr MergeRowsAccepted;
    NMonitoring::TDynamicCounters::TCounterPtr MergeRowsRejected;
    NMonitoring::TDynamicCounters::TCounterPtr MergeRowsBulkAccepted;
    
    NMonitoring::TDynamicCounters::TCounterPtr UselessRowsAccepted;
    NMonitoring::TDynamicCounters::TCounterPtr UselessRowsRejected;

    NMonitoring::THistogramPtr IntersectingPortionsPerRequest;

    NMonitoring::TDynamicCounters::TCounterPtr FilterCacheHits;
    NMonitoring::TDynamicCounters::TCounterPtr FilterCacheMisses;
    
    NMonitoring::TDynamicCounters::TCounterPtr RequestCacheHits;
    NMonitoring::TDynamicCounters::TCounterPtr RequestCacheMisses;
    
    NMonitoring::THistogramPtr LatencyPerRequest;
    NMonitoring::THistogramPtr LatencyBuildInterval;
    NMonitoring::THistogramPtr LatencyAllocateColumnMemory;
    NMonitoring::THistogramPtr LatencyStartIntervalProcessing;
    NMonitoring::THistogramPtr LatencyProcessIntervalResult;
    NMonitoring::THistogramPtr IntervalsPerRequest;
    NMonitoring::THistogramPtr SharedIntervalsPerRequest;
    NMonitoring::TDynamicCounters::TCounterPtr CountFilters;
    NMonitoring::TDynamicCounters::TCounterPtr NextRange;
    mutable std::unordered_map<TDeduplicationStep, NMonitoring::THistogramPtr> StepLatency;
    

public:
    TDuplicateFilteringCounters();

    void OnRowsMerged(const ui64 accepted, const ui64 rejected, const ui64 bulkAccepted) const {
        MergeRowsAccepted->Add(accepted);
        MergeRowsRejected->Add(rejected);
        MergeRowsBulkAccepted->Add(bulkAccepted);
    }
    
    void OnRowsUseless(const ui64 accepted, const ui64 rejected) const {
        UselessRowsAccepted->Add(accepted);
        UselessRowsRejected->Add(rejected);
    }

    void OnFilterRequest(const ui64 intersectingPortions) const {
        IntersectingPortionsPerRequest->Collect(intersectingPortions);
    }
    
    void OnIntervalsRequest(const ui64 intersectingPortions) const {
        IntervalsPerRequest->Collect(intersectingPortions);
    }
    
    void OnSharedIntervalsRequest(const ui64 intersectingPortions) const {
        SharedIntervalsPerRequest->Collect(intersectingPortions);
    }

    void OnFilterCacheHit(const ui64 count = 1) const {
        FilterCacheHits->Add(count);
    }
    void OnFilterCacheMiss(const ui64 count = 1) const {
        FilterCacheMisses->Add(count);
    }
    
    void OnRequestCacheHit(const ui64 count = 1) const {
        RequestCacheHits->Add(count);
    }

    void OnRequestCacheMiss(const ui64 count = 1) const {
        RequestCacheMisses->Add(count);
    }
    
    void OnRequestFinish(ui64 latencyMs) const {
        LatencyPerRequest->Collect(latencyMs);
    }
    
    void OnBuildInterval(ui64 latencyMs) const {
        LatencyBuildInterval->Collect(latencyMs);
    }

    void OnAllocateColumnMemory(ui64 latencyMs) const {
        LatencyAllocateColumnMemory->Collect(latencyMs);
    }

    void OnStartIntervalProcessing(ui64 latencyMs) const {
        LatencyStartIntervalProcessing->Collect(latencyMs);
    }
    
    void OnProcessIntervalResult(ui64 latencyMs) const {
        LatencyProcessIntervalResult->Collect(latencyMs);
    }
    
    void OnCountFilters(const ui64 count = 1) const {
        CountFilters->Add(count);
    }
    
    void OnNextRange(const ui64 count = 1) const {
        NextRange->Add(count);
    }

    void OnStep(TDeduplicationStep step, ui64 latencyMs) const {
        auto it = StepLatency.find(step);
        if (it == StepLatency.end()) {
            auto& value = StepLatency[step] = TBase::GetHistogram("DuplicateFiltering/Step" + ToStringStep(step), NMonitoring::ExponentialHistogram(18, 2, 1));
            value->Collect(latencyMs);
        } else {
            it->second->Collect(latencyMs);
        }
    }
};
}   // namespace NKikimr::NColumnShard
