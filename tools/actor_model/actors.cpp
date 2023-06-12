#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <limits>
#include <math.h>
#include "eratosphene_sieve.h"
static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId WriteActor;
    int alives;
    eratosphene_sieve* eratosphene_sieve;
public:
    TReadActor(NActors::TActorId writeActor, eratosphene_sieve* eratosphene_sieve1): WriteActor(writeActor), alives(0), eratosphene_sieve(eratosphene_sieve1) {}
    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });
    void HandleWakeUp() {
        int val;
        if (std::cin >> val) {
            Register(CreateSelfTMaximumPrimeDevisorActor(val, SelfId(), WriteActor, eratosphene_sieve).Release());
            alives++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
    }
    void HandleDone() {
        alives--;
        if (alives == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t val;
    const NActors::TActorIdentity ReadActor;
    const NActors::TActorId WriteActor;
    int64_t Answer;
    int64_t Ival;
    int64_t Jval;
    bool Flag;
    int64_t ExecutionTimeLimit;
public:
    TMaximumPrimeDevisorActor(int64_t val, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor)
            : val(val), ReadActor(readActor), WriteActor(writeActor), Answer(0), Ival(1), Jval(2), Flag(true), ExecutionTimeLimit(10)
    {}
    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });
    void HandleWakeUp() {
        auto StartTime = std::chrono::high_resolution_clock::now();
        for (int64_t i = Ival; i <= val; i++) {
            for (int64_t j = Jval; j * j <= i; j++) {
                if (i % j == 0) {
                    Flag = false;
                    break;
                }
                auto EndTime = std::chrono::steady_clock::now();
                auto ElapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime).count();
                if (ElapsedTime > ExecutionTimeLimit) {
                    Ival = i;
                    Jval = j;
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            if (Flag && val % i == 0) {
                Answer = i;
            } else {
                Flag = true;
            }
        }
        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(Answer));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};
class TWriteActor : public NActors::TActor<TWriteActor> {
private:
    int sum;
public:
    using TBase = NActors::TActor<TWriteActor>;

    TWriteActor() : TBase(&TWriteActor::Handler), sum(0) {}

    STRICT_STFUNC(Handler,
    {
        hFunc(TEvents::TEvWriteValueRequest, HandleWakeUp);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });

    void HandleWakeUp(TEvents::TEvWriteValueRequest::TPtr &ev) {
        auto &event = *ev->Get();
        sum = sum + event.Value;
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

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
