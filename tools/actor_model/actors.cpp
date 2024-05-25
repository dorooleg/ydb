#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <cmath>
#include <chrono>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

bool is_prime(int64_t n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int64_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

int64_t largest_prime_divisor(int64_t n) {
    int64_t maxPrime = -1;
    for (int64_t i = 2; i <= n; ++i) {
        while (n % i == 0 && is_prime(i)) {
            maxPrime = i;
            n /= i;
        }
    }
    return maxPrime;
}

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    std::istream& Input;
    NActors::TActorId WriteActor;
    int ActiveCalculations = 0;

public:
    TReadActor(std::istream& input, NActors::TActorId writeActor)
        : Input(input), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (Input >> value) {
            ActiveCalculations++;
            RegisterWithSameMailbox(new TMaximumPrimeDevisorActor(value, SelfId(), WriteActor));
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else if (ActiveCalculations == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }

    void HandleDone(const TEvents::TEvDone::TPtr&) {
        ActiveCalculations--;
        if (ActiveCalculations == 0 && Input.eof()) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int64_t Value;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;

public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto start = std::chrono::steady_clock::now();
        int64_t maxPrime = largest_prime_divisor(Value);
        auto end = std::chrono::steady_clock::now();

        std::chrono::duration<double, std::milli> elapsed = end - start;
        if (elapsed.count() > 10.0) {
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            Send(WriteActor, new TEvents::TEvWriteValueRequest(maxPrime));
            Send(ReadActor, new TEvents::TEvDone());
            PassAway();
        }
    }
};

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
private:
    int64_t Sum = 0;

public:
    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(const TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(std::istream& input, NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(input, writeActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor) {
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
