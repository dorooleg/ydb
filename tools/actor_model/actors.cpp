#include "actors.h"
#include "events.h"

#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <library/cpp/actors/core/events.h>
#include <util/system/types.h>
#include <iostream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor
    : public NActors::TActorBootstrapped<TReadActor>
{
    std::istream* In;
    NActors::TActorId WriteActorId;
    int PendingCount = 0;
    bool EofReached = false;

public:
    TReadActor(std::istream& in, NActors::TActorId writeId)
        : In(&in)
        , WriteActorId(writeId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
        cFunc(NActors::TEvents::TEvWakeup::EventType,    OnWakeup);
        hFunc(TEvents::TEvComputationDone,                OnComputationDone);
    );

    void OnWakeup() {
        int64_t value;
        if (*In >> value) {
            ++PendingCount;
            Register(CreateMaximumPrimeDivisorActor(
                value,
                SelfId(),
                WriteActorId
            ).Release());
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            EofReached = true;
            if (PendingCount == 0) {
                Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void OnComputationDone(TEvents::TEvComputationDone::TPtr& ev) {
        --PendingCount;
        if (EofReached && PendingCount == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TMaximumPrimeDivisorActor
    : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor>
{
    ui64 Number;
    ui64 Current  = 2;
    ui64 MaxPrime = 1;
    TInstant SliceStart;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;

public:
    TMaximumPrimeDivisorActor(
        ui64 number,
        NActors::TActorId readId,
        NActors::TActorId writeId
    )
        : Number(number)
        , ReadActorId(readId)
        , WriteActorId(writeId)
    {}

    void Bootstrap() {
        SliceStart = TInstant::Now();
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
        cFunc(NActors::TEvents::TEvWakeup::EventType, OnWakeup);
    );

    void OnWakeup() {
        while (Current <= Number) {
            if (Number % Current == 0) {
                bool isPrime = true;
                for (ui64 j = 2; j * j <= Current; ++j) {
                    if (Current % j == 0) {
                        isPrime = false;
                        break;
                    }
                    if (TInstant::Now() - SliceStart > TDuration::MilliSeconds(10)) {
                        SliceStart = TInstant::Now();
                        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                        return;
                    }
                }
                if (isPrime && Current > MaxPrime) {
                    MaxPrime = Current;
                }
            }

            ++Current;
            if (TInstant::Now() - SliceStart > TDuration::MilliSeconds(10)) {
                SliceStart = TInstant::Now();
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteRequest>(MaxPrime));
        Send(ReadActorId,  std::make_unique<TEvents::TEvComputationDone>(false));
        PassAway();
    }
};

class TWriteActor
    : public NActors::TActor<TWriteActor>
{
    ui64 Sum = 0;

public:
    using TBase = NActors::TActor<TWriteActor>;

    TWriteActor()
        : TBase(&TWriteActor::StateFunc)
    {}

    STRICT_STFUNC(StateFunc,
        hFunc(TEvents::TEvWriteRequest,   OnWriteRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, OnPoison);
    );

    void OnWriteRequest(TEvents::TEvWriteRequest::TPtr& ev) {
        Sum += ev->Get()->PrimeDivisor;
    }

    void OnPoison() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

class TSelfPingActor
    : public NActors::TActorBootstrapped<TSelfPingActor>
{
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
        Y_VERIFY(now - LastTime <= Latency, "Latency too big");
        LastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder<NActors::IActor> CreateReadActor(std::istream& in, NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(in, writeActorId);
}

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(
    ui64 number,
    NActors::TActorId readActorId,
    NActors::TActorId writeActorId
) {
    return MakeHolder<TMaximumPrimeDivisorActor>(number, readActorId, writeActorId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
