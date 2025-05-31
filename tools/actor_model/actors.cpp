#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

namespace {

std::vector<bool> sieve(int n) {
    std::vector<bool> is_prime(n + 1, true);
    is_prime[0] = is_prime[1] = false;
    for (int i = 2; i * i <= n; ++i) {
        if (is_prime[i]) {
            for (int j = i * i; j <= n; j += i) {
                is_prime[j] = false;
            }
        }
    }
    return is_prime;
}

const std::vector<bool>& GetPrimesCache() {
    const static int max_prime = static_cast<int>(std::sqrt(INT_MAX)) + 1;
    static const std::vector<bool> primes = sieve(max_prime);
    return primes;
}

std::shared_ptr<TProgramShouldContinue> GetShouldContinueSingleton() {
    static auto should_continue = std::make_shared<TProgramShouldContinue>();
    return should_continue;
}

} // namespace

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

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
    const NActors::TActorId Writer;
    const NActors::TActorId Reader;
    const int Number;
    int CurrentPrime;
    const std::vector<bool>& Primes;

public:
    TMaximumPrimeDivisorActor(int number, NActors::TActorId reader, NActors::TActorId writer)
        : Writer(writer)
        , Reader(reader)
        , Number(number)
        , CurrentPrime(static_cast<int>(std::sqrt(INT_MAX)))
        , Primes(GetPrimesCache())
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Schedule(TDuration::Zero(), new TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void Finish(int result) {
        Send(Writer, new TEvents::TEvWriteValueRequest(result));
        Send(Reader, new TEvents::TEvDone());
        PassAway();
    }

    void HandleWakeup() {
        const TInstant start = TInstant::Now();
        for (; CurrentPrime >= 2; --CurrentPrime) {
            if (!Primes[CurrentPrime]) continue;
            
            if (Number % CurrentPrime == 0) {
                Finish(CurrentPrime);
                return;
            }
            
            if (TInstant::Now() - start >= TDuration::MilliSeconds(10)) {
                --CurrentPrime; // Сохраняем текущее простое число для следующей итерации
                Schedule(TDuration::Zero(), new TEvents::TEvWakeup());
                return;
            }
        }
        Finish(Number); // Число простое или 1
    }
};

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    const NActors::TActorId Writer;
    bool InputFinished = false;
    int ActiveTasks = 0;

public:
    TReadActor(NActors::TActorId writer)
        : Writer(writer)
    {}

    void Bootstrap() {
        Become(&TThis::StateFunc);
        Schedule(TDuration::Zero(), new TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(TEvents::TEvWakeup::EventType, ReadNext);
        cFunc(TEvents::TEvDone::EventType, OnTaskDone);
    });

    void ReadNext() {
        std::string input;
        if (std::cin >> input) {
            try {
                const int number = std::stoi(input);
                ++ActiveTasks;
                Register(new TMaximumPrimeDivisorActor(number, SelfId(), Writer));
            } catch (...) {
                // Пропускаем нечисловые значения
            }
            Schedule(TDuration::Zero(), new TEvents::TEvWakeup());
        } else {
            InputFinished = true;
            if (ActiveTasks == 0) {
                Send(Writer, new TEvents::TEvPoisonPill());
            }
        }
    }

    void OnTaskDone() {
        --ActiveTasks;
        if (InputFinished && ActiveTasks == 0) {
            Send(Writer, new TEvents::TEvPoisonPill());
        }
    }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
    int TotalSum = 0;

public:
    TWriteActor()
        : TActor(&TWriteActor::StateFunc)
    {}

    STRICT_STFUNC(StateFunc, {
        cFunc(TEvents::TEvPoisonPill::EventType, HandlePoison);
        hFunc(TEvents::TEvWriteValueRequest, HandleValue);
    });

    void HandleValue(TEvents::TEvWriteValueRequest::TPtr& ev) {
        TotalSum += ev->Get()->Value;
    }

    void HandlePoison() {
        std::cout << TotalSum << std::endl;
        GetShouldContinueSingleton()->ShouldStop(0);
        PassAway();
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return GetShouldContinueSingleton();
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writer) {
    return MakeHolder<TReadActor>(writer);
}
