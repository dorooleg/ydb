#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>
#include <util/system/types.h>

THolder<NActors::TActorSystemSetup> BuildActorSystemSetup(ui32 threads, ui32 pools) {
    auto setup = MakeHolder<NActors::TActorSystemSetup>();
    setup->ExecutorsCount = pools;
    setup->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[pools]);
    for (ui32 idx : xrange(pools)) {
        setup->Executors[idx] = new NActors::TBasicExecutorPool(idx, threads, 512);
    }
    setup->Scheduler.Reset(new NActors::TBasicSchedulerThread(NActors::TSchedulerConfig(512, 0)));
    return setup;
}

int main(int argc, const char* argv[]) {
    Y_UNUSED(argc, argv);
    auto actorSystemSetup = BuildActorSystemSetup(20, 1);
    NActors::TActorSystem actorSystem(actorSystemSetup);
    actorSystem.Start();

    auto writeActorId = actorSystem.Register(NActors::CreateWriteActor().Release());
    actorSystem.Register(NActors::CreateSelfPingActor(TDuration::Seconds(1)).Release());
    actorSystem.Register(NActors::CreateReadActor(std::cin, writeActorId).Release());

    auto shouldContinue = NActors::GetProgramShouldContinue();
    while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(200));
    }

    actorSystem.Stop();
    actorSystem.Cleanup();
    return shouldContinue->GetReturnCode();
}