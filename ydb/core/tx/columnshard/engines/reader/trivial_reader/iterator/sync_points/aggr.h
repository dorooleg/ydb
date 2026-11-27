#pragma once
#include "abstract.h"

#include <ydb/core/tx/columnshard/engines/reader/tracing/data_source_probes.h>
#include <ydb/core/tx/columnshard/engines/reader/trivial_reader/iterator/plain_read_data.h>

namespace NKikimr::NOlap::NReader::NTrivial {

LWTRACE_USING(YDB_CS_DATA_SOURCE);

class TOrderedResultWithLimitCollection;

class TSyncPointResultsAggregationControl: public ISyncPoint {
private:
    using TBase = ISyncPoint;

    std::vector<std::unique_ptr<NCommon::TDataSourceLease>> SourcesToAggregate;
    const std::shared_ptr<ISourcesCollection> Collection;
    const std::shared_ptr<TFetchingScript> AggregationScript;
    const std::shared_ptr<TFetchingScript> RestoreResultScript;
    const std::shared_ptr<NArrow::NSSA::IResourcesAggregator> Aggregator;
    TPositiveControlInteger InFlightControl;
    TPositiveControlInteger SourcesCount;
    bool AggregationActivity = true;
    ui32 AggregationsCount = 0;
    ui32 UselessAggregationsCount = 0;

    static inline const double CriticalBadAggregationKffForSource = 1.5;
    static const ui32 GuaranteeNeedAggregationSourceRecordsCount = 1000;
    static const ui64 AggregationMemorySize = ((ui64)8) << 20;

    static const ui32 AggregationPackSize = 10000;

    static const ui32 AggregatedResultKeysCountMinimalForControl = 10000;
    static inline const double CriticalBadAggregationKffForAggregation = 5;

    static inline const double UselessDetectorFractionKff = 0.5;
    static const ui32 UselessDetectorCountLimit = 7;

    TPositiveControlInteger MemoryToAggregate;

    virtual TString DoDebugString() const override {
        TStringBuilder sb;
        sb << "{";
        sb << SourcesToAggregate.size() << ",";
        sb << MemoryToAggregate << ",";
        sb << InFlightControl << ",";
        sb << SourcesCount << ",";
        sb << AggregationActivity << ",";
        sb << AggregationsCount << ",";
        sb << UselessAggregationsCount;
        sb << "}";
        return sb;
    }

    std::unique_ptr<NCommon::TDataSourceLease> Flush() {
        if (SourcesToAggregate.empty()) {
            return nullptr;
        }
        YDB_LOG_DEBUG_COMP(NKikimrServices::TX_COLUMNSHARD_SCAN, "",
            {"event", "aggregation_batching"},
            {"count", SourcesToAggregate.size()});
        ++InFlightControl;
        auto result = std::make_shared<TAggregationDataSource>(std::move(SourcesToAggregate), Context);
        result->InitPurposeSyncPointIndex(GetPointIndex());
        SourcesToAggregate.clear();
        MemoryToAggregate = 0;
        SourcesSequentially.emplace_back(*result);
        result->InitFetchingPlan(AggregationScript);
        return std::make_unique<NCommon::TDataSourceLease>(std::move(result));
    }

    void FlushAndStart() {
        if (auto flushed = Flush()) {
            IDataSource::StartProcessing(std::move(flushed));
        }
    }

    std::unique_ptr<NCommon::TDataSourceLease> TryToFlush() {
        // Sources held here stay in flight for the collection, so it may stop giving new ones long before the pack is full.
        const bool noMoreSources = Collection->IsFinished() || !Collection->CheckInFlightLimits();
        if (!AggregationActivity || SourcesToAggregate.size() >= AggregationPackSize || MemoryToAggregate.Val() >= AggregationMemorySize ||
            (noMoreSources && Collection->GetSourcesInFlightCount() == SourcesCount.Val())) {
            YDB_LOG_DEBUG_COMP(NKikimrServices::TX_COLUMNSHARD_SCAN, "",
                {"event", "flush"},
                {"toAggr", SourcesToAggregate.size()},
                {"fin", Collection->IsFinished()},
                {"fly", Collection->GetSourcesInFlightCount()},
                {"count", SourcesCount},
                {"max", Collection->GetMaxInFlight()},
                {"memory", MemoryToAggregate.Val()});
            return Flush();
        }
        YDB_LOG_DEBUG_COMP(NKikimrServices::TX_COLUMNSHARD_SCAN, "",
            {"toAggr", SourcesToAggregate.size()},
            {"fin", Collection->IsFinished()},
            {"fly", Collection->GetSourcesInFlightCount()},
            {"count", SourcesCount},
            {"max", Collection->GetMaxInFlight()});
        return nullptr;
    }

    bool CanAggregate(const NCommon::IDataSource& source) const {
        // Pages already sent before the scan was resumed are skipped by the result building only.
        if (!AggregationActivity || source.GetAs<IDataSource>()->GetIsStartedByCursor()) {
            return false;
        }
        const auto& table = source.GetStageData().GetTable();
        if (Aggregator->CheckSource(table).IsFail()) {
            return false;
        }
        if (SourcesToAggregate.size()) {
            return true;
        }
        ui32 originalCount = source.GetRecordsCount();
        if (!table.GetFilter().IsTotalAllowFilter()) {
            originalCount = table.GetFilter().GetFilteredCountVerified();
        }
        const ui32 aggrKeysCount = table.GetRecordsCountActualVerified();
        return aggrKeysCount < GuaranteeNeedAggregationSourceRecordsCount || aggrKeysCount * CriticalBadAggregationKffForSource < originalCount;
    }

    void OnSourceProcessed() {
        --InFlightControl;
        if (InFlightControl.Val() == 0) {
            for (auto&& i : SourcesToAggregate) {
                i->GetSource().MutableAs<IDataSource>()->ClearMemoryGuards();
            }
        }
    }

    virtual bool IsSourcePrepared(const NCommon::IDataSource& source) const override {
        return source.IsSyncSection() && source.HasStageResult() &&
               (source.GetStageResult().HasResultChunk() || source.GetStageResult().IsEmpty());
    }

    virtual std::unique_ptr<NCommon::TDataSourceLease> DoOnSourceFinishedOnPreviouse() override {
        return TryToFlush();
    }

    virtual bool IsFinished() const override {
        return ISyncPoint::IsFinished() && SourcesToAggregate.empty();
    }

    virtual std::unique_ptr<NCommon::TDataSourceLease> OnAddSource(std::unique_ptr<NCommon::TDataSourceLease> lease) override {
        auto& source = lease->GetSource();
        const bool localAggregationActivity = CanAggregate(source);
        // The pack leaves before the source to keep the sources order, and a pack holds results of the same types only.
        if (!localAggregationActivity ||
            (SourcesToAggregate.size() &&
                Aggregator->CheckSource(source.GetStageData().GetTable(), &SourcesToAggregate.front()->GetSource().GetStageData().GetTable())
                    .IsFail())) {
            FlushAndStart();
        }
        ++SourcesCount;
        if (localAggregationActivity) {
            MemoryToAggregate += source.GetReservedMemory();
            if (InFlightControl.Val() == 0) {
                source.MutableAs<IDataSource>()->ClearMemoryGuards();
            }
            SourcesToAggregate.emplace_back(std::move(lease));
            return TryToFlush();
        } else {
            ++InFlightControl;
            SourcesSequentially.emplace_back(source);
            source.MutableAs<IDataSource>()->InitFetchingPlan(RestoreResultScript);
            return lease;
        }
    }

    virtual void DoAbort() override {
        MemoryToAggregate = 0;
        SourcesToAggregate.clear();
    }

    ESourceAction OnAggregatedSourceReady(NCommon::IDataSource& source, TPlainReadData& reader) {
        OnSourceProcessed();
        const auto& aggrSource = static_cast<const TAggregationDataSource&>(source);
        for (auto&& i : aggrSource.GetSources()) {
            Collection->OnSourceFinished(i->GetSource());
            --SourcesCount;
        }
        const auto& lastSource = aggrSource.GetSources().back()->GetSource();
        auto cursor =
            Collection->BuildCursor(lastSource, lastSource.GetRecordsCount(), Context->GetCommonContext()->GetReadMetadata()->GetTabletId());
        AFL_VERIFY(!source.GetStageResult().IsEmpty());
        auto resultChunk = source.MutableStageResult().ExtractResultChunk();
        AFL_VERIFY(source.GetStageResult().IsFinished());
        AFL_VERIFY(resultChunk && resultChunk->HasData());
        if (AggregationActivity) {
            ++AggregationsCount;
            if (resultChunk->GetTable()->num_rows() > AggregatedResultKeysCountMinimalForControl &&
                source.GetRecordsCount() < CriticalBadAggregationKffForAggregation * resultChunk->GetTable()->num_rows()) {
                YDB_LOG_DEBUG_COMP(NKikimrServices::TX_COLUMNSHARD_SCAN, "",
                    {"event", "useless_aggregation"},
                    {"sourceIdx", source.GetSourceIdx()},
                    {"table", resultChunk->GetTable()->num_rows()},
                    {"originalCount", source.GetRecordsCount()},
                    {"activity", AggregationActivity},
                    {"uselessCount", UselessAggregationsCount},
                    {"aggrCount", AggregationsCount});
                if (++UselessAggregationsCount > UselessDetectorFractionKff * AggregationsCount &&
                    AggregationsCount > UselessDetectorCountLimit) {
                    AggregationActivity = false;
                }
            }
        }
        YDB_LOG_DEBUG_COMP(NKikimrServices::TX_COLUMNSHARD_SCAN, "",
            {"event", "has_result"},
            {"sourceIdx", source.GetSourceIdx()},
            {"table", resultChunk->GetTable()->num_rows()},
            {"originalCount", source.GetRecordsCount()},
            {"activity", AggregationActivity});
        reader.OnIntervalResult(
            std::make_unique<TPartialReadResult>(source.ExtractResourceGuards(), source.MutableAs<IDataSource>()->ExtractGroupGuard(),
                resultChunk->ExtractTable(), std::move(cursor), Context->GetCommonContext(), std::nullopt, source.GetSourceId()));
        source.MutableAs<IDataSource>()->ClearResult();
        return ESourceAction::Finish;
    }

    ESourceAction OnPortionSourceReady(NCommon::IDataSource& source, TPlainReadData& reader) {
        if (!source.GetStageResult().IsEmpty()) {
            auto resultChunk = source.MutableStageResult().ExtractResultChunk();
            const bool isFinished = source.GetStageResult().IsFinished();
            const bool hasData = resultChunk && resultChunk->HasData();
            if (hasData) {
                std::optional<TPartialSourceAddress> partialSourceAddress;
                if (!isFinished) {
                    partialSourceAddress = TPartialSourceAddress(source.GetSourceIdx(), GetPointIndex());
                }
                YDB_LOG_DEBUG_COMP(NKikimrServices::TX_COLUMNSHARD_SCAN, "",
                    {"event", "has_result"},
                    {"sourceIdx", source.GetSourceIdx()},
                    {"table", resultChunk->GetTable()->num_rows()},
                    {"isFinished", isFinished});
                const ui32 readyRecords = isFinished ? source.GetRecordsCount() : resultChunk->GetStartIndex() + resultChunk->GetRecordsCount();
                auto cursor = Collection->BuildCursor(source, readyRecords, Context->GetCommonContext()->GetReadMetadata()->GetTabletId());
                reader.OnIntervalResult(std::make_unique<TPartialReadResult>(source.GetResourceGuards(),
                    source.MutableAs<IDataSource>()->GetGroupGuard(), resultChunk->ExtractTable(), std::move(cursor),
                    Context->GetCommonContext(), partialSourceAddress, source.GetSourceId()));
            }
            if (!isFinished) {
                return hasData ? ESourceAction::Wait : ESourceAction::Continue;
            }
        }
        OnSourceProcessed();
        Collection->OnSourceFinished(source);
        --SourcesCount;
        source.MutableAs<IDataSource>()->ClearResult();
        return ESourceAction::Finish;
    }

    virtual ESourceAction OnSourceReady(const NCommon::TDataSourceLease& lease, TPlainReadData& reader) override {
        auto& source = lease.GetSource();
        LWTRACK(SyncAggrSyncPoint, source.GetDataSourceOrbit(), source.GetRawPathId(), source.GetTabletId(), source.GetTxId(),
            source.GetSourceId(), GetPointName(), source.GetFilteredRowsCount(), source.GetReservedMemory(),
            source.GetSourcesAheadQueueWaitDuration(), source.GetSourcesAhead(), DebugString());
        AFL_VERIFY(!Next);
        if (source.GetType() == IDataSource::EType::SimpleAggregation) {
            return OnAggregatedSourceReady(source, reader);
        }
        AFL_VERIFY(source.GetType() == IDataSource::EType::SimplePortion);
        return OnPortionSourceReady(source, reader);
    }

public:
    TSyncPointResultsAggregationControl(const std::shared_ptr<ISourcesCollection>& collection,
        const std::shared_ptr<TFetchingScript>& aggregationScript, const std::shared_ptr<TFetchingScript>& restoreResultScript,
        const std::shared_ptr<NArrow::NSSA::IResourcesAggregator>& aggregator, const ui32 pointIndex,
        const std::shared_ptr<TSpecialReadContext>& context)
        : TBase(pointIndex, "SYNC_AGGR", context, nullptr)
        , Collection(collection)
        , AggregationScript(aggregationScript)
        , RestoreResultScript(restoreResultScript)
        , Aggregator(aggregator)
    {
        AFL_VERIFY(AggregationScript);
        AFL_VERIFY(RestoreResultScript);
        AFL_VERIFY(Aggregator);
        AFL_VERIFY(pointIndex);
    }
};

}   // namespace NKikimr::NOlap::NReader::NTrivial
