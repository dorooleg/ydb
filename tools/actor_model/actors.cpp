#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <limits>
#include <math.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId WriteActor;
    int aliveActors;
public:
    TReadActor(NActors::TActorId writeActor)
            : WriteActor(writeActor), aliveActors(0) {}

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
        int value;
        if (std::cin >> value) {
            Register(CreateSelfTMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            aliveActors++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
    }

    void HandleDone() {
        aliveActors--;
        if (aliveActors == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int value;
    int answer;
    int obtainedInThePreviousIteration;
    int structObtained;
    NActors::TActorIdentity ReadActor;
    NActors::TActorId WriteActor;

public:
    TMaximumPrimeDevisorActor(int Value, NActors::TActorIdentity readActor, NActors::TActorId writeActor)
            : value(Value), ReadActor(readActor),
              WriteActor(writeActor), obtainedInThePreviousIteration(1),
              answer(1), structObtained(2) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        auto startTime = std::chrono::steady_clock::now();
        for(int i = obtainedInThePreviousIteration; i <= value; i++){
            if(value % i == 0){
                checkIsPrimeNumberReturns* returns = checkIsPrimeNumber(i, startTime, structObtained);
                if(returns->status){
                    if(returns->result && i > answer){
                        answer = i;
                    }
                }
                else{
                    structObtained = returns->Obtained;
                    obtainedInThePreviousIteration = i;
                    delete returns;
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
        }
        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(answer));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

checkIsPrimeNumberReturns* checkIsPrimeNumber(int n, auto startTime, int Obtained){
    int countDevisiors = 2;
    int j = Obtained;
    while(j * j <= n){
        if(n % j == 0){
            countDevisiors++;
        }
        j++;
        if(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count() > 10){
            return new checkIsPrimeNumberReturns(false, false, j);
        }
    }
    if(countDevisiors == 2){
        return new checkIsPrimeNumberReturns(true, true, j);
    }
    else{
        return new checkIsPrimeNumberReturns(true, false, j);
    }
}

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
        sum = sum + event.value;
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
    TSelfPingActor(const TDuration &latency)
            : Latency(latency) {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
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

THolder <NActors::IActor> CreateSelfPingActor(const TDuration &latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder <NActors::IActor> CreateSelfTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder <NActors::IActor> CreateSelfTMaximumPrimeDevisorActor(int value, NActors::TActorIdentity readActor, NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder <NActors::IActor> CreateSelfTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr <TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}