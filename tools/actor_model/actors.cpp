#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <cmath>
#include <chrono>
#include <iostream>

using namespace NActors;

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

int64_t GetMaxPrimeDivisor(int64_t n) {
    if (n <= 1) return 0;
    int64_t maxPrime = -1;
    while (n % 2 == 0) {
        maxPrime = 2;
        n /= 2;
    }
    for (int64_t i = 3; i <= std::sqrt(n); i += 2) {
        while (n % i == 0) {
            maxPrime = i;
            n /= i;
        }
    }
    if (n > 2) {
        maxPrime = n;
    }
    return maxPrime;
}

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::istream& Stream;
    NActors::TActorId WriteActor;
    int64_t ActiveTasks = 0;

public:
    TReadActor(std::istream& strm, NActors::TActorId writeActor)
        : Stream(strm), WriteActor(std::move(writeActor)) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc,
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        hFunc(::TEvents::TEvDone, HandleDone);
    )

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        int64_t value;
        if (Stream >> value) {
            ActiveTasks++;
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else if (ActiveTasks == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }

    void HandleDone(::TEvents::TEvDone::TPtr) {
        ActiveTasks--;
        if (ActiveTasks == 0 && Stream.eof()) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    const NActors::TActorId ReadActor;
    const NActors::TActorId WriteActor;
    std::chrono::steady_clock::time_point StartTime;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor) {}

    void Bootstrap() {
        StartTime = std::chrono::steady_clock::now();
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc,
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    )

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - StartTime);

        if (elapsed.count() > 10) {
            StartTime = now;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            int64_t maxPrimeDivisor = GetMaxPrimeDivisor(Value);
            Send(WriteActor, new ::TEvents::TEvWriteValueRequest(maxPrimeDivisor));
            Send(ReadActor, new ::TEvents::TEvDone());
            PassAway();
        }
    }
};

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
    int64_t Sum = 0;

public:
    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc,
        hFunc(::TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    )

    void HandleWriteValueRequest(::TEvents::TEvWriteValueRequest::TPtr ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(std::istream& strm, const NActors::TActorId& writeActor) {
    return MakeHolder<TReadActor>(strm, writeActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency)
        : Latency(latency) {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    )

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

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
