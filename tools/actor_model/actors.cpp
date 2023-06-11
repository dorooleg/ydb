#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <fstream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool isFinish = false;
    const NActors::TActorId WriteActor;	
    int c;
    bool EnterFirst;
public:
    TReadActor(const NActors::TActorId writeActor)
        : isFinish(false), WriteActor(writeActor), c(0), EnterFirst(true)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeUp() {
        int64_t value;
        if (std::cin >> value) {
            EnterFirst = false;	
            Register(CreateTMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            c++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (EnterFirst) {
            	Register(CreateTMaximumPrimeDevisorActor(0, SelfId(), WriteActor).Release());
            	c++;
            	Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
            isFinish = true;
        }
    }
    
    void HandleDone() {
       c--;
       if (isFinish && c == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    const NActors::TActorIdentity ReadActor;
    const NActors::TActorId WriteActor;
    int64_t p;	
    int64_t FirstState;
    int64_t SecondState;
    bool mark;
    	
public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor), FirstState(1), p(0), mark(true), SecondState(2)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
    	auto startTime = std::chrono::steady_clock::now();
    	auto endTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).c();
        for (int64_t i = FirstState; i <= Value; i++) {
            for (int64_t j = SecondState; j * j <= i; j++) {
                if (i % j == 0) {
                    mark = false;
		            break;
		        }
		        endTime = std::chrono::steady_clock::now();
            	elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).c();
            	if (elapsedTime > 10) {
            	    FirstState = i;
            	    SecondState = j;
            	    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            if (mark && Value % i == 0) {
                p = i;
            } else {
            	mark = true;
            }
        }
        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(Prime));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
    
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int s;
public:
    using TBase = NActors::TActor<TWriteActor>;
    
    TWriteActor() : TBase(&TWriteActor::Handler), s(0) {}
     	
    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });
    
    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        auto& event = *ev->Get();
        s = s + event.Value;
    }
   
    void HandleDone() {
       std::cout << s << std::endl;
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
        Y_VERIFY(delta <= Latency, "Latency is big");
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
