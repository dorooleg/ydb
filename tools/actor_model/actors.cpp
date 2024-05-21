#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    const NActors::TActorId WriteActor;
    std::istream& InputStream;
    int PendingActors = 0;

public:
    TReadActor(const NActors::TActorId& writeActor, std::istream& inputStream)
        : WriteActor(writeActor), InputStream(inputStream) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    void HandleWakeup() {
        int64_t value;
        if (InputStream >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            ++PendingActors;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else if (PendingActors == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }

    void HandleDone() {
        if (--PendingActors == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });
};

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId& writeActor, std::istream& inputStream) {
    return MakeHolder<TReadActor>(writeActor, inputStream);
}

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    const NActors::TActorId ReadActor;
    const NActors::TActorId WriteActor;
    int64_t MaxPrimeDivisor = 1;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    void HandleWakeup() {
        auto start = std::chrono::high_resolution_clock::now();

        for (int64_t i = 2; i <= std::sqrt(Value); ++i) {
            while (Value % i == 0) {
                MaxPrimeDivisor = i;
                Value /= i;
            }
            if (std::chrono::high_resolution_clock::now() - start > std::chrono::milliseconds(10)) {
                Send(SelfId(), new NActors::TEvents::TEvWakeup());
                return;
            }
        }

        if (Value > 1) {
            MaxPrimeDivisor = Value;
        }

        Send(WriteActor, new TEvents::TEvWriteValueRequest(MaxPrimeDivisor));
        Send(ReadActor, new TEvents::TEvDone());
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    });
};

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
    int64_t Sum = 0;
    std::shared_ptr<TProgramShouldContinue> ShouldContinue;

public:
    TWriteActor(std::shared_ptr<TProgramShouldContinue> shouldContinue)
        : ShouldContinue(shouldContinue) {}

    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });
};

THolder<NActors::IActor> CreateWriteActor(std::shared_ptr<TProgramShouldContinue> shouldContinue) {
    return MakeHolder<TWriteActor>(shouldContinue);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}