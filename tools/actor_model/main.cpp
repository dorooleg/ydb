#include "actors.h"

#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

constexpr ui32 DEFAULT_THREADS = 4;
constexpr ui32 DEFAULT_POOLS = 1;
constexpr ui32 STACK_SIZE = 512;
constexpr ui32 SLEEP_MS = 200;

THolder<NActors::TActorSystemSetup> BuildActorSystemSetup(ui32 threads, ui32 pools) {
  auto setup = MakeHolder<NActors::TActorSystemSetup>();
  setup->ExecutorsCount = pools;
  setup->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[pools]);
  for (ui32 idx : xrange(pools)) {
    setup->Executors[idx] = new NActors::TBasicExecutorPool(idx, threads, STACK_SIZE);
  }

  setup->Scheduler.Reset(
      new NActors::TBasicSchedulerThread(NActors::TSchedulerConfig(STACK_SIZE, 0)));

  return setup;
}

int main(int argc, const char *argv[]) {
  Y_UNUSED(argc);
  auto setup = BuildActorSystemSetup(DEFAULT_THREADS, DEFAULT_POOLS);
  NActors::TActorSystem actorSystem(setup);
  actorSystem.Start();

  actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());
  auto writeId = actorSystem.Register(CreateWriteActor().Release());
  actorSystem.Register(CreateReadActor(writeId).Release());

  auto shouldContinue = GetProgramShouldContinue();
  while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
    Sleep(TDuration::MilliSeconds(SLEEP_MS));
  }

  actorSystem.Stop();
  actorSystem.Cleanup();
  return shouldContinue->GetReturnCode();
}