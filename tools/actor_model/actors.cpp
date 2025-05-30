#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <cmath>
#include <limits>
#include <memory>
#include <chrono>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId WriteActor;
    int aliveActors;
public:
    TReadActor(NActors::TActorId writeActor) : WriteActor(writeActor), aliveActors(0) {}

    void Bootstrap() {
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
        Become(&TReadActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeUp() {
        int value;
        if (std::cin >> value) {
            auto actor = CreateSelfTMaximumPrimeDivisorActor(value, SelfId(), WriteActor);
            Register(actor.Release());
            aliveActors++;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }
    }

    void HandleDone() {
        aliveActors--;
        if (aliveActors == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
        }
    }
};

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
private:
    int value;
    int64_t answer; // Исправлен тип на int64_t
    int currentDivisor;
    int structReceived;
    NActors::TActorIdentity ReadActor;
    NActors::TActorId WriteActor;

public:
    TMaximumPrimeDivisorActor(int Value, NActors::TActorIdentity readActor, NActors::TActorId writeActor)
        : value(Value), ReadActor(readActor),
          WriteActor(writeActor), currentDivisor(1),
          answer(1), structReceived(2) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        auto startTime = std::chrono::steady_clock::now();
        for (int i = currentDivisor; i <= value; ++i) {
            if (value % i == 0) {
                std::unique_ptr<checkIsPrimeNumberReturns> returns(
                    checkIsPrimeNumber(i, startTime, structReceived)
                );
                if (returns->status) {
                    if (returns->result && i > answer) {
                        answer = i;
                    }
                } else {
                    structReceived = returns->received;
                    currentDivisor = i;
                    Send(SelfId(), new NActors::TEvents::TEvWakeup());
                    return;
                }
            }
        }
        Send(WriteActor, new TEvents::TEvWriteValueRequest(answer));
        Send(ReadActor, new TEvents::TEvDone());
        PassAway();
    }
};

checkIsPrimeNumberReturns* checkIsPrimeNumber(int n, std::chrono::steady_clock::time_point startTime, int received) {
    if (n <= 1) {
        return new checkIsPrimeNumberReturns(true, false, received);
    }
    int countDivisors = 0;
    int j = received;

    for (; j * j <= n; ++j) {
        if (n % j == 0) {
            ++countDivisors;
            if (j != n / j) {
                ++countDivisors;
            }
        }
        if (std::chrono::steady_clock::now() - startTime > std::chrono::milliseconds(10)) {
            return new checkIsPrimeNumberReturns(false, false, j);
        }
    }
    bool isPrime = (countDivisors == 0);
    return new checkIsPrimeNumberReturns(true, isPrime, j);
}

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
private:
    int64_t sum;
public:
    TWriteActor() : sum(0) {}

    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWrite);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });

    void HandleWrite(TEvents::TEvWriteValueRequest::TPtr& ev) {
        sum += ev->Get()->value;
    }

    void HandleDone() {
        std::cout << sum << std::endl;
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

THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder<NActors::IActor> CreateSelfTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder<NActors::IActor> CreateSelfTMaximumPrimeDivisorActor(int value, NActors::TActorIdentity readActor, NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDivisorActor>(value, readActor, writeActor);
}

THolder<NActors::IActor> CreateSelfTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr <TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
