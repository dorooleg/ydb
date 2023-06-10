#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <fstream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor>{
    bool finish, no_value;
    const NActors::TActorId WriteActor;	
    int32_t count;
public:
     TReadActor(const NActors::TActorId writeActor)
        : finish(false), no_value(true), WriteActor(writeActor), count(0)
    {}

    void Bootstrap(){
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
        

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            no_value = false;
            Register(CreateTMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }  
        else {
            if (no_value){
                Register(CreateTMaximumPrimeDevisorActor(0, SelfId(), WriteActor).Release());
                count++;
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
            finish = true;
        }
    }

    void HandleDone() {
        count--;
        if (finish && count == 0)
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
    } 
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    const NActors::TActorIdentity ReadActor;
    const NActors::TActorId WriteActor;
    int64_t val, prime, current_state_first, current_state_second;
    bool flag;
public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor)
        : ReadActor(readActor), WriteActor(writeActor), val(value), prime(0), current_state_first(1), current_state_second(2), flag(true)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        TInstant start_t = TInstant::Now();
        TInstant end_t = TInstant::Now();
        TDuration delta = start_t - end_t;
        for (int64_t i = current_state_first; i <= val; i++) {
            for (int64_t j = current_state_second; j * j <= i; j++) {
                if (i % j == 0) {
                    flag = false;
		            break;
		        }
		        end_t = TInstant::Now();
            	delta = end_t - start_t;
            	if (delta.MicroSeconds() > 10) {
            	    current_state_first = i;
            	    current_state_second = j;
            	    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            if (flag && val % i == 0) 
                prime = i;
            else 
            	flag = true;
            
        }
        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(prime));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
    
};


class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t sum;
public:
    
    TWriteActor() : NActors::TActor<TWriteActor>(&TWriteActor::Handler), sum(0) {}
     	
    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });
    
    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        auto& event = *ev->Get();
        sum += event.value;
    }
   
    void HandleDone() {
       std::cout << sum << "\n";
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

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
