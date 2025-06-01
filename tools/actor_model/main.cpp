#include "actors.h" // Здесь уже должны быть объявления CreateReadActor и CreateWriteActor
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h> // Для xrange

// Функция BuildActorSystemSetup из вашего примера
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
    Y_UNUSED(argc, argv); // Макрос для подавления предупреждений о неиспользуемых переменных

    auto actorSystemSetup = BuildActorSystemSetup(4, 1); // Например, 4 потока в 1 пуле
    NActors::TActorSystem actorSystem(actorSystemSetup);
    actorSystem.Start();

    // Закомментируем SelfPingActor, если он не нужен для основной логики
    // actorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());

    // Регистрируем Write и Read акторы
    auto writeActorId = actorSystem.Register(CreateWriteActor().Release());
    actorSystem.Register(CreateReadActor(writeActorId).Release());

    // Раскомментированный код для ожидания завершения
    auto shouldContinue = GetProgramShouldContinue();
    while (shouldContinue->PollState() == TProgramShouldContinue::Continue) {
        Sleep(TDuration::MilliSeconds(200)); // Ожидаем, не нагружая процессор
    }

    actorSystem.Stop();
    actorSystem.Cleanup();
    return shouldContinue->GetReturnCode(); // Возвращаем код завершения
}
