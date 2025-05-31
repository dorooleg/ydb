// main.cpp
#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

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

int main(int argc, const char* argv[])
{
    Y_UNUSED(argc, argv);
    // Настройка и запуск системы акторов
    auto actorySystemSetup = BuildActorSystemSetup(20, 1);
    NActors::TActorSystem actorSystem(actorySystemSetup);
    actorSystem.Start();

    // Актор для самотестирования
    actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());

    // Создаем и регистрируем актора-писателя
    auto writeActor = actorSystem.Register(CreateWriteActor().Release());
    // Создаем и регистрируем актора-читателя, передаем ему входной поток и ID писателя
    auto readActor = actorSystem.Register(CreateReadActor(Cin, writeActor).Release());

    // Основной цикл выполнения
    auto shouldContinue = GetProgramShouldContinue();
    while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(200));
    }

    // Завершение работы
    actorSystem.Stop();
    actorSystem.Cleanup();
    return shouldContinue->GetReturnCode();
}