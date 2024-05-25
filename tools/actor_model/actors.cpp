#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <cmath>
#include <chrono>

// Функция для проверки, является ли число простым
bool is_prime(int64_t n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int64_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

// Функция для нахождения наибольшего простого делителя числа
int64_t largest_prime_divisor(int64_t n) {
    int64_t maxPrime = -1;

    while (n % 2 == 0) {
        maxPrime = 2;
        n >>= 1;
    }

    for (int64_t i = 3; i * i <= n; i += 2) {
        while (n % i == 0) {
            maxPrime = i;
            n /= i;
        }
    }

    if (n > 2)
        maxPrime = n;

    return maxPrime;
}

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
public:
    TReadActor(NActors::TActorId writeActorId)
        : WriteActorId(writeActorId), Unprocessed(0)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        int64_t number;
        if (std::cin >> number) {
            Register(New<TMaximumPrimeDevisorActor>(number, SelfId(), WriteActorId).Release());
            Unprocessed++;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else if (Unprocessed == 0) {
            Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }

    void HandleDone(TEvents::TEvDone::TPtr) {
        Unprocessed--;
        if (Unprocessed == 0 && std::cin.eof()) {
            Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
            PassAway();
        }
    }

private:
    const NActors::TActorId WriteActorId;
    int Unprocessed;
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
public:
    TMaximumPrimeDevisorActor(int64_t number, NActors::TActorId readActorId, NActors::TActorId writeActorId)
        : Number(number), ReadActorId(readActorId), WriteActorId(writeActorId)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto startTime = std::chrono::high_resolution_clock::now();

        int64_t maxPrimeDivisor = largest_prime_divisor(Number);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (duration.count() > 10) {
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            Send(WriteActorId, new TEvents::TEvWriteValueRequest(maxPrimeDivisor));
            Send(ReadActorId, new TEvents::TEvDone());
            PassAway();
        }
    }

private:
    int64_t Number;
    const NActors::TActorId ReadActorId;
    const NActors::TActorId WriteActorId;
};

class TWriteActor : public NActors::TActor<TWriteActor> {
public:
    TWriteActor()
        : Sum(0)
    {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << "Sum of largest prime divisors: " << Sum << std::endl;
        ShouldStop();
        PassAway();
    }

private:
    int64_t Sum;
};

THolder<NActors::IActor> CreateReadActor(NActors::TActorId maxPrimeDevisorActor) {
    return MakeHolder<TReadActor>(maxPrimeDevisorActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t number, NActors::TActorId readActorId, NActors::TActorId writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(number, readActorId, writeActorId);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
