#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

THolder<NActors::TActorSystemSetup> BuildActorSystemSetup(ui32 threads, ui32 pools) {
    auto set = MakeHolder<NActors::TActorSystemSetup>();
    set->ExecutorsCount = pools;
    set->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[pools]);
    for (ui32 idx : xrange(pools)) {
        set->Executors[idx] = new NActors::TBasicExecutorPool(idx, threads, 512);
    }
    set->Scheduler.Reset(new NActors::TBasicSchedulerThread(NActors::TSchedulerConfig(512, 0)));
    return set;
}

int main(int argc, const char* argv[])
{
    Y_UNUSED(argc, argv);
    auto actorySystemSetup = BuildActorSystemSetup(20, 1);
    NActors::TActorSystem actorSystem(actorySystemSetup);
    actorSystem.Start();
    actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());
    NActors::TActorId writer = actorSystem.Register(CreateTWriteActor().Release());
    actorSystem.Register(CreateTReadActor(writer).Release());
    auto shouldContinue = GetProgramShouldContinue();
    while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(200));
    }
    actorSystem.Stop();
    actorSystem.Cleanup();
    return shouldContinue->GetReturnCode();
}