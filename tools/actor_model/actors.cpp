#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <typeinfo>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor: public NActors::TActorBootstrapped<TReadActor>{
    const NActors::TActorId writer;
    bool done;
    int active_devisors;
    bool empty;

public:
    TReadActor(const NActors::TActorId writer)
        : writer(writer), done(false), active_devisors(0), empty(true)
        {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDevisor);
    });

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            empty = false;
            Register(CreateTMaximumPrimeDevisorActor(SelfId(), writer, value).Release());
            active_devisors++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (empty){
                active_devisors++;
                Register(CreateTMaximumPrimeDevisorActor(SelfId(), writer, 0).Release());
            }
            done = true;
        }
    }

    void HandleDevisor(){
        active_devisors--;
        if(done && !active_devisors){
            Send(writer, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

class TMaximumPrimeDevisorActor: public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>{
    const NActors::TActorId reader;
    const NActors::TActorId writer;
    int64_t value;
    int64_t largest_devisor;
    int64_t copy;

public:
    TMaximumPrimeDevisorActor(const NActors::TActorId reader, const NActors::TActorId writer, int64_t value)
    : reader(reader), writer(writer), value(value), largest_devisor(2), copy(value)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto start_time = std::chrono::system_clock::now();
        if(value == 0 || value == 1 || value == -1){
            largest_devisor = value;
        }else{
            for (; largest_devisor <= copy; largest_devisor++) {
                if (copy % largest_devisor == 0) {
                    copy /= largest_devisor;
                    largest_devisor--;
                }
                auto current_time = std::chrono::system_clock::now();
                if(std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() > 10){
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
        }
        Send(writer, std::make_unique<TEvents::TEvWriteValueRequest>(largest_devisor));
        Send(reader, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

class TWriteActor: public NActors::TActorBootstrapped<TWriteActor>{
    int64_t devisor_sum;
public:
    TWriteActor()
    : devisor_sum(0)
    {}

    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleFinish);
        hFunc(TEvents::TEvWriteValueRequest, HandleDevisor)
    });

    void HandleFinish() {
        std::cout << devisor_sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    void HandleDevisor(TEvents::TEvWriteValueRequest::TPtr& devisor_message){
        devisor_sum += devisor_message->Get()->value;
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

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writer) {
    return MakeHolder<TReadActor>(writer);
}
THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(const NActors::TActorIdentity reader, const NActors::TActorId writer, int64_t value) {
    return MakeHolder<TMaximumPrimeDevisorActor>(reader, writer, value);
}
THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}
