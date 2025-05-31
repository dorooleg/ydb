#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    const NActors::TActorId WriteActor;
    int64_t ActorsCount = 0;
    bool EOS = false;

public:
    TReadActor(const NActors::TActorId& writeActorId)
        : WriteActor(writeActorId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t val;
        if (std::cin >> val) {
            ActorsCount++;

            // CREATE NEW TMaximumPrimeDevisorActor
            Register(CreateMaximumPrimeDevisorActor(val, SelfId(), WriteActor).Release());

            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            EOS = true;
            if (ActorsCount == 0) Finish();
        }
    }

    void HandleDone() {
        ActorsCount--;
        if (ActorsCount == 0 && EOS) Finish();
    }

    void Finish() {
        Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        PassAway();
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    const NActors::TActorId ReadActor;
    const NActors::TActorId WriteActor;
    int64_t Divisor = 1;
    int64_t MaxPrime = 1;

public:
    TMaximumPrimeDevisorActor(
        const int64_t value, 
        const NActors::TActorId& readActorId, 
        const NActors::TActorId& writeActorId)
            : Value(value), ReadActor(readActorId), WriteActor(writeActorId)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    bool isOverdued(TInstant start) {
        if ((TInstant::Now() - start) > TDuration::MilliSeconds(10)) {
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            return true;
        }

        return false;
    }

    void HandleWakeup() {
        TInstant start = TInstant::Now();

        // firstly dividing by 2
        while (Value % 2 == 0) {
            if (isOverdued(start)) return;
            MaxPrime = 2;
            Value /= 2;
        }

        // dividing by each value starting from 3, skipping every even divisor
        Divisor = 3;
        while (Divisor * Divisor <= Value) {
            while (Value % Divisor == 0) {
                if (isOverdued(start)) return;
                MaxPrime = Divisor;
                Value /= Divisor;
            }
            
            Divisor += 2;
        }
        if (Value > 1) {
            MaxPrime = Value;
        }

        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(MaxPrime));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
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

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId& writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(
    int64_t value,
    const NActors::TActorId& readActorId,
    const NActors::TActorId& writeActorId) {
        return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
}
