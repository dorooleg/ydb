#include <ydb/library/yql/dq/runtime/dq_async_input.h>
#include <ydb/library/yql/dq/runtime/dq_columns_resolve.h>
#include <ydb/library/yql/dq/runtime/dq_input_producer.h>

#include <contrib/libs/apache/arrow/cpp/src/arrow/array/array_primitive.h>
#include <contrib/libs/apache/arrow/cpp/src/arrow/array/builder_primitive.h>
#include <contrib/libs/apache/arrow/cpp/src/arrow/scalar.h>

#include <library/cpp/testing/unittest/registar.h>

#include <util/generic/size_literals.h>

#include <yql/essentials/minikql/computation/mkql_computation_node_holders.h>
#include <yql/essentials/minikql/mkql_node.h>
#include <yql/essentials/minikql/mkql_type_builder.h>
#include <yql/essentials/public/udf/udf_value.h>

using namespace NKikimr::NMiniKQL;
using namespace NYql;
using namespace NYql::NDq;

namespace {

struct TTestContext {
    TScopedAlloc Alloc;
    TTypeEnvironment TypeEnv;
    TMemoryUsageInfo MemInfo;
    THolderFactory HolderFactory;
    TMultiType* InputType = nullptr;

    TTestContext()
        : Alloc(__LOCATION__)
        , TypeEnv(Alloc)
        , MemInfo("Mem")
        , HolderFactory(Alloc.Ref(), MemInfo)
    {
        TType* elements[2] = {
            TBlockType::Create(TDataType::Create(NUdf::TDataType<ui64>::Id, TypeEnv), TBlockType::EShape::Many, TypeEnv),
            TBlockType::Create(TDataType::Create(NUdf::TDataType<ui64>::Id, TypeEnv), TBlockType::EShape::Scalar, TypeEnv),
        };
        InputType = TMultiType::Create(2, elements, TypeEnv);
    }

    TUnboxedValueBatch MakeKeyBlock(ui64 key) {
        arrow::UInt64Builder builder;
        UNIT_ASSERT(builder.Append(key).ok());
        std::shared_ptr<arrow::Array> array;
        UNIT_ASSERT(builder.Finish(&array).ok());

        NUdf::TUnboxedValue values[2];
        values[0] = HolderFactory.CreateArrowBlock(arrow::Datum(array), DefaultDatumTestValidationMode);
        values[1] = HolderFactory.CreateArrowBlock(
            arrow::Datum(std::make_shared<arrow::UInt64Scalar>(1)), DefaultDatumTestValidationMode);

        TUnboxedValueBatch batch(InputType);
        batch.PushRow(values, 2);
        return batch;
    }

    IDqAsyncInputBuffer::TPtr MakeInput(ui64 index) {
        return CreateDqAsyncInputBuffer(index, "test", InputType, 1_MB, TCollectStatsLevel::None);
    }

    TVector<TSortColumnInfo> MakeSortCols() {
        TSortColumnInfo col(GetColumnInfo(InputType, "0"));
        col.Ascending = true;
        TVector<TSortColumnInfo> sortCols;
        sortCols.push_back(std::move(col));
        return sortCols;
    }

    NUdf::TUnboxedValue MakeMerge(TVector<IDqInput::TPtr>&& inputs, TInstant& startTs, ui64& inputsConsumed) {
        return CreateInputMergeValue(
            InputType,
            std::move(inputs),
            MakeSortCols(),
            HolderFactory,
            TDqMeteringStats::TInputStatsMeter(),
            startTs,
            inputsConsumed);
    }

    static ui64 ReadKey(const NUdf::TUnboxedValue& value) {
        const auto& datum = TArrowBlock::From(value).GetDatum();
        UNIT_ASSERT(datum.is_array());
        const auto array = datum.array_as<arrow::UInt64Array>();
        UNIT_ASSERT_VALUES_EQUAL(array->length(), 1);
        return array->Value(0);
    }

    static ui64 ReadBlockLen(const NUdf::TUnboxedValue& value) {
        return TArrowBlock::From(value).GetDatum().scalar_as<arrow::UInt64Scalar>().value;
    }
};

} // namespace

Y_UNIT_TEST_SUITE(TDqInputMergeBlockStream) {

Y_UNIT_TEST(EmitsPartialBlockWhenProducerYields) {
    TTestContext ctx;
    auto input0 = ctx.MakeInput(0);
    auto input1 = ctx.MakeInput(1);

    input0->Push(ctx.MakeKeyBlock(1), 8);
    input1->Push(ctx.MakeKeyBlock(10), 8);

    TVector<IDqInput::TPtr> inputs;
    inputs.emplace_back(input0);
    inputs.emplace_back(input1);

    TInstant startTs;
    ui64 inputsConsumed = 0;
    auto merge = ctx.MakeMerge(std::move(inputs), startTs, inputsConsumed);

    NUdf::TUnboxedValue out[2];
    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Ok);
    UNIT_ASSERT_VALUES_EQUAL(TTestContext::ReadBlockLen(out[1]), 1);
    UNIT_ASSERT_VALUES_EQUAL(TTestContext::ReadKey(out[0]), 1);

    // Waiting for the next chunk/Finish from the input that produced the last emitted row.
    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Yield);

    input0->Finish();
    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Ok);
    UNIT_ASSERT_VALUES_EQUAL(TTestContext::ReadBlockLen(out[1]), 1);
    UNIT_ASSERT_VALUES_EQUAL(TTestContext::ReadKey(out[0]), 10);

    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Yield);
    input1->Finish();
    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Finish);
}

Y_UNIT_TEST(EmitsGlobalMinRegardlessOfInputOrder) {
    TTestContext ctx;
    auto input0 = ctx.MakeInput(0);
    auto input1 = ctx.MakeInput(1);

    input0->Push(ctx.MakeKeyBlock(10), 8);
    input1->Push(ctx.MakeKeyBlock(1), 8);

    TVector<IDqInput::TPtr> inputs;
    inputs.emplace_back(input0);
    inputs.emplace_back(input1);

    TInstant startTs;
    ui64 inputsConsumed = 0;
    auto merge = ctx.MakeMerge(std::move(inputs), startTs, inputsConsumed);

    NUdf::TUnboxedValue out[2];
    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Ok);
    UNIT_ASSERT_VALUES_EQUAL(TTestContext::ReadKey(out[0]), 1);
}

Y_UNIT_TEST(YieldsWhenNoData) {
    TTestContext ctx;
    auto input0 = ctx.MakeInput(0);
    auto input1 = ctx.MakeInput(1);

    TVector<IDqInput::TPtr> inputs;
    inputs.emplace_back(input0);
    inputs.emplace_back(input1);

    TInstant startTs;
    ui64 inputsConsumed = 0;
    auto merge = ctx.MakeMerge(std::move(inputs), startTs, inputsConsumed);

    NUdf::TUnboxedValue out[2];
    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Yield);
}

Y_UNIT_TEST(MergesFinishedInputsIntoOneBlock) {
    TTestContext ctx;
    auto input0 = ctx.MakeInput(0);
    auto input1 = ctx.MakeInput(1);

    input0->Push(ctx.MakeKeyBlock(1), 8);
    input0->Finish();
    input1->Push(ctx.MakeKeyBlock(10), 8);
    input1->Finish();

    TVector<IDqInput::TPtr> inputs;
    inputs.emplace_back(input0);
    inputs.emplace_back(input1);

    TInstant startTs;
    ui64 inputsConsumed = 0;
    auto merge = ctx.MakeMerge(std::move(inputs), startTs, inputsConsumed);

    NUdf::TUnboxedValue out[2];
    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Ok);
    UNIT_ASSERT_VALUES_EQUAL(TTestContext::ReadBlockLen(out[1]), 2);

    const auto& datum = TArrowBlock::From(out[0]).GetDatum();
    UNIT_ASSERT(datum.is_array());
    const auto array = datum.array_as<arrow::UInt64Array>();
    UNIT_ASSERT_VALUES_EQUAL(array->length(), 2);
    UNIT_ASSERT_VALUES_EQUAL(array->Value(0), 1);
    UNIT_ASSERT_VALUES_EQUAL(array->Value(1), 10);

    UNIT_ASSERT_EQUAL(merge.WideFetch(out, 2), NUdf::EFetchStatus::Finish);
}

} // Y_UNIT_TEST_SUITE(TDqInputMergeBlockStream)
