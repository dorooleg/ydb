#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool done;
    const NActors::TActorId writerId;
    int count = 0;

public:
    TReadActor(const NActors::TActorId writerId)
        : done(false), writerId(writerId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t val;
        if (std::cin >> val) {
            Register(CreateTMaximumPrimeDevisorActor(SelfId(), writerId, val).Release());
            count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            done = true;
            if (count == 0) {
                Send(writerId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone() {
        count--;
        if (done && count == 0) {
            Send(writerId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writerId) {
    return MakeHolder<TReadActor>(writerId);
}

cclass TMaximumPrimeDevisorActor: public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>{
    const NActors::TActorId readerId;
    const NActors::TActorId writerId;
    int64_t val;
    int64_t divisor;
    int64_t copy;

public:
    TMaximumPrimeDevisorActor(const NActors::TActorId readerId, const NActors::TActorId writerId, int64_t val)
    : readerId(readerId), writerId(writerId), val(val), divisor(2), copy(val)
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
        if(val == 0 || val == 1 || val == -1){
            divisor = val;
        }else{
            for (; divisor <= copy; divisor++) {
                if (copy % divisor == 0) {
                    copy /= divisor;
                    divisor--;
                }
                auto current_time = std::chrono::system_clock::now();
                if(std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() > 10){
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
        }
        Send(writerId, std::make_unique<TEvents::TEvWriteValueRequest>(divisor));
        Send(readerId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(const NActors::TActorIdentity readerId, const NActors::TActorId writerId, int64_t val) {
    return MakeHolder<TMaximumPrimeDevisorActor>(readerId, writerId, val);
}

class TWriterActor : public NActors::TActorBootstrapped<TWriterActor> {
    int64_t Sum = 0;

public:
    TWriterActor()
    {}

    void Bootstrap() {
        Become(&TWriterActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& event) {
        int64_t val = (*event->Get()).Value;
        Sum += val;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

THolder<NActors::IActor> CreateTWriteActor() {
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
