#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <memory>
#include <chrono>

static auto ShouldContinueFlag = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId OutputActor;
    bool ReadingFinish;
    size_t ActiveWorkersCount;

public:
    TReadActor(const NActors::TActorId outputActor)
        : OutputActor(outputActor), ReadingFinish(false), ActiveWorkersCount(0) {}

    void Bootstrap() {
        Become(&TReadActor::StateHandler);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateHandler, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvProcessingFinished::EventType, HandleCalculationDone);
    });

    void HandleWakeUp() {
        int64_t value;
        if (std::cin >> value) {
            // Если чтение удачное
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), OutputActor).Release());
            ++ActiveWorkersCount;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            // Если ошибка
            ReadingFinish = true;
            CheckCompletion();
        }
    }

    void HandleCalculationDone() {
        --ActiveWorkersCount;
        CheckCompletion();
    }

    void CheckCompletion() {
        if (ReadingFinish && ActiveWorkersCount == 0) {
            Send(OutputActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t NumberToFactorize;

    const NActors::TActorIdentity InputActor;
    const NActors::TActorId OutputActor;

    int64_t CurrentPrimeCandidate = 2;

    STRICT_STFUNC(StateHandler, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    });

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        TInstant start = TInstant::Now();
        int64_t maxPrime = 1;

        while (CurrentPrimeCandidate <= NumberToFactorize) {
            if (NumberToFactorize % CurrentPrimeCandidate == 0) {
                maxPrime = CurrentPrimeCandidate;
                do {
                    NumberToFactorize /= CurrentPrimeCandidate;
                } while (NumberToFactorize % CurrentPrimeCandidate == 0);
            }
            CurrentPrimeCandidate++;
            if (TInstant::Now() - start > TDuration::MilliSeconds(10)) {
                Send(SelfId(), new NActors::TEvents::TEvWakeup());
                return;
            }
        }

        auto result = MakeHolder<TEvents::TEvWriteValueRequest>(maxPrime);
        Send(OutputActor, std::make_unique<TEvents::TEvWriteValueRequest>(maxPrime));
        Send(InputActor, std::make_unique<TEvents::TEvProcessingFinished>());
        PassAway();
    }

    public:
        TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity inputActor, const NActors::TActorId outputActor)
            : NumberToFactorize(value), InputActor(inputActor), OutputActor(outputActor) {}

        void Bootstrap() {
            Become(&TMaximumPrimeDevisorActor::StateHandler);
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
private:
    int AccumulatedSum;

public:
    using TBase = NActors::TActor<TWriteActor>;
    TWriteActor() : TBase(&TWriteActor::Handler), AccumulatedSum(0) {}

    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, OnWriteValue);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, OnPoisonPill);
    });

    void OnWriteValue(TEvents::TEvWriteValueRequest::TPtr& ev) {
        AccumulatedSum += ev->Get()->Value;
    }

    void OnPoisonPill() {
        std::cout << AccumulatedSum << std::endl;
        ShouldContinueFlag->ShouldStop();
        PassAway();
    }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastCheck;

public:
    TSelfPingActor(const TDuration& latency) : Latency(latency) {}

    void Bootstrap() {
        LastCheck = TInstant::Now();
        Become(&TSelfPingActor::StateHandler);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateHandler, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto now = TInstant::Now();
        Y_VERIFY(now - LastCheck <= Latency, "Latency too big");
        LastCheck = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

// Всевозможные ретёрны

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinueFlag;
}

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId outputActor) {
    return MakeHolder<TReadActor>(outputActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity inputActor, const NActors::TActorId outputActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, inputActor, outputActor);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}
