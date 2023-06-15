#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <stdint.h>
#include <cmath>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    NActors::TActorId WriteId;
    int64_t check = 0;
public:
    TReadActor(const NActors::TActorId writeId)
        : WriteId(writeId)
    {}
   

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleEventDone);
    });
    
    void HandleWakeup() {
        int64_t n;
        if (std::cin >> n){
            check++;
            Register(CreateTMaximumPrimeDevisorActor(n, WriteId, SelfId()).Release());
            Send(SelfId(),  std::make_unique<NActors::TEvents::TEvWakeup>());
        }
    }

    void HandleEventDone(){
        check--;
        if(check == 0){Send(WriteId, std::make_unique<NActors::TEvents::TEvPoisonPill>());}
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId WriteId;
    NActors::TActorId ReadId;
    TInstant startTime;
    int64_t curr;
    int64_t ans;
public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& writeId,const NActors::TActorId& readId)
        : Value(value), WriteId(writeId), ReadId(readId), curr(1), ans(0) 
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });
    

    void HandleWakeup() {
        if (ans == 0) {
            int64_t maxPrime = 1;
            auto startTime = std::chrono::steady_clock::now();
            for (int64_t i = curr; i <= Value; ++i) {
                if (Value % i == 0) {
                    bool isPrime = true;
                    for (int64_t j = 2; j <= sqrt(i); ++j) {
                        auto endTime = std::chrono::steady_clock::now();
                        if(std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime).count() > 10){
                            curr = i;
                            return;
                        }
                        if (i % j == 0) {
                            isPrime = false;
                            break;
                        }
                    }
                    if (isPrime) { maxPrime = i; }
                }
            }
            ans = maxPrime;
            
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            return;
        }
        else {
            Send(WriteId, std::make_unique<TEvents::TEvWriteValueRequest>(ans));
            Send(ReadId, std::make_unique<TEvents::TEvDone>());
            PassAway();
        }
    }
    
    
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t sum = 0;
    
    void Handle(TEvents::TEvWriteValueRequest::TPtr &ev){
        auto *msg = ev->Get();
        sum = sum + msg->value;  
    }

public:
    TWriteActor()
    : TActor(&TThis::StateFunc)
    {}

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePill);
        hFunc(TEvents::TEvWriteValueRequest, Handle)
    });

    void HandlePill(){
        std::cout << sum << '\n';
        ShouldContinue->ShouldStop();
        PassAway();
    }

};

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeId) {
    return MakeHolder<TReadActor>(writeId);
}

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId writeId, const NActors::TActorId readId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, writeId, readId);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}


std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
