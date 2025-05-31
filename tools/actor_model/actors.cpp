#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool Finish;
    const NActors::TActorId WriteActorId;
    int Count = 0;

public:
    TReadActor(const NActors::TActorId writeActorId)
        : Finish(false), WriteActorId(writeActorId)
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
        int64_t value;
        if (std::cin >> value) {
            Register(CreateMaximumPrimeDivisorActor(value, SelfId(), WriteActorId).Release());
            Count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Finish = true;
            if (Count == 0) {
                Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone() {
        Count--;
        if (Finish && Count == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}



class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
    const NActors::TActorId ReadActorId;
    const NActors::TActorId WriteActorId;
    int64_t InitialValue;
    int64_t Value;
    int64_t CurrentDivisor = 2;
    int64_t MaxDivisor = 0;

public:
    TMaximumPrimeDivisorActor(const int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId)
            : ReadActorId(readActorId), WriteActorId(writeActorId), InitialValue(abs(value)), Value(abs(value))
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto start = TInstant::Now();
        TInstant end;
        TDuration max_delay = TDuration::MilliSeconds(10);
        for (int64_t i=CurrentDivisor; i <= sqrt(Value); i+=(i==2?1:2)) {
            CurrentDivisor = i;

            while (Value % CurrentDivisor == 0) {
                MaxDivisor = CurrentDivisor;
                Value /= CurrentDivisor;
                end = TInstant::Now();
                if (end - start >= max_delay) {
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            end = TInstant::Now();
            if (end - start >= max_delay) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        if (Value >= 2) {
            MaxDivisor = Value;
        }

        if (InitialValue == 1) {
            MaxDivisor = 1;
        }

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(const int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId) {
    return MakeHolder<TMaximumPrimeDivisorActor>(value, readActorId, writeActorId);
}


class TWriterActor : public NActors::TActorBootstrapped<TWriterActor> {
    int64_t Sum = 0;

public:
    TWriterActor()
    {}

    void Bootstrap() {
        Become(&TWriterActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& event) {
        int64_t val = (*event->Get()).Value;
        Sum += val;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }


};

THolder<NActors::IActor> CreateWriterActor() {
    return MakeHolder<TWriterActor>();
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
