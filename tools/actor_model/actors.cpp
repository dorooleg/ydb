#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/core/events.h>
#include <util/system/datetime.h>
#include <cmath>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {

    public: TReadActor(std::istream& strm, NActors::TActorId recipient) : inputStream(strm), writeActorId(recipient) {}
    std::istream& inputStream;
    NActors::TActorId writeActorId;
    int actorsNumber = 0;
    bool isInputFinished = false;

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TReadActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEventFinish::EventType, HandleFinished);
    });

    void HandleWakeup() {
        int64_t value;
        while (inputStream >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), writeActorId).Release());
            ++actorsNumber;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
        isInputFinished = true;
    }

    void HandleFinished() {
        --actorsNumber;
        if (isInputFinished && actorsNumber <= 0) {
            Send(writeActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }

};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {

    public: TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId recipient)
        : value(value), readActorId(readActorId), writeActorId(recipient) {}
    int64_t value;
    NActors::TActorId readActorId;
    NActors::TActorId writeActorId;
    int64_t CurrentDivisor = 1;
    int64_t MaxPrimeDivisor = 0;

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TMaximumPrimeDevisorActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        TInstant startTime = TInstant::Now();
        while (CurrentDivisor <= value) {
            if (value % CurrentDivisor == 0 && IsPrime(CurrentDivisor)) {
                MaxPrimeDivisor = CurrentDivisor;
            }
            CurrentDivisor++;
            if ((TInstant::Now() - startTime).MilliSeconds() >= 10) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
        }
        Send(writeActorId, std::make_unique<TEvents::TEventWriteValueRequest>(MaxPrimeDivisor));
        Send(readActorId, std::make_unique<TEvents::TEventFinish>());
        PassAway();
    }

    static bool IsPrime(int64_t n) {
        if (n <= 3) return true;
        if (n % 2 == 0 || n % 3 == 0) return false;
        for (int64_t i = 5; i * i <= n; i += 6) {
            if (n % i == 0 || n % (i + 2) == 0) return false;
        }
        return true;
    }
};


class TWriteActor : public NActors::TActor<TWriteActor> {

    public: TWriteActor() : TActor(&TWriteActor::StateFunc) {}
    int64_t Sum = 0;

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
        hFunc(TEvents::TEventWriteValueRequest, HandleWriteValueRequest);
    });

    void HandleWriteValueRequest(TEvents::TEventWriteValueRequest::TPtr event) {
        Sum += event->Get()->Value;
    }

    void HandlePoisonPill() {
        Cout<<Sum<<Endl;
        GetProgramShouldContinue()->ShouldStop();
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

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateReadActor(std::istream& strm, NActors::TActorId recipient) {
    return MakeHolder<TReadActor>(strm, recipient);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId recipient) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, recipient);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
