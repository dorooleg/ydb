#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

bool IsPrime(int64_t n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    
    for (int64_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return false;
        }
    }
    return true;
}

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    NActors::TActorId WriteActorId;
    int PendingCalculations = 0;
    bool FinishedReading = false;

public:
    TReadActor(const NActors::TActorId& writeActorId)
        : WriteActorId(writeActorId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            auto actor = CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActorId);
            Register(actor.Release());
            PendingCalculations++;
            
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            FinishedReading = true;
            CheckCompletion();
        }
    }

    void HandleDone(TEvents::TEvDone::TPtr& ev) {
        Y_UNUSED(ev);
        PendingCalculations--;
        CheckCompletion();
    }

    void CheckCompletion() {
        if (FinishedReading && PendingCalculations == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoison>());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int64_t Value;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    int64_t Result = 1;
    int64_t CurrentDivisor = 1;
    int64_t MaxDivisor;
    bool Calculated = false;
    TInstant StartTime;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId)
        : Value(value)
        , ReadActorId(readActorId)
        , WriteActorId(writeActorId)
    {
        MaxDivisor = static_cast<int64_t>(std::sqrt(value));
        if (MaxDivisor < 1) MaxDivisor = 1;
    }

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        StartTime = TInstant::Now();
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        if (!Calculated) {
            auto iterationStartTime = TInstant::Now();
            
            while (CurrentDivisor <= MaxDivisor) {
                if (Value % CurrentDivisor == 0) {
                    if (IsPrime(CurrentDivisor)) {
                        Result = CurrentDivisor;
                    }
                    
                    int64_t pairDivisor = Value / CurrentDivisor;
                    if (pairDivisor != CurrentDivisor && IsPrime(pairDivisor)) {
                        Result = std::max(Result, pairDivisor);
                    }
                }
                
                CurrentDivisor++;
                
                auto elapsed = TInstant::Now() - iterationStartTime;
                if (elapsed > TDuration::MilliSeconds(10)) {
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            
            Calculated = true;
        }
        
        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(Result));
        
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        
        PassAway();
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
private:
    int64_t Sum = 0;

public:
    TWriteActor()
        : TActor(&TWriteActor::StateFunc)
    {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoison::EventType, HandlePoison);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoison() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop(0);
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId& writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
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
