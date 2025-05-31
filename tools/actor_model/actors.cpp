#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>


static auto ProgramContinue = std::make_shared<TProgramShouldContinue>();


class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool Completed = false;
    const NActors::TActorId WriterActorId;
    int RunningActors;
    bool InitialRun;
public:
    TReadActor(const NActors::TActorId& writerId) : WriterActorId(writerId), RunningActors(0), InitialRun(true) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            InitialRun = false;
            Register(CreateTMaximumPrimeDevisorActor(value, SelfId(), WriterActorId).Release());
            RunningActors++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (InitialRun) {
                Register(CreateTMaximumPrimeDevisorActor(0, SelfId(), WriterActorId).Release());
                RunningActors++;
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
            Completed = true;
        }
    }

    void HandleDone() {
        RunningActors--;
        if (Completed && RunningActors == 0) {
            Send(WriterActorId, std::make_unique<TEvents::TEvPoisonPill>());
        }
    }
};


class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t ValueToProcess;
    const NActors::TActorIdentity ReaderActor;
    const NActors::TActorId WriterActor;
    int64_t LargestPrime;
    int64_t NextCheckValue;
    bool IsPrime;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity& readerId, const NActors::TActorId& writerId)
        : ValueToProcess(value), ReaderActor(readerId), WriterActor(writerId), LargestPrime(0), NextCheckValue(1), IsPrime(true) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto StartTime = std::chrono::high_resolution_clock::now();

        while (NextCheckValue <= ValueToProcess) {
            for (int64_t i = 2; i * i <= NextCheckValue; i++) {
                if (NextCheckValue % i == 0) {
                    IsPrime = false;
                    break;
                }
                auto CurrentTime = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - StartTime).count() > 10) {
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }

            if (IsPrime && ValueToProcess % NextCheckValue == 0) {
                LargestPrime = NextCheckValue;
            }
            NextCheckValue++;
        }

        Send(WriterActor, std::make_unique<TEvents::TEvWriteValueRequest>(LargestPrime));
        Send(ReaderActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t TotalSum;

public:
    using TBase = NActors::TActor<TWriteActor>;

    TWriteActor() : TBase(&TWriteActor::Handler), TotalSum(0) {}

    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        TotalSum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << TotalSum << std::endl;
        ProgramContinue->ShouldStop();
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


THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}


THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}


THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}


THolder <NActors::IActor> CreateSelfPingActor(const TDuration &latency) {
    return MakeHolder<TSelfPingActor>(latency);
}


std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ProgramContinue;
}
