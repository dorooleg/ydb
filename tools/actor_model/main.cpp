#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

THolder <NActors::TActorSystemSetup> ConfigureActorSystem(ui32 executorCount, ui32 poolCount) {
    auto systemSetup = MakeHolder<NActors::TActorSystemSetup>();
    systemSetup->ExecutorsCount = poolCount;
    systemSetup->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[poolCount]);
    for (auto idx: xrange(poolCount)) {
        systemSetup->Executors[idx].Reset(new NActors::TBasicExecutorPool(idx, executorCount, 512));
    }
    systemSetup->Scheduler.Reset(new NActors::TBasicSchedulerThread(NActors::TSchedulerConfig(512, 0)));
    return systemSetup;
}

int main(int argc, const char *argv[]) {
    Y_UNUSED(argc, argv);
    auto actorSystemSetup = ConfigureActorSystem(20, 1);
    NActors::TActorSystem actorSystem(actorSystemSetup);
    actorSystem.Start();

    actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());

    // Setup Write and Read actors
    auto writeActorId = actorSystem.Register(CreateTWriteActor().Release());
    actorSystem.Register(CreateTReadActor(writeActorId).Release());
    
    // Main event loop
    auto contSignal = GetProgramShouldContinue();
    while (contSignal->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(200));
    }
    actorSystem.Stop();
    actorSystem.Cleanup();
    return contSignal->GetReturnCode();
}
