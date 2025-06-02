#pragma once

#include <iostream>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/core/events.h>
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/actorid.h>
#include <library/cpp/actors/core/hfunc.h>
#include "events.h"

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
public:
    TReadActor(std::istream& input, const NActors::TActorId& writeActor);

    void Bootstrap();
    void Handle(NActors::TEvents::TEvWakeup::TPtr& ev);
    void Handle(TEvents::TEvDone::TPtr& ev);
    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, Handle);
        hFunc(TEvents::TEvDone, Handle);
    });

    static constexpr char ActorName[] = "TREAD_ACTOR";

private:
    std::istream& Input;
    NActors::TActorId WriteActor;
    size_t InFlight = 0;
    bool FinishedReading = false;
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId sender, NActors::TActorId writeActor);

    void Bootstrap();
    void Handle(NActors::TEvents::TEvWakeup::TPtr&);
    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, Handle);
    });

    static constexpr char ActorName[] = "MAX_PRIME_DIV_ACTOR";

private:
    int64_t Value;
    int64_t CurrentDivisor;
    int64_t MaxPrime;
    NActors::TActorId Sender;
    NActors::TActorId WriteActor;
};

class TWriteActor : public NActors::TActor<TWriteActor> {
public:
    TWriteActor();

    void Receive(TAutoPtr<NActors::IEventHandle>& ev);

    static constexpr char ActorName[] = "TWRITE_ACTOR";

private:
    int64_t Sum = 0;
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId sender, NActors::TActorId writeActor);
ui64 GetElapsedMicroSeconds(ui64 start);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
