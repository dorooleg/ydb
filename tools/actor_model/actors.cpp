#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    NActors::TActorId WriteActorId;
    int RunningActors;
    bool Finished;

public:
    TReadActor(const NActors::TActorId& writeActorId)
        : WriteActorId(writeActorId), RunningActors(0), Finished(false) {}

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
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActorId).Release());
            RunningActors++;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            Finished = true;
            if (RunningActors == 0) {
                Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
                PassAway();
            }
        }
    }

    void HandleDone() {
        RunningActors--;
        if (Finished && RunningActors == 0) {
            Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Number;
    int64_t CurrentDivisor;
    int64_t MaxPrimeDivisor;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId)
        : Number(value), CurrentDivisor(2), MaxPrimeDivisor(0),
          ReadActorId(readActorId), WriteActorId(writeActorId) {}

    void Bootstrap() {
        if (Number == 1) {
            Send(WriteActorId, new TEvents::TEvWriteValueRequest(1));
            Send(ReadActorId, new TEvents::TEvDone());
            PassAway();
            return;
        }
        if (Number < 1) {
            Send(WriteActorId, new TEvents::TEvWriteValueRequest(0));
            Send(ReadActorId, new TEvents::TEvDone());
            PassAway();
            return;
        }
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, Calculate);
    });

    void Calculate() {
        auto start = TInstant::Now();
        while (Number > 1) {
            if (CurrentDivisor * CurrentDivisor > Number) {
                MaxPrimeDivisor = Number;
                Number = 1;
                break;
            }
            if (Number % CurrentDivisor == 0) {
                MaxPrimeDivisor = CurrentDivisor;
                Number /= CurrentDivisor;
            } else {
                CurrentDivisor = (CurrentDivisor == 2) ? 3 : CurrentDivisor + 2;
            }
            if (TInstant::Now() - start >= TDuration::MilliSeconds(10)) {
                Send(SelfId(), new NActors::TEvents::TEvWakeup());
                return;
            }
        }
        Send(WriteActorId, new TEvents::TEvWriteValueRequest(MaxPrimeDivisor));
        Send(ReadActorId, new TEvents::TEvDone());
        PassAway();
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum;

public:
    TWriteActor() : TActor(&TWriteActor::StateInit), Sum(0) {}

    STRICT_STFUNC(StateInit, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoison);
    });

    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoison() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop(0);
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
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
