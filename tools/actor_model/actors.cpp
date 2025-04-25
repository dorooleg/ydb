#include "actors.h"
#include "events.h"

#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

namespace {

int64_t FindMaxPrimeDivisor(int64_t x) {
    int64_t maxPrime = 1;
    while (x % 2 == 0) {
        maxPrime = 2;
        x /= 2;
    }
    for (int64_t d = 3; d * d <= x; d += 2) {
        while (x % d == 0) {
            maxPrime = d;
            x /= d;
        }
    }
    return x > 1 ? x : maxPrime;
}

class TDevisorActor final : public NActors::TActorBootstrapped<TDevisorActor> {
public:
    TDevisorActor(int64_t input, const NActors::TActorId& reader, const NActors::TActorId& writer)
        : Input(input), Reader(reader), Writer(writer) {}

    void Bootstrap() {
        int64_t result = FindMaxPrimeDivisor(Input);
        Send(Writer, std::make_unique<TEvSendValue>(result));
        Send(Reader, std::make_unique<TEvComplete>());
        PassAway();
    }

private:
    int64_t Input;
    const NActors::TActorId Reader;
    const NActors::TActorId Writer;
};

class TReaderActor final : public NActors::TActorBootstrapped<TReaderActor> {
public:
    explicit TReaderActor(const NActors::TActorId& writer)
        : Writer(writer) {}

    void Bootstrap() {
        Become(&TReaderActor::State);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

private:
    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            Register(new TDevisorActor(value, SelfId(), Writer));
            ++ActiveTasks;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            InputFinished = true;
            CheckCompletion();
        }
    }

    void HandleComplete() {
        --ActiveTasks;
        CheckCompletion();
    }

    void CheckCompletion() {
        if (InputFinished && ActiveTasks == 0) {
            Send(Writer, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }

    STRICT_STFUNC(State, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvComplete::EventType, HandleComplete);
    });

private:
    const NActors::TActorId Writer;
    int64_t ActiveTasks = 0;
    bool InputFinished = false;
};

class TWriterActor final : public NActors::TActorBootstrapped<TWriterActor> {
public:
    void Bootstrap() {
        Become(&TWriterActor::State);
    }

private:
    void HandleSendValue(const TEvSendValue::TPtr& ev) {
        SumResult += ev->Get()->Value;
    }

    void HandleFinish() {
        std::cout << SumResult << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STFUNC(State) {
        switch (ev->GetTypeRewrite()) {
            cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleFinish);
            hFunc(TEvSendValue, HandleSendValue);
        }
    }

private:
    int64_t SumResult = 0;
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
public:
    TSelfPingActor(const TDuration& delay) : Delay(delay) {}

    void Bootstrap() {
        Last = TInstant::Now();
        Become(&TSelfPingActor::State);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(State, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandlePing);
    });

    void HandlePing() {
        Y_VERIFY(TInstant::Now() - Last <= Delay);
        Last = TInstant::Now();
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

private:
    TDuration Delay;
    TInstant Last;
};

} // end anonymous namespace

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId& writerId) {
    return MakeHolder<TReaderActor>(writerId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriterActor>();
}

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}