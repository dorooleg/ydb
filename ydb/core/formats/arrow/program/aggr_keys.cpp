#include "aggr_keys.h"
#include "collection.h"
#include "execution.h"

#include <ydb/core/formats/arrow/accessor/plain/accessor.h>

#include <ydb/library/formats/arrow/replace_key.h>
#include <ydb/library/formats/arrow/switch/switch_type.h>

#include <util/digest/numeric.h>
#include <util/string/join.h>
#include <yql/essentials/minikql/comp_nodes/mkql_rh_hash.h>

#include <cmath>

#ifndef WIN32
#ifdef NO_SANITIZE_THREAD
#undef NO_SANITIZE_THREAD
#endif
#include <AggregateFunctions/IAggregateFunction.h>
#else
#include <contrib/libs/apache/arrow/cpp/src/arrow/compute/api_aggregate.h>
namespace CH {
enum class AggFunctionId {
    AGG_UNSPECIFIED = 0,
    AGG_ANY = 1,
    AGG_COUNT = 2,
    AGG_MIN = 3,
    AGG_MAX = 4,
    AGG_SUM = 5,
    AGG_AVG = 6,
    //AGG_VAR = 7,
    //AGG_COVAR = 8,
    //AGG_STDDEV = 9,
    //AGG_CORR = 10,
    //AGG_ARG_MIN = 11,
    //AGG_ARG_MAX = 12,
    //AGG_COUNT_DISTINCT = 13,
    //AGG_QUANTILES = 14,
    //AGG_TOP_COUNT = 15,
    //AGG_TOP_SUM = 16,
    AGG_NUM_ROWS = 17,
};
struct GroupByOptions: public arrow::compute::ScalarAggregateOptions {
    struct Assign {
        AggFunctionId function = AggFunctionId::AGG_UNSPECIFIED;
        std::string result_column;
        std::vector<std::string> arguments;
    };

    std::shared_ptr<arrow::Schema> schema;
    std::vector<Assign> assigns;
    bool has_nullable_key = true;
};
}   // namespace CH
#endif

namespace NKikimr::NArrow::NSSA::NAggregation {

CH::AggFunctionId TWithKeysAggregationOption::GetHouseFunction(const EAggregate op) {
    switch (op) {
        case EAggregate::Some:
            return CH::AggFunctionId::AGG_ANY;
        case EAggregate::Count:
            return CH::AggFunctionId::AGG_COUNT;
        case EAggregate::Min:
            return CH::AggFunctionId::AGG_MIN;
        case EAggregate::Max:
            return CH::AggFunctionId::AGG_MAX;
        case EAggregate::Sum:
            return CH::AggFunctionId::AGG_SUM;
        case EAggregate::NumRows:
            return CH::AggFunctionId::AGG_NUM_ROWS;
        default:
            break;
    }
    return CH::AggFunctionId::AGG_UNSPECIFIED;
}

TString TWithKeysAggregationOption::DebugString() const {
    TStringBuilder sb;
    std::vector<ui32> ids;
    for (auto&& i : Inputs) {
        ids.emplace_back(i.GetColumnId());
    }
    sb << "{" << Output.GetColumnId() << "(" << AggregationId << ")" << ":[" << JoinSeq(",", ids) << "]}";
    return sb;
}

TConclusion<TExecutionResult> TWithKeysAggregationProcessor::DoExecute(
    const TProcessorContext& context, const TExecutionNodeContext& /*nodeContext*/) const {
    CH::GroupByOptions funcOpts;
    funcOpts.assigns.reserve(AggregationKeys.size() + Aggregations.size());
    funcOpts.has_nullable_key = false;

    std::vector<arrow::Datum> batch;
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::set<ui32> fieldsUsage;
    for (auto& key : AggregationKeys) {
        AFL_VERIFY(fieldsUsage.emplace(key.GetColumnId()).second);
        batch.emplace_back(context.GetResources().GetArrayVerified(key.GetColumnId()));
        fields.emplace_back(context.GetResources().GetFieldVerified(key.GetColumnId()));
        funcOpts.assigns.emplace_back(CH::GroupByOptions::Assign{ .result_column = ::ToString(key.GetColumnId()) });

        if (!funcOpts.has_nullable_key) {
            arrow::Datum res = batch.back();
            if (res.is_array()) {
                funcOpts.has_nullable_key = res.array()->MayHaveNulls();
            } else {
                return TConclusionStatus::Fail("GROUP BY may be for record batch only.");
            }
        }
    }
    for (auto& aggr : Aggregations) {
        const CH::GroupByOptions::Assign gbAssign = [&aggr]() {
            CH::GroupByOptions::Assign descr;
            descr.function = TWithKeysAggregationOption::GetHouseFunction(aggr.GetAggregationId());
            descr.result_column = ::ToString(aggr.GetOutput().GetColumnId());
            descr.arguments.reserve(aggr.GetInputs().size());

            for (auto& colName : aggr.GetInputs()) {
                descr.arguments.push_back(::ToString(colName.GetColumnId()));
            }
            return descr;
        }();

        funcOpts.assigns.emplace_back(gbAssign);
        for (auto&& i : aggr.GetInputs()) {
            if (fieldsUsage.emplace(i.GetColumnId()).second) {
                batch.emplace_back(context.GetResources().GetArrayVerified(i.GetColumnId()));
                fields.emplace_back(context.GetResources().GetFieldVerified(i.GetColumnId()));
            }
        }
    }

    funcOpts.schema = std::make_shared<arrow::Schema>(fields);

    auto gbRes = arrow::compute::CallFunction(GetHouseGroupByName(), batch, &funcOpts, GetCustomExecContext());
    if (!gbRes.ok()) {
        return TConclusionStatus::Fail(gbRes.status().ToString());
    }
    auto gbBatch = (*gbRes).record_batch();
    context.MutableResources().Remove(AggregationKeys);

    for (auto& assign : funcOpts.assigns) {
        auto column = gbBatch->GetColumnByName(assign.result_column);
        if (!column) {
            return TConclusionStatus::Fail("No expected column in GROUP BY result.");
        }
        if (auto columnId = TryFromString<ui32>(assign.result_column)) {
            context.MutableResources().AddCalculated(*columnId, column);
        } else {
            return TConclusionStatus::Fail("Incorrect column id from name: " + assign.result_column);
        }
    }
    return TExecutionResult::Done();
}

TConclusion<std::shared_ptr<TWithKeysAggregationProcessor>> TWithKeysAggregationProcessor::TBuilder::Finish() {
    AFL_VERIFY(!Finished);
    Finished = true;
    if (Keys.empty()) {
        return TConclusionStatus::Fail("no keys for aggregation");
    }
    if (Aggregations.empty()) {
        return TConclusionStatus::Fail("no aggregations");
    }
    std::set<ui32> input;
    std::set<ui32> output;
    for (auto&& i : Keys) {
        input.emplace(i.GetColumnId());
    }
    for (auto&& i : Aggregations) {
        for (auto&& inp : i.GetInputs()) {
            input.emplace(inp.GetColumnId());
        }
        output.emplace(i.GetOutput().GetColumnId());
    }
    std::vector<TColumnChainInfo> inputChainColumns;
    for (auto&& i : input) {
        inputChainColumns.emplace_back(i);
    }
    std::vector<TColumnChainInfo> outputChainColumns;
    for (auto&& i : output) {
        outputChainColumns.emplace_back(i);
    }
    return std::shared_ptr<TWithKeysAggregationProcessor>(new TWithKeysAggregationProcessor(
        std::move(inputChainColumns), std::move(outputChainColumns), std::move(Keys), std::move(Aggregations)));
}

TConclusionStatus TWithKeysAggregationProcessor::TBuilder::AddGroupBy(
    const std::vector<TColumnChainInfo>& input, const TColumnChainInfo& output, const EAggregate aggrType) {
    if (input.size() > 1) {
        return TConclusionStatus::Fail("a lot of columns for aggregation: " + JoinSeq(", ", input));
    }
    AFL_VERIFY(!Finished);
    Aggregations.emplace_back(input, output, aggrType);
    return TConclusionStatus::Success();
}

TConclusion<TFunctionResult> TAggregateFunction::Call(const TExecFunctionContext& context, const TAccessorsCollection& resources) const {
    if (context.GetColumns().size() == 0 && AggregationType == NAggregation::EAggregate::NumRows) {
        auto rc = resources.GetRecordsCountActualOptional();
        if (!rc) {
            return TConclusionStatus::Fail("resources hasn't info about records count actual");
        } else {
            return TFunctionResult(std::make_shared<arrow::UInt64Scalar>(*rc));
        }
    } else {
        return TBase::Call(context, resources);
    }
}

namespace {

bool IsMergeableKeyType(const arrow::DataType& dataType) {
    return NArrow::SwitchType(dataType.id(), [](const auto& type) {
        using TWrap = std::decay_t<decltype(type)>;
        return TWrap::IsAppropriate;
    });
}

bool IsMergeableValueType(const arrow::DataType& dataType, const EAggregate aggregationType) {
    // Decimals are stored as fixed size binaries and cannot be ordered or summed as such.
    if (dataType.id() == arrow::Type::DECIMAL || dataType.id() == arrow::Type::DECIMAL256) {
        return false;
    }
    return NArrow::SwitchType(dataType.id(), [&](const auto& type) {
        using TWrap = std::decay_t<decltype(type)>;
        switch (aggregationType) {
            case EAggregate::Some:
            case EAggregate::Min:
            case EAggregate::Max:
                return TWrap::IsAppropriate;
            case EAggregate::Sum:
                return TWrap::IsCType && !std::is_same_v<typename TWrap::T, arrow::BooleanType>;
            case EAggregate::Unspecified:
            case EAggregate::Count:
            case EAggregate::NumRows:
                return false;
        }
        return false;
    });
}

template <class TWrap>
bool MergeValue(const EAggregate aggregationType, typename TWrap::ValueType& accumulator, const typename TWrap::ValueType value) {
    switch (aggregationType) {
        case EAggregate::Some:
            return true;
        case EAggregate::Sum:
            if constexpr (TWrap::IsCType && !std::is_same_v<typename TWrap::T, arrow::BooleanType>) {
                accumulator += value;
                return true;
            } else {
                return false;
            }
        case EAggregate::Min:
        case EAggregate::Max:
            if constexpr (std::is_floating_point_v<typename TWrap::ValueType>) {
                // NaN is skipped as fmin and fmax do.
                if (std::isnan(value)) {
                    return true;
                }
                if (std::isnan(accumulator)) {
                    accumulator = value;
                    return true;
                }
            }
            if (aggregationType == EAggregate::Min ? value < accumulator : accumulator < value) {
                accumulator = value;
            }
            return true;
        case EAggregate::Unspecified:
        case EAggregate::Count:
        case EAggregate::NumRows:
            return false;
    }
    return false;
}

TConclusion<std::shared_ptr<arrow::Scalar>> GetResultScalar(const TAccessorsCollection& source, const ui32 columnId) {
    if (const auto& scalar = source.GetConstantScalarOptional(columnId)) {
        return scalar;
    }
    const auto& accessor = source.GetAccessorOptional(columnId);
    if (!accessor) {
        return TConclusionStatus::Fail("no aggregation result column " + ::ToString(columnId));
    }
    if (accessor->GetRecordsCount() != 1) {
        return TConclusionStatus::Fail(
            "aggregation result column " + ::ToString(columnId) + " has " + ::ToString(accessor->GetRecordsCount()) + " records");
    }
    return accessor->GetScalar(0);
}

class TResultsAggregator: public IResourcesAggregator {
private:
    const TColumnChainInfo ColumnInfo;
    const EAggregate AggregationType;

    virtual void DoFillResultColumnIds(THashSet<ui32>& columnIds) const override {
        columnIds.emplace(ColumnInfo.GetColumnId());
    }

    virtual TConclusionStatus DoCheckSource(const TAccessorsCollection& source, const TAccessorsCollection* sample) const override {
        auto scalar = GetResultScalar(source, ColumnInfo.GetColumnId());
        if (scalar.IsFail()) {
            return scalar;
        }
        const auto& dataType = *scalar.GetResult()->type;
        if (!IsMergeableValueType(dataType, AggregationType)) {
            return TConclusionStatus::Fail("cannot merge " + ::ToString(AggregationType) + " results of type " + dataType.ToString());
        }
        if (sample) {
            auto sampleScalar = GetResultScalar(*sample, ColumnInfo.GetColumnId());
            if (sampleScalar.IsFail()) {
                return sampleScalar;
            }
            if (!sampleScalar.GetResult()->type->Equals(dataType)) {
                return TConclusionStatus::Fail("different types of aggregation results: " + dataType.ToString() + " and " +
                                               sampleScalar.GetResult()->type->ToString());
            }
        }
        return TConclusionStatus::Success();
    }

    virtual TConclusionStatus DoExecute(
        const std::vector<std::unique_ptr<TAccessorsCollection>>& sources, TAccessorsCollection& collectionResult) const override {
        AFL_VERIFY(sources.size());
        std::vector<std::shared_ptr<arrow::Scalar>> scalars;
        for (auto&& i : sources) {
            AFL_VERIFY(i);
            auto status = CheckSource(*i, sources.front().get());
            if (status.IsFail()) {
                return status;
            }
            scalars.emplace_back(GetResultScalar(*i, ColumnInfo.GetColumnId()).DetachResult());
        }
        const auto dataType = scalars.front()->type;
        std::shared_ptr<arrow::Scalar> result;
        TString errorMessage;
        if (!NArrow::SwitchType(dataType->id(), [&](const auto& type) {
                using TWrap = std::decay_t<decltype(type)>;
                if constexpr (!TWrap::IsAppropriate) {
                    return false;
                } else {
                    using TScalarType = typename TWrap::TScalar;
                    std::optional<typename TWrap::ValueType> value;
                    for (auto&& i : scalars) {
                        if (!i->is_valid) {
                            continue;
                        }
                        const typename TWrap::ValueType current = type.GetValue(*static_cast<const TScalarType*>(i.get()));
                        if (!value) {
                            value = current;
                        } else if (!MergeValue<TWrap>(AggregationType, *value, current)) {
                            errorMessage = "cannot merge " + ::ToString(AggregationType) + " results of type " + dataType->ToString();
                            return false;
                        }
                    }
                    if (value) {
                        result = type.BuildScalar(*value, dataType);
                    } else {
                        result = std::make_shared<TScalarType>(dataType);
                    }
                    return true;
                }
            })) {
            return TConclusionStatus::Fail(errorMessage.empty() ? TString("cannot merge results of type ") + dataType->ToString() : errorMessage);
        }
        collectionResult.AddCalculated(ColumnInfo.GetColumnId(), NAccessor::TTrivialArray::BuildArrayFromScalar(result));
        collectionResult.TakeSequenceFrom(*sources.front());
        return TConclusionStatus::Success();
    }

public:
    TResultsAggregator(const TColumnChainInfo& column, const EAggregate aggrType)
        : ColumnInfo(column)
        , AggregationType(aggrType) {
    }
};

}   // namespace

std::shared_ptr<IResourcesAggregator> TAggregateFunction::BuildResultsAggregator(const TColumnChainInfo& output) const {
    if (GetFunctionOptions()) {
        return nullptr;
    }
    return std::make_shared<TResultsAggregator>(output, TAggregationsHelper::GetSecondaryAggregationId(AggregationType));
}

namespace {
// Group key of a row in a per-source aggregation result. Unlike TReplaceKeyHashable it tells NULL from any value.
class TMergeKey {
private:
    const std::vector<std::shared_ptr<arrow::Array>>* Columns = nullptr;
    ui32 Position = 0;
    ui64 Hash = 0;

    template <class TFunc>
    static void ApplyToValue(const arrow::Array& column, TFunc&& func) {
        AFL_VERIFY(NArrow::SwitchType(column.type_id(), [&](const auto& type) {
            using TWrap = std::decay_t<decltype(type)>;
            if constexpr (TWrap::IsAppropriate) {
                func(type, *type.CastArray(&column));
                return true;
            } else {
                return false;
            }
        }));
    }

    ui64 CalcHash() const {
        ui64 result = 0;
        for (auto&& column : *Columns) {
            if (column->IsNull(Position)) {
                result = CombineHashes<ui64>(result, 0);
                continue;
            }
            ApplyToValue(*column, [&](const auto& type, const auto& array) {
                result = CombineHashes<ui64>(result, type.CalcHash(type.GetValue(array, Position)));
            });
        }
        return result;
    }

public:
    struct THash {
        size_t operator()(const TMergeKey& key) const {
            return key.Hash;
        }
    };

    TMergeKey() = default;

    TMergeKey(const std::vector<std::shared_ptr<arrow::Array>>& columns, const ui32 position)
        : Columns(&columns)
        , Position(position)
        , Hash(CalcHash()) {
    }

    bool operator==(const TMergeKey& other) const {
        if (Hash != other.Hash) {
            return false;
        }
        AFL_VERIFY(Columns->size() == other.Columns->size());
        for (ui32 i = 0; i < Columns->size(); ++i) {
            const arrow::Array& column = *(*Columns)[i];
            const arrow::Array& otherColumn = *(*other.Columns)[i];
            const bool isNull = column.IsNull(Position);
            if (isNull != otherColumn.IsNull(other.Position)) {
                return false;
            }
            if (isNull) {
                continue;
            }
            bool equal = false;
            ApplyToValue(column, [&](const auto& type, const auto& array) {
                using TArray = std::decay_t<decltype(array)>;
                equal = type.GetValue(array, Position) == type.GetValue(static_cast<const TArray&>(otherColumn), other.Position);
            });
            if (!equal) {
                return false;
            }
        }
        return true;
    }
};

class TResultsAggregatorWithKeys: public IResourcesAggregator {
public:
    class TColumnAggregationInfo {
    private:
        const TColumnChainInfo ColumnInfo;
        const EAggregate AggregationType;

    public:
        const TColumnChainInfo& GetColumnInfo() const {
            return ColumnInfo;
        }
        EAggregate GetAggregationType() const {
            return AggregationType;
        }

        TColumnAggregationInfo(const TColumnChainInfo columnInfo, const EAggregate aggregationType)
            : ColumnInfo(columnInfo)
            , AggregationType(aggregationType) {
        }
    };

private:
    const std::vector<TColumnChainInfo> KeyColumns;
    const std::vector<TColumnAggregationInfo> Aggregations;

    virtual void DoFillResultColumnIds(THashSet<ui32>& columnIds) const override {
        for (auto&& i : KeyColumns) {
            columnIds.emplace(i.GetColumnId());
        }
        for (auto&& i : Aggregations) {
            columnIds.emplace(i.GetColumnInfo().GetColumnId());
        }
    }

    virtual TConclusionStatus DoCheckSource(const TAccessorsCollection& source, const TAccessorsCollection* sample) const override {
        std::optional<ui32> recordsCount;
        const auto checkColumn = [&](const ui32 columnId, const std::optional<EAggregate> aggregationType) {
            const auto& accessor = source.GetAccessorOptional(columnId);
            if (!accessor) {
                return TConclusionStatus::Fail("no aggregation result column " + ::ToString(columnId));
            }
            if (!recordsCount) {
                recordsCount = accessor->GetRecordsCount();
            } else if (*recordsCount != accessor->GetRecordsCount()) {
                return TConclusionStatus::Fail("different records count in aggregation result columns");
            }
            const auto& dataType = *accessor->GetDataType();
            if (aggregationType ? !IsMergeableValueType(dataType, *aggregationType) : !IsMergeableKeyType(dataType)) {
                return TConclusionStatus::Fail("cannot merge aggregation results of type " + dataType.ToString());
            }
            if (sample) {
                const auto& sampleAccessor = sample->GetAccessorOptional(columnId);
                if (!sampleAccessor || !sampleAccessor->GetDataType()->Equals(dataType)) {
                    return TConclusionStatus::Fail("different types of aggregation results in column " + ::ToString(columnId));
                }
            }
            return TConclusionStatus::Success();
        };
        for (auto&& i : KeyColumns) {
            auto status = checkColumn(i.GetColumnId(), std::nullopt);
            if (status.IsFail()) {
                return status;
            }
        }
        for (auto&& i : Aggregations) {
            auto status = checkColumn(i.GetColumnInfo().GetColumnId(), i.GetAggregationType());
            if (status.IsFail()) {
                return status;
            }
        }
        return TConclusionStatus::Success();
    }

    TConclusion<std::shared_ptr<arrow::Array>> BuildColumn(const ui32 keysCount, const TColumnAggregationInfo& aggr,
        const std::vector<std::unique_ptr<TAccessorsCollection>>& sources, const std::vector<std::vector<ui32>>& decoder) const {
        std::vector<std::shared_ptr<arrow::Array>> arrays;
        for (ui32 sourceIdx = 0; sourceIdx < sources.size(); ++sourceIdx) {
            arrays.emplace_back(sources[sourceIdx]->GetArrayVerified(aggr.GetColumnInfo().GetColumnId()));
            AFL_VERIFY((ui32)arrays.back()->length() == decoder[sourceIdx].size())("count", arrays.back()->length());
        }
        const auto dataType = arrays.front()->type();
        std::shared_ptr<arrow::Array> arrResult;
        TString errorMessage;
        if (!NArrow::SwitchType(dataType->id(), [&](const auto& type) {
                using TWrap = std::decay_t<decltype(type)>;
                if constexpr (!TWrap::IsAppropriate) {
                    return false;
                } else {
                    using TArrayType = typename TWrap::TArray;
                    std::vector<std::optional<typename TWrap::ValueType>> result;
                    result.resize(keysCount);
                    for (ui32 sourceIdx = 0; sourceIdx < arrays.size(); ++sourceIdx) {
                        const auto& arrWithType = *static_cast<const TArrayType*>(arrays[sourceIdx].get());
                        const auto& sourceDecoder = decoder[sourceIdx];
                        for (ui32 decoderIdx = 0; decoderIdx < sourceDecoder.size(); ++decoderIdx) {
                            if (arrWithType.IsNull(decoderIdx)) {
                                continue;
                            }
                            const typename TWrap::ValueType value = type.GetValue(arrWithType, decoderIdx);
                            auto& accumulator = result[sourceDecoder[decoderIdx]];
                            if (!accumulator) {
                                accumulator = value;
                            } else if (!MergeValue<TWrap>(aggr.GetAggregationType(), *accumulator, value)) {
                                errorMessage =
                                    "cannot merge " + ::ToString(aggr.GetAggregationType()) + " results of type " + dataType->ToString();
                                return false;
                            }
                        }
                    }
                    auto builder = NArrow::MakeBuilder(dataType);
                    for (auto&& i : result) {
                        if (!i) {
                            TStatusValidator::Validate(builder->AppendNull());
                        } else {
                            type.AppendValue(*builder, *i);
                        }
                    }
                    arrResult = NArrow::FinishBuilder(std::move(builder));
                    return true;
                }
            })) {
            return TConclusionStatus::Fail(errorMessage.empty() ? TString("cannot merge results of type ") + dataType->ToString() : errorMessage);
        }
        return arrResult;
    }

    virtual TConclusionStatus DoExecute(
        const std::vector<std::unique_ptr<TAccessorsCollection>>& sources, TAccessorsCollection& collectionResult) const override {
        AFL_VERIFY(sources.size());
        ui32 reserveCount = 0;
        for (auto&& i : sources) {
            AFL_VERIFY(i);
            auto status = CheckSource(*i, sources.front().get());
            if (status.IsFail()) {
                return status;
            }
            reserveCount += i->GetRecordsCountActualVerified();
        }
        ui64 countCapacity = 4096;
        while (countCapacity < 3 * reserveCount) {
            countCapacity *= 2;
            AFL_VERIFY(countCapacity < (1llu << 30));
        }
        std::deque<std::vector<std::shared_ptr<arrow::Array>>> arraysStorage;
        std::vector<std::vector<ui32>> decoder;
        decoder.resize(sources.size());
        std::vector<std::unique_ptr<arrow::ArrayBuilder>> keyBuilders;
        for (auto&& c : KeyColumns) {
            keyBuilders.emplace_back(NArrow::MakeBuilder(sources.front()->GetAccessorVerified(c.GetColumnId())->GetDataType()));
        }
        ui32 keysCount = 0;
        {
            NMiniKQL::TRobinHoodHashFixedMap<TMergeKey, ui32, std::equal_to<TMergeKey>, TMergeKey::THash> keyToIndex(countCapacity);
            for (ui32 sourceIdx = 0; sourceIdx < sources.size(); ++sourceIdx) {
                auto& keyArrays = arraysStorage.emplace_back();
                for (auto&& c : KeyColumns) {
                    keyArrays.emplace_back(sources[sourceIdx]->GetArrayVerified(c.GetColumnId()));
                    AFL_VERIFY(keyArrays.front()->length() == keyArrays.back()->length());
                }
                const ui32 count = keyArrays.front()->length();
                decoder[sourceIdx].resize(count);
                for (ui32 position = 0; position < count; ++position) {
                    bool isNew;
                    auto* it = keyToIndex.Insert(TMergeKey(keyArrays, position), isNew);
                    if (isNew) {
                        keyToIndex.RestorePayload(keyToIndex.GetMutablePayloadPtr(it), keysCount);
                        ++keysCount;
                        for (ui32 idx = 0; idx < keyArrays.size(); ++idx) {
                            AFL_VERIFY(NArrow::Append(*keyBuilders[idx], *keyArrays[idx], position));
                        }
                    }
                    keyToIndex.SavePayload(keyToIndex.GetPayloadPtr(it), decoder[sourceIdx][position]);
                    if (isNew) {
                        keyToIndex.CheckGrow();
                    }
                }
            }
        }
        for (auto&& a : Aggregations) {
            auto conclusion = BuildColumn(keysCount, a, sources, decoder);
            if (conclusion.IsFail()) {
                return conclusion;
            }
            collectionResult.AddCalculated(a.GetColumnInfo().GetColumnId(), conclusion.DetachResult());
        }
        {
            ui32 idx = 0;
            for (auto&& k : KeyColumns) {
                collectionResult.AddCalculated(k.GetColumnId(), NArrow::FinishBuilder(std::move(keyBuilders[idx])));
                ++idx;
            }
        }
        collectionResult.TakeSequenceFrom(*sources.front());
        return TConclusionStatus::Success();
    }

public:
    TResultsAggregatorWithKeys(std::vector<TColumnChainInfo>&& keyColumns, std::vector<TColumnAggregationInfo>&& aggregations)
        : KeyColumns(std::move(keyColumns))
        , Aggregations(std::move(aggregations)) {
        AFL_VERIFY(KeyColumns.size());
    }
};
}   // namespace

std::shared_ptr<IResourcesAggregator> TWithKeysAggregationProcessor::BuildResultsAggregator() const {
    std::vector<TColumnChainInfo> input = AggregationKeys;
    std::vector<TResultsAggregatorWithKeys::TColumnAggregationInfo> aggrOptions;
    for (auto&& i : Aggregations) {
        aggrOptions.emplace_back(i.GetOutput(), i.GetSecondaryAggregationId());
    }
    return std::make_shared<TResultsAggregatorWithKeys>(std::move(input), std::move(aggrOptions));
}

}   // namespace NKikimr::NArrow::NSSA::NAggregation
