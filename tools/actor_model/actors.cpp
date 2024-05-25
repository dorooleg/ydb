#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <sstream>
#include <iostream>

static auto ContinueStatus = std::make_shared<TProgramShouldContinue>();

int CalculateLargestPrimeFactor(int number) {
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

class TInputHandler : public NActors::TActorBootstrapped<TInputHandler> {
    NActors::TActorSystem* ActorSystem;
    NActors::TActorId PrimeActor;
public:
    TInputHandler(NActors::TActorSystem* actorSystem, NActors::TActorId primeActor)
        : ActorSystem(actorSystem), PrimeActor(primeActor) {}

    void Bootstrap() {
        Become(&TInputHandler::StateFunc);
        std::string input;
        std::getline(std::cin, input);
        std::istringstream iss(input);
        int number;
        while (iss >> number) {
            ActorSystem->Send(PrimeActor, new CustomEvents::TEvHandleInput(number));
        }
        ActorSystem->Send(PrimeActor, new NActors::TEvents::TEvPoisonPill);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(CustomEvents::TEvHandleInput, HandleInput);
    });

    void HandleInput(const CustomEvents::TEvHandleInput::TPtr& ev) {}
};

class TPrimeHandler : public NActors::TActorBootstrapped<TPrimeHandler> {
    NActors::TActorSystem* ActorSystem;
    NActors::TActorId SumActor;
public:
    TPrimeHandler(NActors::TActorSystem* actorSystem, NActors::TActorId sumActor)
        : ActorSystem(actorSystem), SumActor(sumActor) {}

    void Bootstrap() {
        Become(&TPrimeHandler::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(CustomEvents::TEvHandleInput, HandleInput);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoison);
    });

    void HandleInput(const CustomEvents::TEvHandleInput::TPtr& ev) {
        int largestPrime = CalculateLargestPrimeFactor(ev->Get()->Input);
        ActorSystem->Send(SumActor, new CustomEvents::TEvFactorFound(largestPrime));
    }

    void HandlePoison() {
        ActorSystem->Send(SumActor, new NActors::TEvents::TEvPoisonPill);
        PassAway();
    }
};

class TSumHandler : public NActors::TActorBootstrapped<TSumHandler> {
    int TotalSum = 0;
public:
    void Bootstrap() {
        Become(&TSumHandler::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(CustomEvents::TEvFactorFound, HandleFactor);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoison);
    });

    void HandleFactor(const CustomEvents::TEvFactorFound::TPtr& ev) {
        TotalSum += ev->Get()->Value;
    }

    void HandlePoison() {
        std::cout << TotalSum << std::endl;
        PassAway();
    }
};

THolder<NActors::IActor> CreateInputHandler(NActors::TActorSystem* actorSystem, NActors::TActorId primeActor) {
    return MakeHolder<TInputHandler>(actorSystem, primeActor);
}

THolder<NActors::IActor> CreatePrimeHandler(NActors::TActorSystem* actorSystem, NActors::TActorId sumActor) {
    return MakeHolder<TPrimeHandler>(actorSystem, sumActor);
}

THolder<NActors::IActor> CreateSumHandler() {
    return MakeHolder<TSumHandler>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramStatus() {
    return ContinueStatus;
}