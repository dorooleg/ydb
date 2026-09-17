#include <ydb/core/tx/limiter/grouped_memory/service/counters.h>
#include <ydb/core/tx/limiter/grouped_memory/service/manager.h>
#include <ydb/core/tx/limiter/grouped_memory/usage/abstract.h>
#include <ydb/core/tx/limiter/grouped_memory/usage/config.h>
#include <ydb/core/tx/limiter/grouped_memory/usage/service.h>
#include <ydb/core/protos/config.pb.h>

#include <ydb/library/actors/core/log.h>

#include <library/cpp/monlib/dynamic_counters/counters.h>
#include <library/cpp/testing/unittest/registar.h>
#include <util/generic/object_counter.h>

#include <vector>

Y_UNIT_TEST_SUITE(GroupedMemoryLimiter) {
    using namespace NKikimr;

    class TAllocation: public NOlap::NGroupedMemoryManager::IAllocation, public TObjectCounter<TAllocation> {
    public:
        std::shared_ptr<NOlap::NGroupedMemoryManager::TAllocationGuard> Guard;
    private:
        using TBase = NOlap::NGroupedMemoryManager::IAllocation;
        virtual void DoOnAllocationImpossible(const TString& errorMessage) override {
            AFL_VERIFY(false)("error", errorMessage);
        }

        virtual bool DoOnAllocated(std::shared_ptr<NOlap::NGroupedMemoryManager::TAllocationGuard>&& guard,
            const std::shared_ptr<NOlap::NGroupedMemoryManager::IAllocation>& /*allocation*/) override {
            Guard = std::move(guard);
            return true;
        }

    public:
        TAllocation(const ui64 mem)
            : TBase(mem) {
        }
    };

    Y_UNIT_TEST(Simplest) {
        auto counters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "test");
        NOlap::NGroupedMemoryManager::TConfig config;
        {
            NKikimrConfig::TGroupedMemoryLimiterConfig protoConfig;
            protoConfig.SetMemoryLimit(100);
            protoConfig.SetUnrestrictedGroupsCount(1);
            UNIT_ASSERT(config.DeserializeFromProto(protoConfig));
        }
        std::unique_ptr<NActors::IActor> actor(
            NOlap::NGroupedMemoryManager::TScanMemoryLimiterOperator::CreateService(config, MakeIntrusive<NMonitoring::TDynamicCounters>()));
        auto groupedMemoryLimiterCounters = std::make_shared<NKikimr::NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "Scan");
        auto stage = std::make_shared<NKikimr::NOlap::NGroupedMemoryManager::TStageFeatures>("GLOBAL", config.GetMemoryLimit(), config.GetHardMemoryLimit(), nullptr, groupedMemoryLimiterCounters->BuildStageCounters("general"));
        auto manager = std::make_shared<NOlap::NGroupedMemoryManager::TManager>(NActors::TActorId(), config, "test", counters, stage);
        {
            auto alloc1 = std::make_shared<TAllocation>(50);
            manager->RegisterProcess(0, {});
            manager->RegisterProcessScope(0, 0);
            manager->RegisterGroup(0, 0, 1);
            manager->RegisterAllocation(0, 0, 1, alloc1, {});
            UNIT_ASSERT(alloc1->IsAllocated());
            auto alloc1_1 = std::make_shared<TAllocation>(50);
            manager->RegisterAllocation(0, 0, 1, alloc1_1, {});
            UNIT_ASSERT(alloc1_1->IsAllocated());

            manager->RegisterGroup(0, 0, 2);
            auto alloc2 = std::make_shared<TAllocation>(50);
            manager->RegisterAllocation(0, 0, 2, alloc2, {});
            UNIT_ASSERT(!alloc2->IsAllocated());
            alloc1->Guard.reset();
            manager->UnregisterAllocation(0, 0, alloc1->GetIdentifier());

            UNIT_ASSERT(alloc2->IsAllocated());
            manager->UnregisterAllocation(0, 0, alloc2->GetIdentifier());
            manager->UnregisterAllocation(0, 0, alloc1_1->GetIdentifier());
            manager->UnregisterGroup(0, 0, 1);
            manager->UnregisterGroup(0, 0, 2);
            manager->UnregisterProcessScope(0, 0);
            manager->UnregisterProcess(0);
        }
        UNIT_ASSERT_VALUES_EQUAL(stage->GetUsage().Val(), 0);
        UNIT_ASSERT(manager->IsEmpty());
        UNIT_ASSERT_VALUES_EQUAL(TObjectCounter<TAllocation>::ObjectCount(), 0);
    }

    Y_UNIT_TEST(Simple) {
        auto counters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "test");
        NOlap::NGroupedMemoryManager::TConfig config;
        {
            NKikimrConfig::TGroupedMemoryLimiterConfig protoConfig;
            protoConfig.SetMemoryLimit(100);
            protoConfig.SetUnrestrictedGroupsCount(1);
            UNIT_ASSERT(config.DeserializeFromProto(protoConfig));
        }
        std::unique_ptr<NActors::IActor> actor(NOlap::NGroupedMemoryManager::TScanMemoryLimiterOperator::CreateService(config, MakeIntrusive<NMonitoring::TDynamicCounters>()));
        auto groupedMemoryLimiterCounters = std::make_shared<NKikimr::NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "Scan");
        auto stage = std::make_shared<NKikimr::NOlap::NGroupedMemoryManager::TStageFeatures>("GLOBAL", config.GetMemoryLimit(), config.GetHardMemoryLimit(), nullptr, groupedMemoryLimiterCounters->BuildStageCounters("general"));
        auto manager = std::make_shared<NOlap::NGroupedMemoryManager::TManager>(NActors::TActorId(), config, "test", counters, stage);
        {
            manager->RegisterProcess(0, {});
            manager->RegisterProcessScope(0, 0);
            auto alloc1 = std::make_shared<TAllocation>(10);
            manager->RegisterGroup(0, 0, 1);
            manager->RegisterAllocation(0, 0, 1, alloc1, {});
            UNIT_ASSERT(alloc1->IsAllocated());
            auto alloc2 = std::make_shared<TAllocation>(1000);
            manager->RegisterGroup(0, 0, 2);
            manager->RegisterAllocation(0, 0, 2, alloc2, {});
            UNIT_ASSERT(!alloc2->IsAllocated());
            auto alloc3 = std::make_shared<TAllocation>(1000);
            manager->RegisterGroup(0, 0, 3);
            manager->RegisterAllocation(0, 0, 3, alloc3, {});
            UNIT_ASSERT(alloc1->IsAllocated());
            UNIT_ASSERT(!alloc2->IsAllocated());
            UNIT_ASSERT(!alloc3->IsAllocated());
            auto alloc1_1 = std::make_shared<TAllocation>(1000);
            manager->RegisterAllocation(0, 0, 1, alloc1_1, {});
            UNIT_ASSERT(alloc1_1->IsAllocated());
            UNIT_ASSERT(!alloc2->IsAllocated());
            alloc1_1->ResetAllocation();
            manager->UnregisterAllocation(0, 0, alloc1_1->GetIdentifier());
            UNIT_ASSERT(!alloc2->IsAllocated());
            manager->UnregisterGroup(0, 0, 1);
            UNIT_ASSERT(alloc2->IsAllocated());

            manager->UnregisterAllocation(0, 0, alloc1->GetIdentifier());
            UNIT_ASSERT(!alloc3->IsAllocated());
            manager->UnregisterGroup(0, 0, 2);
            manager->UnregisterAllocation(0, 0, alloc2->GetIdentifier());
            UNIT_ASSERT(alloc3->IsAllocated());
            manager->UnregisterGroup(0, 0, 3);
            manager->UnregisterAllocation(0, 0, alloc3->GetIdentifier());
            manager->UnregisterProcessScope(0, 0);
            manager->UnregisterProcess(0);
        }
        UNIT_ASSERT_VALUES_EQUAL(stage->GetUsage().Val(), 0);
        UNIT_ASSERT(manager->IsEmpty());
        UNIT_ASSERT_VALUES_EQUAL(TObjectCounter<TAllocation>::ObjectCount(), 0);
    }

    Y_UNIT_TEST(CommonUsage) {
        auto counters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "test");
        NOlap::NGroupedMemoryManager::TConfig config;
        {
            NKikimrConfig::TGroupedMemoryLimiterConfig protoConfig;
            protoConfig.SetMemoryLimit(100);
            protoConfig.SetUnrestrictedGroupsCount(1);
            UNIT_ASSERT(config.DeserializeFromProto(protoConfig));
        }
        std::unique_ptr<NActors::IActor> actor(
            NOlap::NGroupedMemoryManager::TScanMemoryLimiterOperator::CreateService(config, MakeIntrusive<NMonitoring::TDynamicCounters>()));
        auto groupedMemoryLimiterCounters = std::make_shared<NKikimr::NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "Scan");
        auto stage = std::make_shared<NKikimr::NOlap::NGroupedMemoryManager::TStageFeatures>("GLOBAL", config.GetMemoryLimit(), config.GetHardMemoryLimit(), nullptr, groupedMemoryLimiterCounters->BuildStageCounters("general"));
        auto manager = std::make_shared<NOlap::NGroupedMemoryManager::TManager>(NActors::TActorId(), config, "test", counters, stage);
        {
            manager->RegisterProcess(0, {});
            manager->RegisterProcessScope(0, 0);
            manager->RegisterGroup(0, 0, 1);
            auto alloc0 = std::make_shared<TAllocation>(1000);
            manager->RegisterAllocation(0, 0, 1, alloc0, {});
            auto alloc1 = std::make_shared<TAllocation>(1000);
            manager->RegisterAllocation(0, 0, 1, alloc1, {});
            UNIT_ASSERT(alloc0->IsAllocated());
            UNIT_ASSERT(alloc1->IsAllocated());

            manager->RegisterGroup(0, 0, 2);
            auto alloc2 = std::make_shared<TAllocation>(1000);
            manager->RegisterAllocation(0, 0, 2, alloc0, {});
            manager->RegisterAllocation(0, 0, 2, alloc2, {});
            UNIT_ASSERT(alloc0->IsAllocated());
            UNIT_ASSERT(!alloc2->IsAllocated());

            auto alloc3 = std::make_shared<TAllocation>(1000);
            manager->RegisterGroup(0, 0, 3);
            manager->RegisterAllocation(0, 0, 3, alloc0, {});
            manager->RegisterAllocation(0, 0, 3, alloc3, {});
            UNIT_ASSERT(alloc0->IsAllocated());
            UNIT_ASSERT(alloc1->IsAllocated());
            UNIT_ASSERT(!alloc2->IsAllocated());
            UNIT_ASSERT(!alloc3->IsAllocated());

            manager->UnregisterGroup(0, 0, 1);
            manager->UnregisterAllocation(0, 0, alloc1->GetIdentifier());

            UNIT_ASSERT(alloc0->IsAllocated());
            UNIT_ASSERT(alloc2->IsAllocated());
            UNIT_ASSERT(!alloc3->IsAllocated());
            manager->UnregisterGroup(0, 0, 2);
            manager->UnregisterAllocation(0, 0, alloc2->GetIdentifier());
            UNIT_ASSERT(alloc0->IsAllocated());
            UNIT_ASSERT(alloc3->IsAllocated());

            manager->UnregisterGroup(0, 0, 3);
            manager->UnregisterAllocation(0, 0, alloc3->GetIdentifier());
            manager->UnregisterAllocation(0, 0, alloc0->GetIdentifier());
            manager->UnregisterProcess(0);
        }
        UNIT_ASSERT_VALUES_EQUAL(stage->GetUsage().Val(), 0);
        UNIT_ASSERT(manager->IsEmpty());
        UNIT_ASSERT_VALUES_EQUAL(TObjectCounter<TAllocation>::ObjectCount(), 0);
    }

    Y_UNIT_TEST(Update) {
        auto counters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "test");
        NOlap::NGroupedMemoryManager::TConfig config;
        {
            NKikimrConfig::TGroupedMemoryLimiterConfig protoConfig;
            protoConfig.SetMemoryLimit(100);
            protoConfig.SetUnrestrictedGroupsCount(1);
            UNIT_ASSERT(config.DeserializeFromProto(protoConfig));
        }
        std::unique_ptr<NActors::IActor> actor(
            NOlap::NGroupedMemoryManager::TScanMemoryLimiterOperator::CreateService(config, MakeIntrusive<NMonitoring::TDynamicCounters>()));
        auto groupedMemoryLimiterCounters = std::make_shared<NKikimr::NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "Scan");
        auto stage = std::make_shared<NKikimr::NOlap::NGroupedMemoryManager::TStageFeatures>("GLOBAL", config.GetMemoryLimit(), config.GetHardMemoryLimit(), nullptr, groupedMemoryLimiterCounters->BuildStageCounters("general"));
        auto manager = std::make_shared<NOlap::NGroupedMemoryManager::TManager>(NActors::TActorId(), config, "test", counters, stage);
        {
            manager->RegisterProcess(0, {});
            manager->RegisterProcessScope(0, 0);
            auto alloc1 = std::make_shared<TAllocation>(1000);
            manager->RegisterGroup(0, 0, 1);
            manager->RegisterAllocation(0, 0, 1, alloc1, {});
            UNIT_ASSERT(alloc1->IsAllocated());
            auto alloc2 = std::make_shared<TAllocation>(10);
            manager->RegisterGroup(0, 0, 3);
            manager->RegisterAllocation(0, 0, 3, alloc2, {});
            UNIT_ASSERT(!alloc2->IsAllocated());

            alloc1->Guard->Update(10);
            manager->AllocationUpdated(0, 0, alloc1->GetIdentifier());
            UNIT_ASSERT(alloc2->IsAllocated());

            manager->UnregisterGroup(0, 0, 3);
            manager->UnregisterAllocation(0, 0, alloc2->GetIdentifier());

            manager->UnregisterGroup(0, 0, 1);
            manager->UnregisterAllocation(0, 0, alloc1->GetIdentifier());
            manager->UnregisterProcess(0);
        }
        UNIT_ASSERT_VALUES_EQUAL(stage->GetUsage().Val(), 0);
        UNIT_ASSERT(manager->IsEmpty());
        UNIT_ASSERT_VALUES_EQUAL(TObjectCounter<TAllocation>::ObjectCount(), 0);
    }

    Y_UNIT_TEST(UnregisterScopeKeepsWaitingWhenScopeHasLinks) {
        auto groupedMemoryLimiterCounters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(
            MakeIntrusive<NMonitoring::TDynamicCounters>(), "Scan");
        auto stage = std::make_shared<NOlap::NGroupedMemoryManager::TStageFeatures>("GLOBAL", 100, std::nullopt, nullptr,
            groupedMemoryLimiterCounters->BuildStageCounters("general"));
        NOlap::NGroupedMemoryManager::TProcessMemory process(0, 1, NActors::TActorId(), true, {}, stage, 1);

        process.RegisterScope(0);
        process.RegisterScope(0);

        process.RegisterGroup(0, 1);
        auto alloc1 = std::make_shared<TAllocation>(100);
        process.RegisterAllocation(0, 1, alloc1, {});
        UNIT_ASSERT(alloc1->IsAllocated());

        process.RegisterGroup(0, 2);
        auto alloc2 = std::make_shared<TAllocation>(50);
        process.RegisterAllocation(0, 2, alloc2, {});
        UNIT_ASSERT(!alloc2->IsAllocated());
        UNIT_ASSERT(process.HasWaitingAllocations());

        process.UnregisterScope(0);
        UNIT_ASSERT(process.HasWaitingAllocations());

        alloc1->Guard.reset();
        UNIT_ASSERT(process.TryAllocateWaiting(1));
        UNIT_ASSERT(alloc2->IsAllocated());
        
        alloc2->Guard.reset();
        process.UnregisterAllocation(0, alloc1->GetIdentifier());
        process.UnregisterAllocation(0, alloc2->GetIdentifier());
        process.UnregisterGroup(0, 1);
        process.UnregisterGroup(0, 2);
        process.UnregisterScope(0);

        alloc1.reset();
        alloc2.reset();

        UNIT_ASSERT_VALUES_EQUAL(stage->GetUsage().Val(), 0);
        UNIT_ASSERT_VALUES_EQUAL(TObjectCounter<TAllocation>::ObjectCount(), 0);
    }

    class TFallibleAllocation: public NOlap::NGroupedMemoryManager::IAllocation {
    public:
        std::shared_ptr<NOlap::NGroupedMemoryManager::TAllocationGuard> Guard;
        TString Error;

    private:
        using TBase = NOlap::NGroupedMemoryManager::IAllocation;
        virtual void DoOnAllocationImpossible(const TString& errorMessage) override {
            Error = errorMessage;
        }

        virtual bool DoOnAllocated(std::shared_ptr<NOlap::NGroupedMemoryManager::TAllocationGuard>&& guard,
            const std::shared_ptr<NOlap::NGroupedMemoryManager::IAllocation>& /*allocation*/) override {
            Guard = std::move(guard);
            return true;
        }

    public:
        TFallibleAllocation(const ui64 mem)
            : TBase(mem) {
        }
    };

    Y_UNIT_TEST(UnrestrictedWindowAllocatesFirst16AndTransfersSlot) {
        auto counters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "test");
        NOlap::NGroupedMemoryManager::TConfig config;
        {
            NKikimrConfig::TGroupedMemoryLimiterConfig protoConfig;
            protoConfig.SetMemoryLimit(1);
            protoConfig.SetUnrestrictedGroupsCount(16);
            UNIT_ASSERT(config.DeserializeFromProto(protoConfig));
        }
        auto groupedMemoryLimiterCounters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(
            MakeIntrusive<NMonitoring::TDynamicCounters>(), "Scan");
        auto stage = std::make_shared<NOlap::NGroupedMemoryManager::TStageFeatures>(
            "GLOBAL", config.GetMemoryLimit(), config.GetHardMemoryLimit(), nullptr, groupedMemoryLimiterCounters->BuildStageCounters("general"));
        auto manager = std::make_shared<NOlap::NGroupedMemoryManager::TManager>(NActors::TActorId(), config, "test", counters, stage);

        manager->RegisterProcess(0, {});
        manager->RegisterProcessScope(0, 0);

        std::vector<std::shared_ptr<TAllocation>> allocations;
        allocations.reserve(17);
        for (ui64 groupId = 1; groupId <= 17; ++groupId) {
            manager->RegisterGroup(0, 0, groupId);
            auto alloc = std::make_shared<TAllocation>(100);
            manager->RegisterAllocation(0, 0, groupId, alloc, {});
            allocations.emplace_back(std::move(alloc));
        }

        for (ui32 i = 0; i < 16; ++i) {
            UNIT_ASSERT_C(allocations[i]->IsAllocated(), i);
        }
        UNIT_ASSERT(!allocations[16]->IsAllocated());

        allocations[0]->Guard.reset();
        manager->UnregisterAllocation(0, 0, allocations[0]->GetIdentifier());
        manager->UnregisterGroup(0, 0, 1);

        UNIT_ASSERT(allocations[16]->IsAllocated());

        for (ui32 i = 1; i < allocations.size(); ++i) {
            if (allocations[i]->Guard) {
                allocations[i]->Guard.reset();
            }
            manager->UnregisterAllocation(0, 0, allocations[i]->GetIdentifier());
            manager->UnregisterGroup(0, 0, i + 1);
        }
        manager->UnregisterProcessScope(0, 0);
        manager->UnregisterProcess(0);
        allocations.clear();

        UNIT_ASSERT_VALUES_EQUAL(stage->GetUsage().Val(), 0);
        UNIT_ASSERT(manager->IsEmpty());
        UNIT_ASSERT_VALUES_EQUAL(TObjectCounter<TAllocation>::ObjectCount(), 0);
    }

    Y_UNIT_TEST(UnrestrictedWindowIsIndependentPerProcess) {
        auto counters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "test");
        NOlap::NGroupedMemoryManager::TConfig config;
        {
            NKikimrConfig::TGroupedMemoryLimiterConfig protoConfig;
            protoConfig.SetMemoryLimit(1);
            protoConfig.SetUnrestrictedGroupsCount(16);
            UNIT_ASSERT(config.DeserializeFromProto(protoConfig));
        }
        auto groupedMemoryLimiterCounters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(
            MakeIntrusive<NMonitoring::TDynamicCounters>(), "Scan");
        auto stage = std::make_shared<NOlap::NGroupedMemoryManager::TStageFeatures>(
            "GLOBAL", config.GetMemoryLimit(), config.GetHardMemoryLimit(), nullptr, groupedMemoryLimiterCounters->BuildStageCounters("general"));
        auto manager = std::make_shared<NOlap::NGroupedMemoryManager::TManager>(NActors::TActorId(), config, "test", counters, stage);

        std::vector<std::shared_ptr<TAllocation>> first;
        std::vector<std::shared_ptr<TAllocation>> second;
        first.reserve(17);
        second.reserve(17);

        for (ui64 processId = 0; processId < 2; ++processId) {
            manager->RegisterProcess(processId, {});
            manager->RegisterProcessScope(processId, 0);
            auto& target = processId == 0 ? first : second;
            for (ui64 groupId = 1; groupId <= 17; ++groupId) {
                manager->RegisterGroup(processId, 0, groupId);
                auto alloc = std::make_shared<TAllocation>(100);
                manager->RegisterAllocation(processId, 0, groupId, alloc, {});
                target.emplace_back(std::move(alloc));
            }
        }

        for (ui32 i = 0; i < 16; ++i) {
            UNIT_ASSERT(first[i]->IsAllocated());
            UNIT_ASSERT(second[i]->IsAllocated());
        }
        UNIT_ASSERT(!first[16]->IsAllocated());
        UNIT_ASSERT(!second[16]->IsAllocated());

        first[0]->Guard.reset();
        manager->UnregisterAllocation(0, 0, first[0]->GetIdentifier());
        manager->UnregisterGroup(0, 0, 1);

        UNIT_ASSERT(first[16]->IsAllocated());
        UNIT_ASSERT(!second[16]->IsAllocated());

        auto cleanup = [&](const ui64 processId, std::vector<std::shared_ptr<TAllocation>>& allocations) {
            for (ui32 i = 0; i < allocations.size(); ++i) {
                if (allocations[i]->Guard) {
                    allocations[i]->Guard.reset();
                }
                manager->UnregisterAllocation(processId, 0, allocations[i]->GetIdentifier());
                if (!(processId == 0 && i == 0)) {
                    manager->UnregisterGroup(processId, 0, i + 1);
                }
            }
            manager->UnregisterProcessScope(processId, 0);
            manager->UnregisterProcess(processId);
        };
        cleanup(0, first);
        cleanup(1, second);
        first.clear();
        second.clear();

        UNIT_ASSERT_VALUES_EQUAL(stage->GetUsage().Val(), 0);
        UNIT_ASSERT(manager->IsEmpty());
        UNIT_ASSERT_VALUES_EQUAL(TObjectCounter<TAllocation>::ObjectCount(), 0);
    }

    Y_UNIT_TEST(HardLimitStillBlocksUnrestrictedGroup) {
        auto counters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(MakeIntrusive<NMonitoring::TDynamicCounters>(), "test");
        NOlap::NGroupedMemoryManager::TConfig config;
        {
            NKikimrConfig::TGroupedMemoryLimiterConfig protoConfig;
            protoConfig.SetMemoryLimit(1);
            protoConfig.SetHardMemoryLimit(50);
            protoConfig.SetUnrestrictedGroupsCount(16);
            UNIT_ASSERT(config.DeserializeFromProto(protoConfig));
        }
        auto groupedMemoryLimiterCounters = std::make_shared<NOlap::NGroupedMemoryManager::TCounters>(
            MakeIntrusive<NMonitoring::TDynamicCounters>(), "Scan");
        auto stage = std::make_shared<NOlap::NGroupedMemoryManager::TStageFeatures>(
            "GLOBAL", config.GetMemoryLimit(), config.GetHardMemoryLimit(), nullptr, groupedMemoryLimiterCounters->BuildStageCounters("general"));
        auto manager = std::make_shared<NOlap::NGroupedMemoryManager::TManager>(NActors::TActorId(), config, "test", counters, stage);

        manager->RegisterProcess(0, {});
        manager->RegisterProcessScope(0, 0);
        manager->RegisterGroup(0, 0, 1);
        auto alloc = std::make_shared<TFallibleAllocation>(100);
        manager->RegisterAllocation(0, 0, 1, alloc, {});
        UNIT_ASSERT(!alloc->IsAllocated());
        UNIT_ASSERT(!alloc->Error.empty());

        manager->UnregisterGroup(0, 0, 1);
        manager->UnregisterProcessScope(0, 0);
        manager->UnregisterProcess(0);

        UNIT_ASSERT_VALUES_EQUAL(stage->GetUsage().Val(), 0);
        UNIT_ASSERT(manager->IsEmpty());
    }
};
