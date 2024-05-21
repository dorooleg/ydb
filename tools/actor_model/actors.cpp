#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <cmath>
#include <iostream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::shared_ptr<TProgramShouldContinue> ShouldContinue;
    std::istream& Strm;
    NActors::TActorId WriteActor;
    int InProgress;

public:
    TReadActor(std::shared_ptr<TProgramShouldContinue> shouldContinue, std::istream& strm)
        : ShouldContinue(shouldContinue), Strm(strm), InProgress(0)
    {}

    void Bootstrap() {
        WriteActor = Register(CreateWriteActor(ShouldContinue).Release());
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
        Become(&TReadActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc,
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    )

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        int64_t value;
        if (Strm >> value) {
            InProgress++;
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else if (InProgress == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill);
            ShouldContinue->ShouldStop();
            PassAway();
        }
    }

    void HandleDone() {
        if (--InProgress == 0 && Strm.eof()) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill);
            ShouldContinue->ShouldStop();
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateReadActor(std::shared_ptr<TProgramShouldContinue> shouldContinue) {
    return MakeHolder<TReadActor>(shouldContinue, std::cin);
}

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;
    int64_t LargestPrime;
    TInstant StartTime;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor), LargestPrime(1)
    {}

    void Bootstrap() {
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
        Become(&TMaximumPrimeDevisorActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc,
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    )

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        StartTime = TInstant::Now();
        Calculate();
        if (TInstant::Now() - StartTime > TDuration::MilliSeconds(10)) {
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            Send(WriteActor, new TEvents::TEvWriteValueRequest(LargestPrime));
            Send(ReadActor, new TEvents::TEvDone);
            PassAway();
        }
    }

    void Calculate() {
        if (Value <= 1) return;

        while (Value % 2 == 0) {
            LargestPrime = 2;
            Value /= 2;
        }

        for (int i = 3; i <= std::sqrt(Value); i += 2) {
            while (Value % i == 0) {
                LargestPrime = i;
                Value /= i;
            }
        }

        if (Value > 2) {
            LargestPrime = Value;
        }
    }
};

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
    std::shared_ptr<TProgramShouldContinue> ShouldContinue;
    int64_t Sum;

public:
    TWriteActor(std::shared_ptr<TProgramShouldContinue> shouldContinue)
        : ShouldContinue(shouldContinue), Sum(0)
    {}

    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc,
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValue);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    )

    void HandleWriteValue(TEvents::TEvWriteValueRequest::TPtr ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

THolder<NActors::IActor> CreateWriteActor(std::shared_ptr<TProgramShouldContinue> shouldContinue) {
    return MakeHolder<TWriteActor>(shouldContinue);
}
