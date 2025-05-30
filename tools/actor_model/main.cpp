#include "actors.h"
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>
#include <chrono>

namespace {

class TActorSystemBuilder {
public:
    struct TConfig {
        ui32 WorkerThreads;
        ui32 ExecutorPools;
        ui32 MailboxSize;
    };

    static THolder<NActors::TActorSystemSetup> Create(const TConfig& config) {
        auto setup = MakeHolder<NActors::TActorSystemSetup>();
        
        setup->ExecutorsCount = config.ExecutorPools;
        setup->Executors.Reset(new TAutoPtr<NActors::IExecutorPool>[config.ExecutorPools]);
        
        for (ui32 i : xrange(config.ExecutorPools)) {
            setup->Executors[i] = new NActors::TBasicExecutorPool(
                i, config.WorkerThreads, config.MailboxSize);
        }
        
        setup->Scheduler.Reset(new NActors::TBasicSchedulerThread(
            NActors::TSchedulerConfig(config.MailboxSize, 0)));
        
        return setup;
    }
};

class TApplicationRunner {
public:
    explicit TApplicationRunner(NActors::TActorSystem& system)
        : ActorSystem(system) {}

    void Initialize() {
        // Инициализация системных акторов
        RegisterLatencyMonitor();
        RegisterProcessingActors();
    }

    void Run() {
        while (ShouldContinue()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

private:
    void RegisterLatencyMonitor() {
        ActorSystem.Register(CreateSelfPingActor(TDuration::Seconds(1)).Release());
    }

    void RegisterProcessingActors() {
        ResultCollector = ActorSystem.Register(CreateWriteActor().Release());
        DataProcessor = ActorSystem.Register(
            CreateReadActor(Cin, ResultCollector).Release());
    }

    bool ShouldContinue() const {
        return GetProgramShouldContinue()->PollState() == TProgramShouldContinue::Continue;
    }

    NActors::TActorSystem& ActorSystem;
    NActors::TActorId ResultCollector;
    NActors::TActorId DataProcessor;
};

} // namespace

int main(int argc, const char* argv[]) {
    Y_UNUSED(argc, argv);

    // Конфигурация системы акторов
    TActorSystemBuilder::TConfig systemConfig{
        .WorkerThreads = 20,
        .ExecutorPools = 1,
        .MailboxSize = 512
    };

    // Инициализация системы акторов
    auto actorSystemSetup = TActorSystemBuilder::Create(systemConfig);
    NActors::TActorSystem actorSystem(std::move(actorSystemSetup));
    actorSystem.Start();

    // Запуск приложения
    TApplicationRunner app(actorSystem);
    app.Initialize();
    app.Run();

    // Завершение работы
    actorSystem.Stop();
    actorSystem.Cleanup();
    
    return GetProgramShouldContinue()->GetReturnCode();
}