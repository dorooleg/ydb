#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

THolder<NActors::TActorSystemSetup> BuildActorSystemSetup(uint32_t threads, uint32_t pools) {
    auto setup = MakeHolder<NActors::TActorSystemSetup>();
    setup->ExecutorsCount = pools;
    setup->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[pools]);
    for (uint32_t idx : xrange(pools)) {
        setup->Executors[idx] = new NActors::TBasicExecutorPool(idx, threads, 512);
    }
    setup->Scheduler.Reset(new NActors::TBasicSchedulerThread(NActors::TSchedulerConfig(512, 0)));
    return setup;
}

int main(int argc, const char* argv[]) {
    Y_UNUSED(argc);
    Y_UNUSED(argv);

    auto setup = BuildActorSystemSetup(20, 1);
    NActors::TActorSystem actorSystem(setup);
    actorSystem.Start();

    actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());
    auto writerId = actorSystem.Register(CreateWriteActor().Release());
    actorSystem.Register(CreateReadActor(writerId).Release());

    auto shouldContinue = GetProgramShouldContinue();
    while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(200));
    }
    actorSystem.Stop();
    actorSystem.Cleanup();
    return shouldContinue->GetReturnCode();
}