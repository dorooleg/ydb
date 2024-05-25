#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();


class TReadActor: public NActors::TActorBootstrapped<TReadActor>{
private:
    const NActors::TActorId writer;
    bool is_done;
    size_t devisors;

public:
    TReadActor(const NActors::TActorId writer)
            : writer(writer), is_done(false){
        devisors = 0;
    }


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
            is_done=false;
            Register(CreateTMaximumPrimeDevisorActor(SelfId(), writer, value).Release());
            devisors=devisors+1;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            
            if (devisors == 0) {
                Send(writer, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
            is_done = true;
        }
    }

    void HandlerDevisor(){
        devisors=devisors-1;
        if(is_done && devisors==0){
            Send(writer, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};



class TMaximumPrimeDevisorActor: public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>{
private:
    const NActors::TActorId reader;
    const NActors::TActorId writer;
    int64_t value;
    int64_t copy;
    int64_t curr = 2;
    int64_t largest_divisor = 0;

public:
    TMaximumPrimeDevisorActor(const NActors::TActorId reader, const NActors::TActorId writer, int64_t value)
            : reader(reader), writer(writer), value(value), copy(value)
    {}

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeup() {
        TInstant start = TInstant::Now();
        for (int64_t i = curr; i <= sqrt(value); i+=(i==2?1:2)) {
            curr = i;

            while (value % curr == 0) {
                largest_divisor = curr;
                value /= curr;
                TInstant time =  TInstant::Now();
                if (time - start >= TDuration::MilliSeconds(10)){
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            TInstant time = TInstant::Now();
            if (time - start >= TDuration::MilliSeconds(10)){
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }
        if (value >= 2) {
            largest_divisor = value;
        }
        if (copy == 1) {
            largest_divisor = 1;
        }
        Send(writer, std::make_unique<TEvents::TEvWriteValueRequest>(largest_divisor));
        Send(reader, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};


class TWriteActor: public NActors::TActorBootstrapped<TWriteActor>{
private:
    int64_t sum=0;
public:
    TWriteActor(){}

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

    void HandleDevisor(TEvents::TEvWriteValueRequest::TPtr& message){
        sum += message->Get()->value;
    }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency)
            : Latency(latency){}

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