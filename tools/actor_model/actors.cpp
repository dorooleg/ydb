#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t OriginalNumber;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;

    int64_t D = 2; 
    int64_t N; 
    bool FinishCount = false;
    int64_t FMaxPrime = 1;

public:
    TMaximumPrimeDevisorActor(int64_t number, NActors::TActorId readActorId, NActors::TActorId writeActorId)
        : OriginalNumber(number)
        , ReadActorId(readActorId)
        , WriteActorId(writeActorId)
        , N(number)
    {
        if (OriginalNumber <= 0) 
        {
            FMaxPrime = 0;
            N = 0; 
        } else if (OriginalNumber == 1) 
        {
            FMaxPrime = 1;
            N = 1; 
        }
    }

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateWork);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateWork, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() 
    {
        if (FinishCount) 
        {
            return;
        }
        TInstant sPoint = TInstant::Now();
        if (N == 1) 
        { 
            End(FMaxPrime);
            return;
        }
        if (N <= 0) 
        { 
            End(0);
            return;
        }
        while (N > 1) 
        { 
            if (TInstant::Now() - TDuration::MilliSeconds(10) > sPoint) 
            {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return; 
            }
            if (D * D > N) 
            { 
                FMaxPrime = N; 
                N = 1;              
                break;              
            }
            if (N % D == 0) 
            {    
                FMaxPrime = D; 
                while (N % D == 0)
                    N /= D;
            }
            if (D == 2) D = 3;
            else D += 2;
        }
        End(FMaxPrime);
    }

    void End(int64_t result) 
    {
        if (FinishCount)
        {
             return; 
        }
        Send(WriteActorId, MakeHolder<TEvents::TEvWriteValueRequest>(result));
        Send(ReadActorId, MakeHolder<TEvents::TEvDone>());
        FinishCount = true;
        PassAway();
    }
};

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::istream& Strm;
    NActors::TActorId WriteActorRecipientId;
    int PendingDevisorActors = 0;
    bool ReadingDone = false;

public:
    TReadActor(std::istream& strm, NActors::TActorId writeActorRecipientId)
        : Strm(strm)
        , WriteActorRecipientId(writeActorRecipientId)
    {}

    void Bootstrap() 
    {
        Become(&TReadActor::StateRead);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateRead, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() 
    {
        int64_t v;
        if (Strm >> v) 
        {
            Register(new TMaximumPrimeDevisorActor(v, SelfId(), WriteActorRecipientId));
            PendingDevisorActors++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else 
        {
            ReadingDone = true;
            CheckCompletion();
        }
    }

    void HandleDone(TEvents::TEvDone::TPtr&) 
    {
        PendingDevisorActors--;
        CheckCompletion();
    }

    void CheckCompletion() 
    {
        if (ReadingDone && PendingDevisorActors == 0) 
        {
            Send(WriteActorRecipientId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : NActors::TActor<TWriteActor>(&TWriteActor::StateWork) {}

    STRICT_STFUNC(StateWork, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& ev) 
    {
        auto* msg = ev->Get();
        Sum += msg->Value;
    }

    void HandlePoisonPill() 
    {
        std::cout << Sum << std::endl;
        GetProgramShouldContinue()->ShouldStop(0); 
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(std::istream& s, NActors::TActorId id) {
    return MakeHolder<TReadActor>(s, id);
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
