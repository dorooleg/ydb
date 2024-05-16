#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ProgramContinueFlag = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId OutputActor;
    bool ReadingComplete = false;
    size_t ActiveCalculators = 0;
public:
    TReadActor(const NActors::TActorId outputActor)
            : OutputActor(outputActor), ReadingComplete(false), ActiveCalculators(0) {}

    void Bootstrap() {
        Become(&TReadActor::StateHandler);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateHandler, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvProcessingFinished::EventType, HandleCalculationDone);
    });

    void HandleWakeUp() {
        int64_t number;
        if (std::cin >> number) {
            Register(CreateMaximumPrimeDevisorActor(number, SelfId(), OutputActor).Release());
            ActiveCalculators++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            ReadingComplete = true;
            if (ActiveCalculators == 0) {
                Send(OutputActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            }
        }
    }

    void HandleCalculationDone() {
        ActiveCalculators--;
        if (ReadingComplete && ActiveCalculators == 0) {
            Send(OutputActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int64_t Number;
    const NActors::TActorIdentity InputActor;
    const NActors::TActorId OutputActor;
    int64_t LargestPrimeDivisor = 0;
    int64_t CurrentDivisor = 2;

public:
    TMaximumPrimeDevisorActor(int64_t number, const NActors::TActorIdentity inputActor, const NActors::TActorId outputActor)
            : Number(number), InputActor(inputActor), OutputActor(outputActor), LargestPrimeDivisor(0), CurrentDivisor(2) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateHandler);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateHandler, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        auto startTime = std::chrono::high_resolution_clock::now();
        for (int64_t divisor = CurrentDivisor; divisor * divisor <= Number; ++divisor) {
            CurrentDivisor = divisor;
            if (Number % divisor == 0) {
                LargestPrimeDivisor = divisor;
                Number /= divisor;
            }
            if (std::chrono::high_resolution_clock::now() - startTime > std::chrono::milliseconds(10)) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }
        if (Number > 1) {
            LargestPrimeDivisor = Number;
        }
        Send(OutputActor, std::make_unique<TEvents::TEvWriteValueRequest>(LargestPrimeDivisor));
        Send(InputActor, std::make_unique<TEvents::TEvProcessingFinished>());
        PassAway();
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
        ProgramContinueFlag->ShouldStop();
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
        TDuration delta = now - LastCheck;
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastCheck = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId outputActor) {
    return MakeHolder<TReadActor>(outputActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t number, const NActors::TActorIdentity inputActor, const NActors::TActorId outputActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(number, inputActor, outputActor);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ProgramContinueFlag;
}