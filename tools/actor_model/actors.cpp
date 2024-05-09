#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <string>
#include <cmath>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
public:
    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    void HandleWakeup() {
        std::string line;
        if (std::getline(std::cin, line)) {
            std::istringstream iss(line);
            int64_t value;
            while (iss >> value) {
                Send(SelfId(), new TEvents::TEvPrimeNumber(value));
            }
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }
        else {
            Send(GetProgramShouldContinue()->WriteActorId, new NActors::TEvents::TEvPoisonPill());
        }
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        hFunc(TEvents::TEvPrimeNumber, HandlePrimeNumber);
        });

    void HandlePrimeNumber(TEvents::TEvPrimeNumber* ev) {
        auto* actor = new TMaximumPrimeDevisorActor(ev->Number, SelfId(), GetProgramShouldContinue()->WriteActorId);
        Register(actor);
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Number;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;

public:
    TMaximumPrimeDevisorActor(int64_t number, NActors::TActorId readActor, NActors::TActorId writeActor)
        : Number(number), ReadActorId(readActor), WriteActorId(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    int64_t CalculateMaxPrimeDivisor(int64_t number) {
        int64_t maxPrime = -1;
        while (number % 2 == 0) {
            maxPrime = 2;
            number /= 2;
        }
        for (int i = 3; i <= sqrt(number); i += 2) {
            while (number % i == 0) {
                maxPrime = i;
                number = number / i;
            }
        }
        if (number > 2)
            maxPrime = number;
        return maxPrime;
    }

    void HandleWakeup() {
        int64_t maxPrime = CalculateMaxPrimeDivisor(Number);
        Send(WriteActorId, new TEvents::TEvWriteValueRequest(maxPrime));
        Send(ReadActorId, new NActors::TEvents::TEvDone());
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        });
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest* ev) {
        Sum += ev->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        hFunc(NActors::TEvents::TEvPoisonPill, HandlePoisonPill);
        });
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

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
