#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    const NActors::TActorId Writer;
    bool IsFinished;
    size_t PendingTasks;

public:
    TReadActor(const NActors::TActorId writer)
        : Writer(writer), IsFinished(false), PendingTasks(0) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandlerWakeup);
        cFunc(TEvents::TEvDone::EventType, HandlerDevisor);
    });

    void HandlerWakeup() {
        int64_t value;
        if (std::cin >> value) {
            IsFinished = false;
            Register(CreateTMaximumPrimeDevisorActor(SelfId(), Writer, value).Release());
            ++PendingTasks;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (PendingTasks == 0) {
                Send(Writer, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
            IsFinished = true;
        }
    }

    void HandlerDevisor() {
        --PendingTasks;
        if (IsFinished && PendingTasks == 0) {
            Send(Writer, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
private:
    const NActors::TActorId ReaderActorId;
    const NActors::TActorId WriterActorId;
    int64_t CurrentDivisor = 2;
    int64_t MaxDivisor = 0;
    int64_t Value;
    int64_t Copy;

public:
    TMaximumPrimeDevisorActor(const NActors::TActorId reader, const NActors::TActorId writer, int64_t value)
        : ReaderActorId(reader), WriterActorId(writer), Value(value), Copy(value) {}
    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeup() {
        TInstant start = TInstant::Now();
        for (int64_t i = CurrentDivisor; i <= sqrt(Value); i += (i == 2 ? 1 : 2)) {
            CurrentDivisor = i;
            while (Value % CurrentDivisor == 0) {
                MaxDivisor = CurrentDivisor;
                Value /= CurrentDivisor;
                if (TInstant::Now() - start >= TDuration::MilliSeconds(10)) {
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            if (TInstant::Now() - start >= TDuration::MilliSeconds(10)) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }
        if (Value >= 2) {
            MaxDivisor = Value;
        }
        if (Copy == 1) {
            MaxDivisor = 1;
        }
        Send(WriterActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxDivisor));
        Send(ReaderActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
private:
    int64_t sum = 0;

public:
    TWriteActor() {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleDevisor);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleFinish);
    });

    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    void HandleFinish() {
        std::cout << sum << "\n";
        ShouldContinue->ShouldStop();
        PassAway();
    }

    void HandleDevisor(TEvents::TEvWriteValueRequest::TPtr& message) {
        sum += message->Get()->value;
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

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writer) {
    return MakeHolder<TReadActor>(writer);
}

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(const NActors::TActorIdentity reader, const NActors::TActorId writer, int64_t value) {
    return MakeHolder<TMaximumPrimeDevisorActor>(reader, writer, value);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}
