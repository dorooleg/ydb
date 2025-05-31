#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <util/string/cast.h>
#include <util/string/split.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor;
class TWriteActor;

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    int64_t CurrentDivisor;
    int64_t MaxPrimeDivisor = 1;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    TInstant StartTime;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId)
        : Value(value)
        , CurrentDivisor(2)
        , ReadActorId(readActorId)
        , WriteActorId(writeActorId)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        StartTime = TInstant::Now();
        
        while (CurrentDivisor * CurrentDivisor <= Value) {
            if (Value % CurrentDivisor == 0) {
                MaxPrimeDivisor = CurrentDivisor;
                while (Value % CurrentDivisor == 0) {
                    Value /= CurrentDivisor;
                }
            }
            CurrentDivisor++;

            if (TInstant::Now() - StartTime > TDuration::MilliSeconds(10)) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }
        
        if (Value > 1) {
            MaxPrimeDivisor = Value;
        }
        
        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxPrimeDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    IInputStream& Strm;
    NActors::TActorId WriteActorId;
    size_t ActiveActors = 0;
    bool FinishedReading = false;
    TString Buffer;

public:
    TReadActor(IInputStream& strm, const NActors::TActorId& writeActorId)
        : Strm(strm)
        , WriteActorId(writeActorId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Strm.ReadLine(Buffer);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        TVector<TString> tokens;
        StringSplitter(Buffer).Split(' ').SkipEmpty().Collect(&tokens);
        
        if (tokens.empty()) {
            FinishedReading = true;
            CheckFinish();
            return;
        }
        
        for (const auto& token : tokens) {
            try {
                int64_t value = FromString<int64_t>(token);
                auto actor = new TMaximumPrimeDevisorActor(value, SelfId(), WriteActorId);
                Register(actor);
                ActiveActors++;
            } catch (...) {
            }
        }
        
        FinishedReading = true;
        CheckFinish();
    }

    void HandleDone(TEvents::TEvDone::TPtr&) {
        ActiveActors--;
        CheckFinish();
    }

    void CheckFinish() {
        if (FinishedReading && ActiveActors == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;
    IOutputStream& OutStream;

public:
    TWriteActor(IOutputStream& outStream)
        : NActors::TActor<TWriteActor>(&TWriteActor::StateFunc)
        , OutStream(outStream)
    {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValue);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValue(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        OutStream << Sum << Endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

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

THolder<NActors::IActor> CreateReadActor(IInputStream& strm, const NActors::TActorId& writeActorId) {
    return MakeHolder<TReadActor>(strm, writeActorId);
}

THolder<NActors::IActor> CreateWriteActor(IOutputStream& outStream) {
    return MakeHolder<TWriteActor>(outStream);
}
