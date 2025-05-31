#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <util/system/getpid.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

// TReadActor implementation
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::istream& Strm;
    NActors::TActorId WriteActorId;
    size_t PendingDevisors;

public:
    TReadActor(std::istream& strm, const NActors::TActorId& writeActorId)
            : Strm(strm), WriteActorId(writeActorId), PendingDevisors(0) {}

    void Bootstrap(const NActors::TActorContext& ctx) {
        Become(&TReadActor::StateFunc);
        ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr& ev, const NActors::TActorContext& ctx) {
        Y_UNUSED(ev);
        int64_t value;
        if (Strm >> value) {
            ++PendingDevisors;
            ctx.Register(NActors::CreateMaximumPrimeDevisorActor(value, ctx.SelfID, WriteActorId).Release());
            ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvWakeup>());
        } else if (PendingDevisors == 0) {
            ctx.Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }

    void HandleDone(NActors::TEvDone::TPtr& ev, const NActors::TActorContext& ctx) {
        Y_UNUSED(ev);
        if (--PendingDevisors == 0 && !Strm.good()) {
            ctx.Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }

    void HandlePoisonPill(NActors::TEvents::TEvPoisonPill::TPtr& ev, const NActors::TActorContext& ctx) {
        Y_UNUSED(ev, ctx);
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        HFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        HFunc(NActors::TEvDone, HandleDone);
        HFunc(NActors::TEvents::TEvPoisonPill, HandlePoisonPill);
    })

    void PassAway() override {
        NActors::TActorBootstrapped<TReadActor>::PassAway();
    }
};

// TMaximumPrimeDevisorActor implementation
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    int64_t CurrentDivisor;
    int64_t MaxPrimeDivisor;

    bool IsPrime(int64_t n) {
        if (n < 2) return false;
        if (n == 2) return true;
        if (n % 2 == 0) return false;
        for (int64_t i = 3; i * i <= n; i += 2) {
            if (n % i == 0) return false;
        }
        return true;
    }

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId)
            : Value(value), ReadActorId(readActorId), WriteActorId(writeActorId), CurrentDivisor(1), MaxPrimeDivisor(1) {}

    void Bootstrap(const NActors::TActorContext& ctx) {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr& ev, const NActors::TActorContext& ctx) {
        Y_UNUSED(ev);
        TInstant start = TInstant::Now();
        while (CurrentDivisor <= Value) {
            if (TInstant::Now() - start > TDuration::MilliSeconds(10)) {
                ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
            if (Value % CurrentDivisor == 0 && IsPrime(CurrentDivisor)) {
                MaxPrimeDivisor = CurrentDivisor;
            }
            ++CurrentDivisor;
        }
        ctx.Send(WriteActorId, std::make_unique<NActors::TEvWriteValueRequest>(MaxPrimeDivisor));
        ctx.Send(ReadActorId, std::make_unique<NActors::TEvDone>());
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        HFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    })

    void PassAway() override {
        NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>::PassAway();
    }
};

// TWriteActor implementation
class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc), Sum(0) {}

    void HandleWriteValueRequest(NActors::TEvWriteValueRequest::TPtr& ev, const NActors::TActorContext& ctx) {
        Y_UNUSED(ctx);
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill(NActors::TEvents::TEvPoisonPill::TPtr& ev, const NActors::TActorContext& ctx) {
        Y_UNUSED(ev, ctx);
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        HFunc(NActors::TEvWriteValueRequest, HandleWriteValueRequest);
        HFunc(NActors::TEvents::TEvPoisonPill, HandlePoisonPill);
    })

    void PassAway() override {
        NActors::TActor<TWriteActor>::PassAway();
    }
};

THolder<NActors::IActor> NActors::CreateReadActor(std::istream& strm, const NActors::TActorId& writeActorId) {
    return MakeHolder<TReadActor>(strm, writeActorId);
}

THolder<NActors::IActor> NActors::CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> NActors::CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
}

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency)
            : Latency(latency) {}

    void Bootstrap(const NActors::TActorContext& ctx) {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr& ev, const NActors::TActorContext& ctx) {
        Y_UNUSED(ev);
        auto now = TInstant::Now();
        TDuration delta = now - LastTime;
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastTime = now;
        ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        HFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    })
};

THolder<NActors::IActor> NActors::CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> NActors::GetProgramShouldContinue() {
    return ShouldContinue;
}