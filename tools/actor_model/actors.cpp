#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

// region ActorClasses

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::istream& Stream;
    NActors::TActorId Recipient;
    bool InputEnded;
    int Count = 0;

public:
    TReadActor(std::istream& stream, NActors::TActorId recipient) :
        Stream(stream),
        Recipient(recipient),
        InputEnded()
        {}

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TReadActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t num;

        if (Stream >> num) {
            Register(CreateMaximumPrimeDivisorActor(num, SelfId(), Recipient).Release());
            Count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            InputEnded = true;
            if (Count == 0) {
                Send(Recipient, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone() {
        Count--;
        if (InputEnded && Count == 0) {
            Send(Recipient, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
    int64_t Num;
    NActors::TActorId ReadActorID, WriteActorID;
    int64_t CurDivisor = 1, MaxDivisor = 0;

public:
    TMaximumPrimeDivisorActor(int64_t num, NActors::TActorId readActorID, NActors::TActorId writeActorID) :
        Num(num),
        ReadActorID(readActorID),
        WriteActorID(writeActorID)
        {}

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TMaximumPrimeDivisorActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleDivision);
    });

    void HandleDivision() {
        auto now = TInstant::Now();

        while(CurDivisor <= Num) {

            if (Num == 1) { MaxDivisor = 1; CurDivisor++; }

            // пригодилось всё-таки егэ по информатике...
            bool isPrime = true;
            for (int64_t i = 2; i * i <= CurDivisor; i++) if (CurDivisor % i == 0) { isPrime = false; break; }
            if (Num % CurDivisor == 0 && isPrime) MaxDivisor = CurDivisor;
            CurDivisor++;

            if (TInstant::Now() - now > TDuration::MilliSeconds(10)) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
        }

        Send(WriteActorID, std::make_unique<TEvents::TEvWriteValueRequest>(MaxDivisor));
        Send(ReadActorID, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t TotalSum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisoning);
    });

    void HandleRequest(TEvents::TEvWriteValueRequest::TPtr ev) {
        TotalSum += ev -> Get() -> Num;
    }

    void HandlePoisoning() {
        Cout << TotalSum << Endl;
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

//endregion

//region THolders

THolder<NActors::IActor> CreateReadActor(std::istream& stream, NActors::TActorId recipient) {
    return MakeHolder<TReadActor>(stream, recipient);
}

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(int64_t num, NActors::TActorId readActorID, NActors::TActorId writeActorID) {
    return MakeHolder<TMaximumPrimeDivisorActor>(num, readActorID, writeActorID);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

//endregion

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
