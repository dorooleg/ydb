#include "duplicate_filtering.h"

namespace NKikimr::NColumnShard {
TDuplicateFilteringCounters::TDuplicateFilteringCounters()
    : TBase("DuplicateFiltering")
    , MergeRowsAccepted(TBase::GetDeriviative("DuplicateFiltering/SourcesMerging/RowsAccepted"))
    , MergeRowsRejected(TBase::GetDeriviative("DuplicateFiltering/SourcesMerging/RowsRejected"))
    , MergeRowsBulkAccepted(TBase::GetDeriviative("DuplicateFiltering/SourcesMerging/RowsBulkAccepted"))
    , UselessRowsAccepted(TBase::GetDeriviative("DuplicateFiltering/SourcesMerging/UselessRowsAccepted"))
    , UselessRowsRejected(TBase::GetDeriviative("DuplicateFiltering/SourcesMerging/UselessRowsRejected"))
    , IntersectingPortionsPerRequest(TBase::GetHistogram("DuplicateFiltering/IntersectingPortions", NMonitoring::ExponentialHistogram(18, 2, 1)))
    , FilterCacheHits(TBase::GetDeriviative("DuplicateFiltering/FilterCache/Hits"))
    , FilterCacheMisses(TBase::GetDeriviative("DuplicateFiltering/FilterCache/Misses"))
    , RequestCacheHits(TBase::GetDeriviative("DuplicateFiltering/RequestCache/Hits"))
    , RequestCacheMisses(TBase::GetDeriviative("DuplicateFiltering/RequestCache/Misses"))
    , LatencyPerRequest(TBase::GetHistogram("DuplicateFiltering/LatencyMs", NMonitoring::ExponentialHistogram(18, 2, 1)))
    , LatencyBuildInterval(TBase::GetHistogram("DuplicateFiltering/BuildIntervalLatencyMs", NMonitoring::ExponentialHistogram(18, 2, 1)))
    , LatencyAllocateColumnMemory(TBase::GetHistogram("DuplicateFiltering/AllocateColumnMemoryLatencyMs", NMonitoring::ExponentialHistogram(18, 2, 1)))
    , LatencyStartIntervalProcessing(TBase::GetHistogram("DuplicateFiltering/StartIntervalProcessingLatencyMs", NMonitoring::ExponentialHistogram(18, 2, 1)))
    , LatencyProcessIntervalResult(TBase::GetHistogram("DuplicateFiltering/ProcessIntervalResultMs", NMonitoring::ExponentialHistogram(18, 2, 1)))
    , IntervalsPerRequest(TBase::GetHistogram("DuplicateFiltering/Intervals", NMonitoring::ExponentialHistogram(18, 2, 1)))
    , SharedIntervalsPerRequest(TBase::GetHistogram("DuplicateFiltering/SharedIntervals", NMonitoring::ExponentialHistogram(18, 2, 1)))
    , CountFilters(TBase::GetDeriviative("DuplicateFiltering/CountFilters"))
    , NextRange(TBase::GetDeriviative("DuplicateFiltering/NextRange"))
{
}
}   // namespace NKikimr::NColumnShard
