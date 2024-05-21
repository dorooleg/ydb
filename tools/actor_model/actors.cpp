#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <chrono>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool Finished = false;
    const NActors::TActorId WriteActor;
    int PendingActors = 0;

public:
    TReadActor(const NActors::TActorId writeActor)
        : WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeUp() {
        int64_t value;
        if (std::cin >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            PendingActors++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Finished = true;
            if (PendingActors == 0) {
                SendPoisonPill();
            }
        }
    }

    void HandleDone() {
        PendingActors--;
        if (Finished && PendingActors == 0) {
            SendPoisonPill();
        }
    }

    void SendPoisonPill() {
        Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        PassAway();
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            sFunc(NActors::TEvents::TEvWakeup, HandleWakeUp);
            sFunc(TEvents::TEvDone, HandleDone);
            default:
                break;
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    const NActors::TActorId ReadActor;
    const NActors::TActorId WriteActor;
    int64_t LargestPrimeDevisor = 1;
    int64_t CurrentDevisor = 2;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId readActor, const NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeUp() {
        if(Value < 1){
            Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(0));
            Send(ReadActor, std::make_unique<TEvents::TEvDone>());
            PassAway();
            return;
        }
        auto startTime = std::chrono::steady_clock::now();

        while (CurrentDevisor * CurrentDevisor <= Value) {
            if (Value % CurrentDevisor == 0) {
                LargestPrimeDevisor = CurrentDevisor;
                Value /= CurrentDevisor;
            } else {
                CurrentDevisor++;
            }
            auto runTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
            if (runTime > 10) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        if (Value > 1) {
            LargestPrimeDevisor = Value;
        }

        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(LargestPrimeDevisor));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            sFunc(NActors::TEvents::TEvWakeup, HandleWakeUp);
            default:
                break;
        }
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
            sFunc(NActors::TEvents::TEvPoisonPill, HandlePoisonPill);
            default:
               break;
        }
    }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency)
        : Latency(latency) {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeup() {
        auto now = TInstant::Now();
        TDuration delta = now - LastTime;
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            sFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
            default:
                break;
        }
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId readActor, const NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
