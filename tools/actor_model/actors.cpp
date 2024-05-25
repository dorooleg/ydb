#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <chrono>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    NActors::TActorId WriteActor;
    bool IsFinished = false;
    int ActiveWorkers = 0;

public:
    explicit TReadActor(const NActors::TActorId& writeActor)
    : WriteActor(writeActor), IsFinished(false), ActiveWorkers(0) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            auto actor = CreateTMaximumPrimeDivisorActor(value, SelfId(), WriteActor);
            Register(actor.Release());
            ActiveWorkers++;
        } else {
            IsFinished = true;
            CheckForShutdown();
        }
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    void HandleDone() {
        ActiveWorkers--;
        CheckForShutdown();
    }

    void CheckForShutdown() {
        if (IsFinished && ActiveWorkers == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }
};

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
private:
    int64_t Value;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;
    int64_t LargestPrime = 0;
    int64_t CurrentDivisor = 2;

public:
    TMaximumPrimeDivisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

private:
    void HandleWakeup() {
        auto start = std::chrono::high_resolution_clock::now();
        while (CurrentDivisor * CurrentDivisor <= Value) {
            if (Value % CurrentDivisor == 0) {
                LargestPrime = CurrentDivisor;
                Value /= CurrentDivisor;
            } else {
                CurrentDivisor = (CurrentDivisor == 2) ? 3 : CurrentDivisor + 2;
            }
            if (std::chrono::high_resolution_clock::now() - start > std::chrono::milliseconds(10)) {
                Send(SelfId(), new NActors::TEvents::TEvWakeup());
                return;
            }
        }
        if (Value > 1) LargestPrime = Value;

        Send(WriteActor, new TEvents::TEvWriteValueRequest(LargestPrime));
        Send(ReadActor, new TEvents::TEvDone());
        PassAway();
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
private:
    int64_t Sum;
public:
    TWriteActor() : TActor(&TWriteActor::Handler), Sum(0) {}

    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });

    void HandleWriteRequest(const TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandleDone() {
       std::cout << Sum << std::endl;
       ShouldContinue->ShouldStop();
       PassAway();
    }		
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency)
        : Latency(latency)
    {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto now = TInstant::Now();
        TDuration delta = now - LastTime;
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder<NActors::IActor> CreateMaxPrimeDivisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDivisorActor>(value, readActor, writeActor);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
