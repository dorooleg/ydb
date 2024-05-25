#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <library/cpp/actors/util/should_continue.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

//Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor

// TODO: напишите реализацию TReadActor
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
public:
    explicit TReadActor(std::shared_ptr<TProgramShouldContinue> shouldContinue, const NActors::TActorId& writeActorId)
        : ShouldContinue(shouldContinue), WriteActorId(writeActorId), ActiveDevisorActors(0) {}

    void Bootstrap() {
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
        Become(&TReadActor::StateFunc);
    }

private:
    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr&) {
        int64_t value;
        if (std::cin >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActorId).Release());
            ++ActiveDevisorActors;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            CheckFinish();
        }
    }

    void HandleDone(TEvents::TEvDone::TPtr&) {
        --ActiveDevisorActors;
        CheckFinish();
    }

    void CheckFinish() {
        if (ActiveDevisorActors == 0) {
            Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
            ShouldContinue->ShouldStop();
            PassAway();
        }
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    })

    std::shared_ptr<TProgramShouldContinue> ShouldContinue;
    NActors::TActorId WriteActorId;
    int ActiveDevisorActors;
};

THolder<NActors::IActor> CreateReadActor(std::shared_ptr<TProgramShouldContinue> shouldContinue, const NActors::TActorId& writeActorId) {
    return MakeHolder<TReadActor>(shouldContinue, writeActorId);
}


// TODO: напишите реализацию TMaximumPrimeDevisorActor
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor), MaxPrimeDivisor(1), Start(std::chrono::steady_clock::now()) {}

    void Bootstrap() {
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
        Become(&TMaximumPrimeDevisorActor::StateFunc);
    }

private:
    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr&) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - Start);

        if (elapsed.count() >= 10) {
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
            return;
        }

        CalculateMaxPrimeDivisor();

        Send(WriteActor, new TEvents::TEvWriteValueRequest(MaxPrimeDivisor));
        Send(ReadActor, new TEvents::TEvDone());
        PassAway();
    }

    void CalculateMaxPrimeDivisor() {
        int64_t n = Value;
        for (int64_t i = 2; i <= std::sqrt(n); ++i) {
            while (n % i == 0) {
                MaxPrimeDivisor = i;
                n /= i;
            }
        }
        if (n > 1) {
            MaxPrimeDivisor = n;
        }
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    })

    int64_t Value;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;
    int64_t MaxPrimeDivisor;
    std::chrono::steady_clock::time_point Start;
};

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}


// TODO: напишите реализацию TWriteActor
class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
public:
    explicit TWriteActor(std::shared_ptr<TProgramShouldContinue> shouldContinue)
        : ShouldContinue(shouldContinue), Sum(0) {}

    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

private:
    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill(NActors::TEvents::TEvPoisonPill::TPtr&) {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    })

    std::shared_ptr<TProgramShouldContinue> ShouldContinue;
    int64_t Sum;
};

THolder<NActors::IActor> CreateWriteActor(std::shared_ptr<TProgramShouldContinue> shouldContinue) {
    return MakeHolder<TWriteActor>(shouldContinue);
}

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

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
