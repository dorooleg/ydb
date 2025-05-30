#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

THolder<NActors::TActorSystemSetup> CreateSystemConfiguration(ui32 workerThreads, ui32 executorPools) {
    auto config = MakeHolder<NActors::TActorSystemSetup>();
    config->ExecutorsCount = executorPools;
    config->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[executorPools]);
    for (ui32 poolIdx : xrange(executorPools)) {
        config->Executors[poolIdx] = new NActors::TBasicExecutorPool(poolIdx, workerThreads, 512);
    }
    config->Scheduler.Reset(new NActors::TBasicSchedulerThread(NActors::TSchedulerConfig(512, 0)));
    return config;
}

int main(int argc, const char* argv[]) {
    Y_UNUSED(argc, argv);
    
    auto systemConfig = CreateSystemConfiguration(20, 1);
    NActors::TActorSystem actorSystem(systemConfig);
    actorSystem.Start();

    // Register system actors
    actorSystem.Register(CreateHeartbeatActor(TDuration::Seconds(1)).Release());
    NActors::TActorId resultWriterId = actorSystem.Register(CreateOutputWriter().Release());
    actorSystem.Register(CreateInputReader(std::cin, resultWriterId).Release());

    // Main execution loop
    auto continueFlag = GetContinueFlag();
    while (continueFlag->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(200));
    }
    
    actorSystem.Stop();
    actorSystem.Cleanup();
    return continueFlag->GetReturnCode();
}
