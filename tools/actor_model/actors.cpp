#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

// Read Actor
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::istream& InputStream;
    NActors::TActorId WriteActor;
    int PendingActors = 0;

public:
    TReadActor(std::istream& inputStream, NActors::TActorId writeActor)
        : InputStream(inputStream), WriteActor(writeActor) {}

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TReadActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, TEvWakeup);
        hFunc(TEvDone, TEvDone);
    });

    void TEvWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        int64_t value;
        if (InputStream >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            ++PendingActors;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else if (PendingActors == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }

    void TEvDone(TEvDone::TPtr) {
        --PendingActors;
        if (PendingActors == 0 && InputStream.eof()) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateReadActor(std::istream& inputStream, NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(inputStream, writeActor);
}

// Maximum Prime Divisor Actor
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;
    int64_t CurrentDivisor;
    int64_t MaximumPrimeDivisor;

public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor), CurrentDivisor(2), MaximumPrimeDivisor(1) {}

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TMaximumPrimeDevisorActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, TEvWakeup);
    });

    void TEvWakeup() {
        auto startTime = TInstant::Now();
        while (CurrentDivisor <= Value) {
            if (Value % CurrentDivisor == 0 && IsPrime(CurrentDivisor)) {
                MaximumPrimeDivisor = CurrentDivisor;
            }
            ++CurrentDivisor;
            if (TInstant::Now() - startTime > TDuration::MilliSeconds(10)) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }
        Send(WriteActor, std::make_unique<TEvWriteValueRequest>(MaximumPrimeDivisor));
        Send(ReadActor, std::make_unique<TEvDone>());
        PassAway();
    }

    bool IsPrime(int64_t number) {
        if (number <= 1) return false;
        for (int64_t i = 2; i * i <= number; ++i) {
            if (number % i == 0) return false;
        }
        return true;
    }
};

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

// Write Actor
class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvWriteValueRequest, TEvWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, TEvPoisonPill);
    });

    void TEvWriteValueRequest(TEvWriteValueRequest::TPtr ev) {
        Sum += ev->Get()->Value;
    }

    void TEvPoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

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
