#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <sstream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

int64_t CalculateMaximumPrimeDevisor(int64_t n) {
    int64_t maxPrime = 1;
    
    while (n % 2 == 0) {
        maxPrime = 2;
        n /= 2;
    }
    
    for (int64_t i = 3; i * i <= n; i += 2) {
        while (n % i == 0) {
            maxPrime = i;
            n /= i;
        }
    }
    
    if (n > 1) {
        maxPrime = n;
    }
    
    return maxPrime;
}

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    std::istream& InputStream;
    NActors::TActorId WriteActorId;
    int WaitingResponses;

public:
    TReadActor(std::istream& inputStream, NActors::TActorId writeActorId)
        : InputStream(inputStream)
        , WriteActorId(writeActorId)
        , WaitingResponses(0)
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
        if (InputStream >> value) {
            WaitingResponses++;
            Register(new TMaximumPrimeDevisorActor(value, SelfId(), WriteActorId));
            
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else if (WaitingResponses == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }

    void HandleDone() {
        WaitingResponses--;
        if (WaitingResponses == 0 && InputStream.eof()) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int64_t Value;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    int64_t CurrentNumber;
    int64_t CurrentDivisor;
    int64_t MaxPrimeDivisor;
    TInstant StartTime;

public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId)
        : Value(value)
        , ReadActorId(readActorId)
        , WriteActorId(writeActorId)
        , CurrentNumber(value)
        , CurrentDivisor(2)
        , MaxPrimeDivisor(1)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        StartTime = TInstant::Now();

        int64_t result = CalculateMaximumPrimeDevisor(Value);

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(result));
        
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        
        PassAway();
    }
};

class TWriteActor : public NActors::TActor {
private:
    int64_t Sum;

public:
    TWriteActor()
        : TActor(&TWriteActor::StateFunc)
        , Sum(0)
    {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->Stop(0);
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

THolder<NActors::IActor> CreateReadActor(std::istream& inputStream, NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(inputStream, writeActorId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
