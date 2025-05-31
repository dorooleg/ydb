#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <util/system/mutex.h>
#include <util/system/datetime.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Number;
    int64_t MaxPrime = 0;
    int64_t Current = 2;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;

public:
    TMaximumPrimeDevisorActor(int64_t number, NActors::TActorId readActor, NActors::TActorId writeActor)
            : Number(number), ReadActor(readActor), WriteActor(writeActor) {}

    void Bootstrap() {
        if (Number == 1) {
            MaxPrime = 1;
            Send(WriteActor, new TEvents::TEvWriteValueRequest(MaxPrime));
            Send(ReadActor, new TEvents::TEvDone());
            PassAway();
            return;
        }

        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc,
            cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    )

    void HandleWakeup() {
        auto start = TInstant::Now();

        while (Current <= Number && TInstant::Now() - start < TDuration::MilliSeconds(10)) {
            if (Number % Current == 0 && IsPrime(Current)) {
                MaxPrime = Current;
            }
            ++Current;
        }

        if (Current > Number) {
            if (MaxPrime > 0) {
                Send(WriteActor, new TEvents::TEvWriteValueRequest(MaxPrime));
            }
            Send(ReadActor, new TEvents::TEvDone());
            PassAway();
        }
        else {
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }
    }

    bool IsPrime(int64_t n) {
        if (n <= 1) return false;
        for (int64_t i = 2; i * i <= n; ++i)
            if (n % i == 0)
                return false;
        return true;
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    std::shared_ptr<TProgramShouldContinue> ShouldContinue;
    int64_t Sum = 0;

public:
    TWriteActor()
            : TActor(&TWriteActor::StateFunc)
            , ShouldContinue(GetProgramShouldContinue()) {}

    STRICT_STFUNC(StateFunc,
            hFunc(TEvents::TEvWriteValueRequest, HandleWrite);
    cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoison);
    )

    void HandleWrite(const TEvents::TEvWriteValueRequest::TPtr& ev) {
        int64_t val = ev->Get()->Value;
        Sum += val;
    }

    void HandlePoison() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    NActors::TActorId WriteActor;
    int PendingResponses = 0;

public:
    TReadActor(NActors::TActorId writeActor)
            : WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc,
            cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    hFunc(TEvents::TEvDone, HandleDone);
    )

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            ++PendingResponses;
            Register(new TMaximumPrimeDevisorActor(value, SelfId(), WriteActor));
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            if (PendingResponses == 0) {
                Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
                PassAway();
            }
        }
    }

    void HandleDone(const TEvents::TEvDone::TPtr&) {
        --PendingResponses;
        if (PendingResponses == 0 && std::cin.eof()) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }
};


THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
        TDuration Latency;
        TInstant LastTime;

    public:
        TSelfPingActor(const TDuration& latency) : Latency(latency) {}

        void Bootstrap() {
            LastTime = TInstant::Now();
            Become(&TSelfPingActor::StateFunc);
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }

        STRICT_STFUNC(StateFunc,
                cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        )

        void HandleWakeup() {
            auto now = TInstant::Now();
            TDuration delta = now - LastTime;
            Y_VERIFY(delta <= Latency, "Latency too big");
            LastTime = now;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }
    };

    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}
