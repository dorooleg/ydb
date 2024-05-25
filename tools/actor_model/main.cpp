#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>


THolder<NActors::TActorSystemSetup> SetupSystem(ui32 threads, ui32 pools) {
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
    
    
    auto systemSetup = SetupSystem(20, 1);
    NActors::TActorSystem actorSystem(systemSetup);
    actorSystem.Start();

    
    auto sumHandlerId = actorSystem.Register(CreateSumHandler().Release());
    auto primeHandlerId = actorSystem.Register(CreatePrimeHandler(&actorSystem, sumHandlerId).Release());
    actorSystem.Register(CreateInputHandler(&actorSystem, primeHandlerId).Release());

    
    auto shouldContinue = GetProgramStatus();
    while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(200));
    }


    actorSystem.Stop();
    Sleep(TDuration::Seconds(1));
    actorSystem.Cleanup();

    return shouldContinue->GetReturnCode();
}
