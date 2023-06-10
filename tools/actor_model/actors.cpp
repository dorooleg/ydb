#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId WriteActor;
    int count;
    bool finish;
public:
    TReadActor(NActors::TActorId writeActor)
            : WriteActor(writeActor), count(0), finish(false) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeUp() {
        int value;
        if (std::cin >> value) {
            Register(
                    CreateSelfTMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
        else{
            finish = true;
        }
    }

    void HandleDone() {
        count--;
        if (finish && count == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};



class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int Value;
    int answer;
    int ObtainedInThePreviousIterationI;
    int ObtainedInThePreviousIterationJ;
    int ObtainedInThePreviousIterationCurrentCountDividers;
    NActors::TActorIdentity ReadActor;
    NActors::TActorId WriteActor;

public:
    TMaximumPrimeDevisorActor(int value, NActors::TActorIdentity readActor, NActors::TActorId writeActor)
            : Value(value), ReadActor(readActor),
              WriteActor(writeActor), ObtainedInThePreviousIterationI(1), ObtainedInThePreviousIterationJ(1),
              ObtainedInThePreviousIterationCurrentCountDividers(0), answer(1) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        auto startTime = std::chrono::steady_clock::now();
        auto endTime =  std::chrono::steady_clock::now();
        for (int i = ObtainedInThePreviousIterationI; i <= Value; i++) {
            if(Value % i == 0) {
                int currentCountDividers = ObtainedInThePreviousIterationCurrentCountDividers;
                for (int j = ObtainedInThePreviousIterationJ; j <= i; j++) {
                    if (i % j == 0) {
                        currentCountDividers++;
                    }
                    endTime = std::chrono::steady_clock::now();
                    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - startTime).count();
                    if (elapsedTime > 10) {
                        ObtainedInThePreviousIterationI = i;
                        ObtainedInThePreviousIterationJ = j + 1;
                        ObtainedInThePreviousIterationCurrentCountDividers = currentCountDividers;
                        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                        return;
                    }
                }
                if (currentCountDividers == 2) {
                    if(i > answer){
                        answer = i;
                    }
                }
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
        sum = sum + event.Value;
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

THolder <NActors::IActor> CreateSelfTReadActor(NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder <NActors::IActor> CreateSelfTMaximumPrimeDevisorActor(int value, NActors::TActorIdentity readActor,
                                                              NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder <NActors::IActor> CreateSelfTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr <TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId WriteActor;
    int count;
    bool finish;
public:
    TReadActor(NActors::TActorId writeActor)
            : WriteActor(writeActor), count(0), finish(false) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeUp() {
        int value;
        if (std::cin >> value) {
            Register(
                    CreateSelfTMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
        else{
            finish = true;
        }
    }

    void HandleDone() {
        count--;
        if (finish && count == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};



class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    int Value;
    int answer;
    int ObtainedInThePreviousIterationI;
    int ObtainedInThePreviousIterationJ;
    int ObtainedInThePreviousIterationCurrentCountDividers;
    NActors::TActorIdentity ReadActor;
    NActors::TActorId WriteActor;

public:
    TMaximumPrimeDevisorActor(int value, NActors::TActorIdentity readActor, NActors::TActorId writeActor)
            : Value(value), ReadActor(readActor),
              WriteActor(writeActor), ObtainedInThePreviousIterationI(1), ObtainedInThePreviousIterationJ(1),
              ObtainedInThePreviousIterationCurrentCountDividers(0), answer(1) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        auto startTime = std::chrono::steady_clock::now();
        auto endTime =  std::chrono::steady_clock::now();
        for (int i = ObtainedInThePreviousIterationI; i <= Value; i++) {
            if(Value % i == 0) {
                int currentCountDividers = ObtainedInThePreviousIterationCurrentCountDividers;
                for (int j = ObtainedInThePreviousIterationJ; j <= i; j++) {
                    if (i % j == 0) {
                        currentCountDividers++;
                    }
                    endTime = std::chrono::steady_clock::now();
                    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - startTime).count();
                    if (elapsedTime > 10) {
                        ObtainedInThePreviousIterationI = i;
                        ObtainedInThePreviousIterationJ = j + 1;
                        ObtainedInThePreviousIterationCurrentCountDividers = currentCountDividers;
                        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                        return;
                    }
                }
                if (currentCountDividers == 2) {
                    if(i > answer){
                        answer = i;
                    }
                }
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
        sum = sum + event.Value;
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

THolder <NActors::IActor> CreateSelfTReadActor(NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder <NActors::IActor> CreateSelfTMaximumPrimeDevisorActor(int value, NActors::TActorIdentity readActor,
                                                              NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder <NActors::IActor> CreateSelfTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr <TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
