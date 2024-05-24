#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <fstream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool Finish = false;
    const NActors::TActorId WriteActor;	
    int Count;
    bool FirstEnter;
public:
    TReadActor(const NActors::TActorId writeActor)
        : Finish(false), WriteActor(writeActor), Count(0), FirstEnter(true)
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
            FirstEnter = false;	
            Register(CreateTMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            Count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (FirstEnter) {
            	Register(CreateTMaximumPrimeDevisorActor(0, SelfId(), WriteActor).Release());
            	Count++;
            	Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
            Finish = true;
        }
    }
    
    void HandleDone() {
       Count--;
       if (Finish && Count == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    const NActors::TActorIdentity ReadActor;
    const NActors::TActorId WriteActor;
    int64_t Prime;	
    int64_t CurrentStateFirst;
    int64_t CurrentStateSecond;
    bool flag;
    	
public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor), CurrentStateFirst(1), Prime(0), flag(true), CurrentStateSecond(2)
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
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        for (int64_t i = CurrentStateFirst; i <= Value; i++) {
            for (int64_t j = CurrentStateSecond; j * j <= i; j++) {
                if (i % j == 0) {
                    flag = false;
		            break;
		        }
		        endTime = std::chrono::steady_clock::now();
            	elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            	if (elapsedTime > 10) {
            	    CurrentStateFirst = i;
            	    CurrentStateSecond = j;
            	    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            if (flag && Value % i == 0) {
                Prime = i;
            } else {
            	flag = true;
            }
        }
        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(Prime));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
    
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int Sum;
public:
    using TBase = NActors::TActor<TWriteActor>;
    
    TWriteActor() : TBase(&TWriteActor::Handler), Sum(0) {}
     	
    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });
    
    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        auto& event = *ev->Get();
        Sum = Sum + event.Value;
    }
   
    void HandleDone() {
       std::cout << Sum << std::endl;
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