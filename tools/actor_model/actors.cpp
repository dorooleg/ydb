#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <cmath>
#include <limits>
#include <memory>
#include <chrono>
#include <unordered_map>

namespace NPrimeDivisors {

using namespace NActors;

static auto ProgramShouldContinue = std::make_shared<TProgramShouldContinue>();

// Вспомогательный класс для проверки простых чисел с ограничением по времени
class TPrimeChecker {
public:
    struct TCheckResult {
        bool IsCompleted;
        bool IsPrime;
        int LastChecked;
    };

    static TCheckResult CheckWithTimeout(int number, int startFrom, 
                                       const std::chrono::steady_clock::time_point& startTime) {
        if (number <= 1) {
            return {true, false, startFrom};
        }

        int lastChecked = startFrom;
        if (lastChecked < 2) lastChecked = 2;

        for (; lastChecked * lastChecked <= number; ++lastChecked) {
            if (number % lastChecked == 0) {
                return {true, false, lastChecked};
            }

            if (std::chrono::steady_clock::now() - startTime > std::chrono::milliseconds(10)) {
                return {false, false, lastChecked};
            }
        }

        return {true, true, lastChecked};
    }
};

// Актор для вычисления максимального простого делителя
class TMaxPrimeDivisorFinder : public TActorBootstrapped<TMaxPrimeDivisorFinder> {
private:
    const int64_t Number;
    int64_t CurrentDivisor;
    int64_t MaxDivisor;
    TActorId SenderActor;
    TActorId ReceiverActor;
    bool IsCompleted;

public:
    TMaxPrimeDivisorFinder(int64_t number, const TActorId& sender, const TActorId& receiver)
        : Number(number), CurrentDivisor(1), MaxDivisor(1), 
          SenderActor(sender), ReceiverActor(receiver), IsCompleted(false) {}

    void Bootstrap() {
        Become(&TMaxPrimeDivisorFinder::ActiveState);
        ScheduleNextCheck();
    }

private:
    void ScheduleNextCheck() {
        Send(SelfId(), new TEvents::TEvWakeup());
    }

    void ProcessDivision() {
        auto startTime = std::chrono::steady_clock::now();
        
        for (; CurrentDivisor <= Number; ++CurrentDivisor) {
            if (Number % CurrentDivisor == 0) {
                auto result = TPrimeChecker::CheckWithTimeout(
                    CurrentDivisor, 2, startTime);
                
                if (result.IsCompleted && result.IsPrime && CurrentDivisor > MaxDivisor) {
                    MaxDivisor = CurrentDivisor;
                }

                if (!result.IsCompleted) {
                    CurrentDivisor = result.LastChecked;
                    ScheduleNextCheck();
                    return;
                }
            }

            if (std::chrono::steady_clock::now() - startTime > std::chrono::milliseconds(10)) {
                ScheduleNextCheck();
                return;
            }
        }

        CompleteProcessing();
    }

    void CompleteProcessing() {
        if (!IsCompleted) {
            Send(ReceiverActor, new TEvents::TEvWriteValueRequest(MaxDivisor));
            Send(SenderActor, new TEvents::TEvDone());
            IsCompleted = true;
            PassAway();
        }
    }

    STRICT_STFUNC(ActiveState, {
        cFunc(TEvents::TEvWakeup::EventType, ProcessDivision);
    });
};

// Актор для чтения входных данных
class TInputReader : public TActorBootstrapped<TInputReader> {
private:
    const TActorId ResultWriter;
    size_t ActiveWorkers;
    bool InputExhausted;

public:
    TInputReader(const TActorId& writer) 
        : ResultWriter(writer), ActiveWorkers(0), InputExhausted(false) {}

    void Bootstrap() {
        Become(&TInputReader::ReadingState);
        RequestNextNumber();
    }

private:
    void RequestNextNumber() {
        Send(SelfId(), new TEvents::TEvWakeup());
    }

    void ProcessInput() {
        int64_t value;
        if (std::cin >> value) {
            RegisterChild(CreateMaxPrimeDivisorFinder(value, SelfId(), ResultWriter));
            ActiveWorkers++;
            RequestNextNumber();
        } else {
            InputExhausted = true;
            CheckCompletion();
        }
    }

    void HandleCompletion() {
        ActiveWorkers--;
        CheckCompletion();
    }

    void CheckCompletion() {
        if (InputExhausted && ActiveWorkers == 0) {
            Send(ResultWriter, new TEvents::TEvPoisonPill());
            PassAway();
        }
    }

    STRICT_STFUNC(ReadingState, {
        cFunc(TEvents::TEvWakeup::EventType, ProcessInput);
        cFunc(TEvents::TEvDone::EventType, HandleCompletion);
    });
};

// Актор для записи результатов
class TResultWriter : public TActorBootstrapped<TResultWriter> {
private:
    int64_t TotalSum;

public:
    TResultWriter() : TotalSum(0) {}

    void Bootstrap() {
        Become(&TResultWriter::WritingState);
    }

private:
    void AddResult(TEvents::TEvWriteValueRequest::TPtr& ev) {
        TotalSum += ev->Get()->Value;
    }

    void Finalize() {
        std::cout << "Total sum of max prime divisors: " << TotalSum << std::endl;
        ProgramShouldContinue->ShouldStop();
        PassAway();
    }

    STRICT_STFUNC(WritingState, {
        hFunc(TEvents::TEvWriteValueRequest, AddResult);
        cFunc(TEvents::TEvPoisonPill::EventType, Finalize);
    });
};

// Актор для самотестирования задержек
class TLatencyTester : public TActorBootstrapped<TLatencyTester> {
    const TDuration MaxLatency;
    TInstant LastPingTime;

public:
    TLatencyTester(const TDuration& latency) 
        : MaxLatency(latency), LastPingTime(TInstant::Now()) {}

    void Bootstrap() {
        Become(&TLatencyTester::TestingState);
        ScheduleNextPing();
    }

private:
    void ScheduleNextPing() {
        Send(SelfId(), new TEvents::TEvWakeup());
    }

    void VerifyLatency() {
        auto now = TInstant::Now();
        auto delta = now - LastPingTime;
        Y_VERIFY(delta <= MaxLatency, "Maximum allowed latency exceeded");
        LastPingTime = now;
        ScheduleNextPing();
    }

    STRICT_STFUNC(TestingState, {
        cFunc(TEvents::TEvWakeup::EventType, VerifyLatency);
    });
};

} // namespace NPrimeDivisors

// Фабричные функции
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<NPrimeDivisors::TLatencyTester>(latency);
}

THolder<NActors::IActor> CreateSelfTReadActor(const NActors::TActorId& writeActor) {
    return MakeHolder<NPrimeDivisors::TInputReader>(writeActor);
}

THolder<NActors::IActor> CreateSelfTMaximumPrimeDivisorActor(int64_t value, 
                                                           const NActors::TActorId& readActor,
                                                           const NActors::TActorId& writeActor) {
    return MakeHolder<NPrimeDivisors::TMaxPrimeDivisorFinder>(value, readActor, writeActor);
}

THolder<NActors::IActor> CreateSelfTWriteActor() {
    return MakeHolder<NPrimeDivisors::TResultWriter>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return NPrimeDivisors::ProgramShouldContinue;
}