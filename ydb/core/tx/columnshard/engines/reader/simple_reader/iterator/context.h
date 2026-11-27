#pragma once
#include "fetching.h"

#include <ydb/core/formats/arrow/reader/merger.h>
#include <ydb/core/tx/columnshard/common/limits.h>
#include <ydb/core/tx/columnshard/engines/reader/abstract/read_context.h>
#include <ydb/core/tx/columnshard/engines/reader/common_reader/iterator/context.h>
#include <ydb/core/tx/columnshard/engines/reader/common_reader/iterator/fetch_steps.h>
#include <ydb/core/tx/columnshard/engines/reader/simple_reader/constructor/read_metadata.h>
#include <ydb/core/tx/columnshard/hooks/abstract/abstract.h>

#include <util/system/guard.h>
#include <util/system/spinlock.h>

namespace NKikimr::NOlap::NReader::NSimple {

class IDataSource;
class TSourceConstructor;
using TColumnsSet = NCommon::TColumnsSet;
using TColumnsSetIds = NCommon::TColumnsSetIds;
using EMemType = NCommon::EMemType;
using TFetchingScript = NCommon::TFetchingScript;

class TSpecialReadContext: public NCommon::TSpecialReadContext, TNonCopyable {
private:
    using TBase = NCommon::TSpecialReadContext;
    mutable TSpinLock DuplicatesManagerLock;
    NActors::TActorId DuplicatesManager = NActors::TActorId();
    const std::shared_ptr<TAtomicCounter> DuplicatesAbortionFlag = std::make_shared<TAtomicCounter>(0);
    ui64 DuplicateFilterPortionCount = 0;

private:
    std::shared_ptr<TFetchingScript> BuildColumnsFetchingPlan(const bool needSnapshots, const bool partialUsageByPredicateExt,
        const bool useIndexes, const bool needFilterSharding, const bool needFilterDeletion, const bool needFilterDuplicates,
        const bool isFinalSyncPoint) const;
    TMutex Mutex;
    std::array<std::array<std::array<std::array<std::array<std::array<NCommon::TFetchingScriptOwner, 2>, 2>, 2>, 2>, 2>, 2> CacheFetchingScripts;

    virtual std::shared_ptr<TFetchingScript> DoGetColumnsFetchingPlan(const NCommon::IDataSource& source, const bool isFinalSyncPoint) override;
    mutable std::optional<std::shared_ptr<TFetchingScript>> SourcesAggregationScript;
    mutable std::shared_ptr<NArrow::NSSA::IResourcesAggregator> SourcesAggregator;
    mutable std::optional<std::shared_ptr<TFetchingScript>> RestoreResultScript;

    bool NeedDuplicateFiltering() const {
        return GetReadMetadata()->NeedDuplicateFiltering();
    }

public:
    std::shared_ptr<TFetchingScript> GetSourcesAggregationScript() const {
        if (!SourcesAggregationScript) {
            const auto& readMetadata = GetReadMetadata();
            const auto& graph = readMetadata->GetProgram().GetGraphOptional();
            // Merged results leave in no key order, and limits count rows of single sources.
            const bool canMergeSources = graph && !readMetadata->IsSorted() && !readMetadata->GetLimitRobustOptional() &&
                                         !readMetadata->GetProgram().GetDistinctKeyColumnIdOptional();
            SourcesAggregator = canMergeSources ? graph->GetResultsAggregationProcessor() : nullptr;
            if (!SourcesAggregator) {
                SourcesAggregationScript = nullptr;
            } else {
                NCommon::TFetchingScriptBuilder builder(*this);
                builder.AddStep(std::make_shared<TInitializeSourceStep>());
                builder.AddStep(std::make_shared<TStepAggregationSources>(SourcesAggregator));
                builder.AddStep(std::make_shared<TCleanAggregationSources>(SourcesAggregator));
                SourcesAggregationScript = std::move(builder).Build();
            }
        }
        return *SourcesAggregationScript;
    }

    const std::shared_ptr<NArrow::NSSA::IResourcesAggregator>& GetSourcesAggregatorVerified() const {
        AFL_VERIFY(GetSourcesAggregationScript());
        return SourcesAggregator;
    }

    std::shared_ptr<TFetchingScript> GetRestoreResultScript() const {
        if (!RestoreResultScript) {
            NCommon::TFetchingScriptBuilder builder(*this);
            builder.AddStep(std::make_shared<NCommon::TBuildStageResultStep>());
            builder.AddStep(std::make_shared<TPrepareResultStep>(true));
            RestoreResultScript = std::move(builder).Build();
        }
        return *RestoreResultScript;
    }

    virtual TString ProfileDebugString() const override;

    void RegisterActors(NCommon::ISourcesConstructor& sources);
    void UnregisterActors();

    const std::shared_ptr<TAtomicCounter>& GetDuplicatesAbortionFlag() const {
        return DuplicatesAbortionFlag;
    }

    NActors::TActorId GetDuplicatesManager() const {
        TGuard<TSpinLock> g(DuplicatesManagerLock);
        return DuplicatesManager;
    }

    ui64 GetDuplicateFilterPortionCount() const {
        return DuplicateFilterPortionCount;
    }

    TSpecialReadContext(const std::shared_ptr<TReadContext>& commonContext)
        : TBase(commonContext)
    {
    }

    ~TSpecialReadContext() {
        if (NActors::TActorSystem::IsStopped()) {
            return;
        }

        TGuard<TSpinLock> g(DuplicatesManagerLock);
        AFL_VERIFY(!DuplicatesManager);
    }
};

}   // namespace NKikimr::NOlap::NReader::NSimple
