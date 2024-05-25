#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <cmath>
#include <iostream>

#define CALCULATION_LIMIT 10

// Shared variable to signal when the program should stop
static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    std::shared_ptr <TProgramShouldContinue> ShouldContinue;
    std::istream &Stream;
    NActors::TActorId WriteActor;
    int InProgress;

public:
    TReadActor(std::shared_ptr <TProgramShouldContinue> shouldContinue, std::istream &stream)
            : ShouldContinue(shouldContinue), Stream(stream), InProgress(0) {}

    // Initialize the actor and start its lifecycle
    void Bootstrap() {
        WriteActor = Register(CreateWriteActor(ShouldContinue).Release());
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
        Become(&TReadActor::StateFunc);
    }

    // Define the state function for handling events
    STRICT_STFUNC(StateFunc,
            hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    cFunc(TEvents::TEvDone::EventType, HandleDone
    );
    )

    // Handle completion of prime calculation
    void HandleDone() {
        if (--InProgress == 0 && Stream.eof()) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill);
            ShouldContinue->ShouldStop();
            PassAway();
        }
    }

    // Handle the wakeup event to read and process input
    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        int64_t value;
        if (Stream >> value) {
            InProgress++;
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else if (InProgress == 0) {
            Send(WriteActor, new NActors::TEvents::TEvPoisonPill);
            ShouldContinue->ShouldStop();
            PassAway();
        }
    }
};

THolder <NActors::IActor> CreateReadActor(std::shared_ptr <TProgramShouldContinue> shouldContinue) {
    return MakeHolder<TReadActor>(shouldContinue, std::cin);
}

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;
    int64_t LargestPrime;
    TInstant StartTime;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId &readActor, const NActors::TActorId &writeActor)
            : Value(value), ReadActor(readActor), WriteActor(writeActor), LargestPrime(1) {}

    // Initialize the actor and start its lifecycle
    void Bootstrap() {
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
        Become(&TMaximumPrimeDevisorActor::StateFunc);
    }

    // Define the state function for handling events
    STRICT_STFUNC(StateFunc,
            hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    )

    // Handle the wakeup event to start or continue the calculation
    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        StartTime = TInstant::Now();
        Calculate();
        if (TInstant::Now() - StartTime > TDuration::MilliSeconds(CALCULATION_LIMIT)) {
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            Send(WriteActor, new TEvents::TEvWriteValueRequest(LargestPrime));
            Send(ReadActor, new TEvents::TEvDone);
            PassAway();
        }
    }

private:
    // Calculate the largest prime divisor
    void Calculate() {
        if (Value <= 1) return;

        divideByPrime(2);

        for (int i = 3; i <= std::sqrt(Value); i += 2) {
            divideByPrime(i);
        }

        if (Value > 2) {
            LargestPrime = Value;
        }
    }

    // Helper function to divide Value by a given prime number
    void divideByPrime(int prime) {
        while (Value % prime == 0) {
            LargestPrime = prime;
            Value /= prime;
        }
    }
};

THolder <NActors::IActor>
CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId &readActor, const NActors::TActorId &writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
    std::shared_ptr <TProgramShouldContinue> ShouldContinue;
    int64_t Sum;

public:
    TWriteActor(std::shared_ptr <TProgramShouldContinue> shouldContinue)
            : ShouldContinue(shouldContinue), Sum(0) {}

    // Initialize the actor and start its lifecycle
    void Bootstrap() {
        Become(&TWriteActor::StateFunc);
    }

    // Define the state function for handling events
    STRICT_STFUNC(StateFunc,
            hFunc(TEvents::TEvWriteValueRequest, HandleWriteValue);
    cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleTerminate
    );
    )

    // Handle write value request
    void HandleWriteValue(TEvents::TEvWriteValueRequest::TPtr ev) {
        Sum += ev->Get()->Value;
    }

    // Handle event to terminate the actor
    void HandleTerminate() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

THolder <NActors::IActor> CreateWriteActor(std::shared_ptr <TProgramShouldContinue> shouldContinue) {
    return MakeHolder<TWriteActor>(shouldContinue);
}
