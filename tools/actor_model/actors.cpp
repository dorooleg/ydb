#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

std::vector<bool> sieve(int n) {
    std::vector<bool> is_prime(n + 1, true);
    for (int i = 2; i <= n; i++)
        if (is_prime[i])
            for (int j = 2 * i; j <= n; j += i)
                is_prime[j] = false;
    return is_prime;            
}

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();
std::vector<bool> primes = sieve(sqrt(INT_MAX));

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

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    NActors::TActorId Writer;
    NActors::TActorId Reader;
    int OnCalculate = 0;
    int CurPrime = sqrt(INT_MAX);
public:
    TMaximumPrimeDevisorActor(int on_calculate, NActors::TActorId reader, NActors::TActorId writer) 
        : Writer(writer)
        , Reader(reader) {
            OnCalculate = on_calculate;
        }

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Schedule(TDuration::Zero(), new TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void Finish(int n) {
        Send(Writer, new TEvents::TEvWriteValueRequest(n));
        Send(Reader, new TEvents::TEvDone());
        PassAway();
    }

    void HandleWakeup() {
        TInstant StartTime = TInstant::Now();
        for (int prime = CurPrime; prime >= 2; prime--) {
            if (primes[prime] == 0) continue;
            if (OnCalculate % prime == 0) {
                Finish(prime);
                return;
            }
            auto elapsed = TInstant::Now() - StartTime;
            if (elapsed.MilliSeconds() >= 10) {
                CurPrime = prime;
                Send(SelfId(), new TEvents::TEvWakeup());
                return;
            }
        }
        Finish(OnCalculate);
    }
};

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    NActors::TActorId Writer;
    bool StrmFinished = false;
    int Num = 0;
public:
    TReadActor(NActors::TActorId writer) 
        : Writer(writer) {}

    void Bootstrap() {
        Become(&TThis::StateFunc);
        Schedule(TDuration::Zero(), new TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        std::string str;
        if (std::cin >> str) {
            int num = std::stoi(str);
            Num += 1;
            Register(new TMaximumPrimeDevisorActor(num, SelfId(), Writer));
            Send(SelfId(), new TEvents::TEvWakeup());
        } else {
            // std::cout << "Strm finished" << std::endl;
            StrmFinished = true;
            if (Num <= 0) {
                Send(SelfId(), new TEvents::TEvDone());
            }
        }
    }

    void HandleDone() {
        Num -= 1;
        if (Num <= 0 && StrmFinished) {
            Send(Writer, new TEvents::TEvPoisonPill());
        }
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int sum = 0;
public:
    TWriteActor()
        : TActor(&TWriteActor::StateFunc)
    {
        Become(&TThis::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(TEvents::TEvPoisonPill::EventType, HandlePoison);
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteRequest);
    });

    void HandleWriteRequest(TEvents::TEvWriteValueRequest::TPtr& ev) {
        this->sum += ev->Get()->Value;
    }

    void HandlePoison() {
        std::cout << sum << std::endl;
        ShouldContinue->ShouldStop(0);
        PassAway();
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writer) {
    return MakeHolder<TReadActor>(writer);
}
