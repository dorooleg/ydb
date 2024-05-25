#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <sstream>
#include <iostream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

int FindLargestPrimeDivisor(int number) {
    if (number == 1) {
        return 1;
    }
    int largest = 1;
    for (int div = 2; div * div <= number; ++div) {
        while (number % div == 0) {
            largest = div;
            number /= div;
        }
    }
    return number > 1 ? number : largest;
}

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    NActors::TActorSystem* ActorSystem;
    NActors::TActorId MaxPrimeActor;
public:
    TReadActor(NActors::TActorSystem* actorSystem, NActors::TActorId maxPrimeActor)
        : ActorSystem(actorSystem), MaxPrimeActor(maxPrimeActor) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        std::string input;
        std::getline(std::cin, input);
        std::istringstream iss(input);
        int number;
        while (iss >> number) {
            ActorSystem->Send(MaxPrimeActor, new TEvents::TEvProcessNumber(number));
        }
        ActorSystem->Send(MaxPrimeActor, new NActors::TEvents::TEvPoisonPill);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvProcessNumber, HandleNumber);
    });

    void HandleNumber(const TEvents::TEvProcessNumber::TPtr& ev) {
    }
};

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
    NActors::TActorSystem* ActorSystem;
    NActors::TActorId WriteActor;
public:
    TMaximumPrimeDivisorActor(NActors::TActorSystem* actorSystem, NActors::TActorId writeActor)
        : ActorSystem(actorSystem), WriteActor(writeActor) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvProcessNumber, HandleNumber);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoison);
    });

    void HandleNumber(const TEvents::TEvProcessNumber::TPtr& ev) {
        int largestPrime = FindLargestPrimeDivisor(ev->Get()->Number);
        ActorSystem->Send(WriteActor, new TEvents::TEvPrimeDivisorFound(largestPrime));
    }

    void HandlePoison() {
        ActorSystem->Send(WriteActor, new NActors::TEvents::TEvPoisonPill);
        PassAway();
    }
};

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
    int Sum = 0;
public:
    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvPrimeDivisorFound, HandlePrime);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoison);
    });

    void HandlePrime(const TEvents::TEvPrimeDivisorFound::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoison() {
        std::cout << Sum << std::endl;
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(NActors::TActorSystem* actorSystem, NActors::TActorId maxPrimeActor) {
    return MakeHolder<TReadActor>(actorSystem, maxPrimeActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(NActors::TActorSystem* actorSystem, NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDivisorActor>(actorSystem, writeActor);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}