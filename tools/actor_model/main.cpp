#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

THolder<NActors::TActorSystemSetup> BuildActorSystemSetup(ui32 threads, ui32 pools) {
    auto setup = MakeHolder<NActors::TActorSystemSetup>();
    setup->ExecutorsCount = pools;
    setup->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[pools]);
    ui32 index = 0;
    while (index < pools) {
        setup->Executors[index] = new NActors::TBasicExecutorPool(index, threads, 512);
        ++index;
    }
    setup->Scheduler.Reset(new NActors::TBasicSchedulerThread(NActors::TSchedulerConfig(512, 0)));
    return setup;
}

int main(int argc, const char* argv[]) {
    Y_UNUSED(argc, argv);
    auto actorSystemSetup = BuildActorSystemSetup(20, 1);
    NActors::TActorSystem actorSystem(actorSystemSetup);
    actorSystem.Start();

    actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());
    
    NActors::TActorId writeActorId = actorSystem.Register(CreateWriteActor().Release());
    
    actorSystem.Register(CreateReadActor(std::cin, writeActorId).Release());

    auto shouldContinue = GetProgramShouldContinue();
    
    for (;;) {
        if (shouldContinue->PollState() != TProgramShouldContinue::Continue) {
            break;
        }
        Sleep(TDuration::MilliSeconds(200));
    }

    actorSystem.Stop();
    actorSystem.Cleanup();
    
    return shouldContinue->GetReturnCode();
}
