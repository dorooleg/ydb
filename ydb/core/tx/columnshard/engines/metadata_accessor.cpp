#include "metadata_accessor.h"

#include "reader/common/description.h"
#include "reader/common_reader/constructor/read_metadata.h"
#include "reader/plain_reader/iterator/constructors.h"
#include "reader/simple_reader/iterator/collections/constructors.h"

#include <ydb/core/formats/arrow/accessor/abstract/accessor.h>
#include <ydb/core/formats/arrow/accessor/plain/accessor.h>
#include <ydb/core/tx/conveyor_composite/usage/service.h>

#include <ydb/library/actors/core/log.h>
#include <ydb/library/formats/arrow/simple_arrays_cache.h>

#include <util/folder/path.h>
#include <algorithm>

namespace NKikimr::NOlap {
ITableMetadataAccessor::ITableMetadataAccessor(const TString& tablePath)
    : TablePath(tablePath) {
    AFL_VERIFY(!!TablePath);
}

std::vector<TNameTypeInfo> ITableMetadataAccessor::GetPrimaryKeyInfo(const TVersionedPresetSchemas& vSchemas) const {
    return GetSnapshotSchemaVerified(vSchemas, TSnapshot::Max())->GetIndexInfo().GetPrimaryKeyColumns();
}

const std::shared_ptr<arrow::Schema>& ITableMetadataAccessor::GetPrimaryKeyScheme(const TVersionedPresetSchemas& vSchemas) const {
    return GetSnapshotSchemaVerified(vSchemas, TSnapshot::Max())->GetIndexInfo().GetPrimaryKey();
}

TString ITableMetadataAccessor::GetTableName() const {
    return TFsPath(TablePath).Fix().GetName();
}

TUserTableAccessor::TUserTableAccessor(const TString& tableName, const NColumnShard::TUnifiedPathId& pathId)
    : TBase(tableName)
    , PathId(pathId) {
    AFL_VERIFY(pathId.IsValid());
}

struct TInfo {
    std::optional<NArrow::TSimpleRow> Left;
    std::optional<IColumnEngine::TSelectedPortionInfo> PortionInfo;
};

std::unique_ptr<NReader::NCommon::ISourcesConstructor> TUserTableAccessor::SelectMetadata(const TSelectMetadataContext& context,
    const NReader::TReadDescription& readDescription, const bool isPlain) const {
    AFL_VERIFY(readDescription.PKRangesFilter);
    // here we select portions for a read
    std::vector<IColumnEngine::TSelectedPortionInfo> portions =
        context.GetEngine().Select(PathId.InternalPathId, readDescription.GetSnapshot(), *readDescription.PKRangesFilter,
            readDescription.readNonconflictingPortions, readDescription.readConflictingPortions, readDescription.ownPortions);
    
    THashMap<ui64, TInfo> previous;
    if (!isPlain) {
        std::sort(portions.begin(), portions.end(), [](const auto& l, const auto& r) {
            if (l.GetPortion()->IndexKeyStart() < r.GetPortion()->IndexKeyStart()) {
                return true;
            }
            if (l.GetPortion()->IndexKeyStart() == r.GetPortion()->IndexKeyStart()) {
                return l.GetPortion()->IndexKeyEnd() < r.GetPortion()->IndexKeyEnd();
            }
            return false;
        });
        std::deque<NReader::NSimple::TIntervalSourceConstructor> sources;
        // for (const auto& p: portions) {
        //     const auto& portion = p.GetPortion();
        //     const bool isVisible = p.GetIsVisible();
        //     sources.emplace_back(NReader::NSimple::TIntervalSourceConstructor(
        //         portion, isVisible, readDescription.GetSorting(),
        //         NArrow::TSimpleRow(portion->IndexKeyStart()), NArrow::TSimpleRow(portion->IndexKeyEnd())));
        // }
        
        for (size_t i = 0; i < portions.size(); ++i) {
            previous[portions[i].GetPortion()->GetPortionId()] = TInfo{portions[i].GetPortion()->IndexKeyStart(), portions[i]};
            if (previous.size() >= 300) {
                for (auto it = previous.begin(); it != previous.end();) {
                    const auto& portion = it->second.PortionInfo->GetPortion();
                    const bool isVisible = it->second.PortionInfo->GetIsVisible();
                    if (*it->second.Left == portions[i].GetPortion()->IndexKeyStart() && it->second.Left != it->second.PortionInfo->GetPortion()->IndexKeyEnd()) {
                        ++it;
                        continue;
                    }
                    if (it->second.PortionInfo->GetPortion()->IndexKeyEnd() <= portions[i].GetPortion()->IndexKeyStart()) {
                        sources.emplace_back(NReader::NSimple::TIntervalSourceConstructor(
                            portion, isVisible, readDescription.GetSorting(),
                            NArrow::TSimpleRow(*it->second.Left), NArrow::TSimpleRow(it->second.PortionInfo->GetPortion()->IndexKeyEnd())));
                        previous.erase(it++);
                    } else {
                        sources.emplace_back(NReader::NSimple::TIntervalSourceConstructor(
                            portion, isVisible, readDescription.GetSorting(),
                            NArrow::TSimpleRow(*it->second.Left), NArrow::TSimpleRow(portions[i].GetPortion()->IndexKeyStart())));
                        it->second.Left = portions[i].GetPortion()->IndexKeyStart();
                        ++it;
                    }
                }
            }
        }
        
        for (auto it = previous.begin(); it != previous.end();) {
            const auto& portion = it->second.PortionInfo->GetPortion();
            const bool isVisible = it->second.PortionInfo->GetIsVisible();
            sources.emplace_back(NReader::NSimple::TIntervalSourceConstructor(
                portion, isVisible, readDescription.GetSorting(),
                NArrow::TSimpleRow(*it->second.Left), NArrow::TSimpleRow(portion->IndexKeyEnd())));
            previous.erase(it++);
        }

        // Cerr << "sources.size() = " << sources.size() << Endl;
        return std::make_unique<NReader::NSimple::TIntervalsSources>(std::move(sources), readDescription.GetSorting());
    } else {
        std::vector<std::shared_ptr<TPortionInfo>> sources;
        for (auto&& i : portions) {
            sources.emplace_back(i.GetPortion());
        }
        return std::make_unique<NReader::NPlain::TPortionSources>(std::move(sources));
    }
}

std::unique_ptr<NReader::NCommon::ISourcesConstructor> TAbsentTableAccessor::SelectMetadata(const TSelectMetadataContext& /*context*/,
    const NReader::TReadDescription& /*readDescription*/, const bool /*isPlain*/) const {
    return NReader::NSimple::TPortionsSources::BuildEmpty();
}

}   // namespace NKikimr::NOlap
