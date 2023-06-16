#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId Target;
    int HandledEvents;
    bool readDone;
public:
    TReadActor(NActors::TActorId writeActor)
            : Target(writeActor), HandledEvents(0), readDone(false) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleCalculationDone);
    });

    void HandleWakeUp() {
        int value;
        if (std::cin >> value) {
            HandledEvents++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            readDone = true;
        }
    }

    void HandleCalculationDone() {
        HandledEvents--;
        if (readDone && HandledEvents == 0) {
            Send(Target, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int value;
    int answer;
    int IValue;
    int JValue;
    NActors::TActorIdentity ReadActor;
    NActors::TActorId WriteActor;

public:
    TMaximumPrimeDevisorActor(int Value, NActors::TActorIdentity readActor, NActors::TActorId writeActor)
            : value(Value), ReadActor(readActor),
              WriteActor(writeActor), IValue(1),
              answer(1), JValue(2) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        auto startTime = std::chrono::high_resolution_clock::now();
        for (int i = IValue; i < value; i++) {
            bool isPrime = true;
            for (int j = JValue; j * j < i; j++) {
                if (i % j == 0) {
                    isPrime = false;
                    break;
                }
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - startTime).count() > 10) {
                    IValue = i;
                    JValue = j;
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            if (value % i && isPrime && answer < i) {
                answer = i;
            }
        }
        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(answer));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};


class TWriteActor : public NActors::TActor<TWriteActor> {
private:
    int sum;
public:
    using TBase = NActors::TActor<TWriteActor>;

    TWriteActor() : TBase(&TWriteActor::Handler), sum(0) {}

    STRICT_STFUNC(Handler,
    {
        hFunc(TEvents::TEvWriteValueRequest, HandleWakeUp);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });

    void HandleWakeUp(TEvents::TEvWriteValueRequest::TPtr &ev) {
        auto &event = *ev->Get();
        sum = sum + event.value;
    }

    void HandleDone() {
        std::cout << sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration &latency)
            : Latency(latency) {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
    STRICT_STFUNC(StateFunc,
    {
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

THolder <NActors::IActor> CreateSelfPingActor(const TDuration &latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

THolder <NActors::IActor> CreateSelfTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder <NActors::IActor>
CreateSelfTMaximumPrimeDevisorActor(int value, NActors::TActorIdentity readActor, NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder <NActors::IActor> CreateSelfTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr <TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}