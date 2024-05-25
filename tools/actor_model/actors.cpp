#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor
*/


// TODO: напишите реализацию TReadActor
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    int countWorker = 0;
    bool endInput = false;

    std::istream& Instream;
    NActors::TActorId WriteActor;

    //определяем состояние актора
    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandlerWakeup);
        hFunc(TEvents::TEvDone, HandlerDone);
    });

public:
    TReadActor(std::istream& inputStream, NActors::TActorId writeActor) : Instream(inputStream), WriteActor(writeActor) {}

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TReadActor::StateFunc);
    }

    void HandlerWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        int64_t value;
        if (Instream >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            countWorker++;
        } else {
            endInput = true;
            if (countWorker == 0) {
                Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }
    void HandlerDone(TEvents::TEvDone::TPtr) {
        countWorker--;
        if (endInput && countWorker == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};
THolder<NActors::IActor> CreateReadActor(std::istream& inputStream, NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(inputStream, writeActor);
}



// TODO: напишите реализацию TMaximumPrimeDevisorActor
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;
    int64_t CurPrimeDevAct = 2;
    
    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandlerWakeup);
    });

    void HandlerWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        TInstant start = TInstant::Now();
        int64_t MaxPrimeDevAct = 1;

        while (CurPrimeDevAct <= Value) {
            if (Value % CurPrimeDevAct == 0) {
                MaxPrimeDevAct = CurPrimeDevAct;

                do {
                    Value /= CurPrimeDevAct;
                } while (Value % CurPrimeDevAct == 0);
            }

            CurPrimeDevAct++;

            if (TInstant::Now() - start > TDuration::MilliSeconds(10)) {
                Send(SelfId(), new NActors::TEvents::TEvWakeup());
                return;
            }
        }

        auto result = MakeHolder<TEvents::TEvWriteValueRequest>(MaxPrimeDevAct);
        Send(WriteActor, result.Release());
        Send(ReadActor, new TEvents::TEvDone());
        PassAway();
    }

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }
};
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t Value, NActors::TActorId ReadActor, NActors::TActorId WriteActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(Value, ReadActor, WriteActor);
}

// TODO: напишите реализацию TWriteActor
class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandlerWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlerPoisonPill);
    });

    void HandlerWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr ev) {
        Sum += ev->Get()->Value;
    }

    void HandlerPoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

public:
    TWriteActor() :
        TActor(&TWriteActor::StateFunc) {}
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
