#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <util/stream/input.h>
#include <util/stream/output.h>
#include <util/system/types.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    IInputStream& Strm;
    NActors::TActorId WriteActorId;
    size_t ActiveWorkers = 0;
    bool Finished = false;

    TVector<int64_t> Numbers;
    size_t CurrentIndex = 0;

public:
    TReadActor(IInputStream& strm, const NActors::TActorId& writeActorId)
        : Strm(strm)
        , WriteActorId(writeActorId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        if (CurrentIndex < Numbers.size()) {
            int64_t value = Numbers[CurrentIndex++];
            auto actor = CreateMaximumPrimeDivisorActor(value, SelfId(), WriteActorId);
            Register(actor.Release());
            ActiveWorkers++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            return;
        }

        TString line;
        if (Strm.ReadLine(line)) {
            Numbers.clear();
            CurrentIndex = 0;

            TVector<TString> tokens = StringSplitter(line).Split(' ').SkipEmpty();
            for (const auto& tok : tokens) {
                int64_t value = FromString<int64_t>(tok);
                Numbers.push_back(value);
            }

            if (Numbers.empty()) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            } else {
                int64_t value = Numbers[CurrentIndex++];
                auto actor = CreateMaximumPrimeDivisorActor(value, SelfId(), WriteActorId);
                Register(actor.Release());
                ActiveWorkers++;
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
        } else {
            Finished = true;
            if (ActiveWorkers == 0) {
                Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            }
        }
    }

    void HandleDone(const TEvents::TEvDone::TPtr&) {
        ActiveWorkers--;
        if (Finished && ActiveWorkers == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
private:
    int64_t Value;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    int64_t CurrentDivisor = 2;
    int64_t MaxPrimeDivisor = -1;

public:
    TMaximumPrimeDivisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId)
        : Value(value)
        , ReadActorId(readActorId)
        , WriteActorId(writeActorId)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
    TInstant start = TInstant::Now();
    int64_t n = Value < 0 ? -Value : Value;

    if (n <= 1) {
        MaxPrimeDivisor = 1;
        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxPrimeDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
        return;
    }

    while (CurrentDivisor * CurrentDivisor <= n && (TInstant::Now() - start) < TDuration::MilliSeconds(10)) {
        if (n % CurrentDivisor == 0) {
            MaxPrimeDivisor = CurrentDivisor;
            while (n % CurrentDivisor == 0) {
                n /= CurrentDivisor;
            }
        }
        ++CurrentDivisor;
    }

    if (n > 1 && (TInstant::Now() - start) < TDuration::MilliSeconds(10)) {
        MaxPrimeDivisor = n;
    }

    if (CurrentDivisor * CurrentDivisor > n || n == 1) {
        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxPrimeDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    } else {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
  }

};

class TWriteActor : public NActors::TActor<TWriteActor> {
private:
    int64_t Sum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValue);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValue(const TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        Cout << Sum << Endl;
        ShouldContinue->ShouldStop(0);
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(IInputStream& strm, const NActors::TActorId& writeActorId) {
    return MakeHolder<TReadActor>(strm, writeActorId);
}

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId) {
    return MakeHolder<TMaximumPrimeDivisorActor>(value, readActorId, writeActorId);
}

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
