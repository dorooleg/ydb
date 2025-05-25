#include "actors.h"

#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

namespace {
    constexpr ui32 DEFAULT_THREAD_COUNT = 20;
    constexpr ui32 DEFAULT_POOL_COUNT = 1;
    constexpr ui32 QUEUE_SIZE = 512;
    constexpr ui32 POLL_INTERVAL_MS = 200;
    constexpr ui32 PING_INTERVAL_SECONDS = 1;
}

THolder<NActors::TActorSystemSetup> BuildActorSystemSetup(ui32 threads, ui32 pools) {
    auto setup = MakeHolder<NActors::TActorSystemSetup>();
    setup->ExecutorsCount = pools;
    setup->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[pools]);

    for (ui32 idx : xrange(pools)) {
        setup->Executors[idx] = new NActors::TBasicExecutorPool(idx, threads, QUEUE_SIZE);
    }
    setup->Scheduler.Reset(new NActors::TBasicSchedulerThread(
        NActors::TSchedulerConfig(QUEUE_SIZE, 0)
    ));
    return setup;
}

int main(int argc, const char* argv[])
{
    Y_UNUSED(argc, argv);
    auto actorSystemSetup = BuildActorSystemSetup(DEFAULT_THREAD_COUNT, DEFAULT_POOL_COUNT);
    NActors::TActorSystem actorSystem(actorSystemSetup);
    actorSystem.Start();

    actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(PING_INTERVAL_SECONDS)).Release());

    //регистрация Write
    NActors::TActorId write = actorSystem.Register(CreateWriteActor().Release());
    //регистрация Read
    actorSystem.Register(CreateReadActor(write).Release());

    auto shouldContinue = GetProgramShouldContinue();
    while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(POLL_INTERVAL_MS));
    }

    actorSystem.Stop();
    actorSystem.Cleanup();
    
    return shouldContinue->GetReturnCode();
}