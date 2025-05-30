#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
#include <chrono>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<NActors::IActor> CreateSelfTReadActor(NActors::TActorId writeActor);
THolder<NActors::IActor> CreateSelfTMaximumPrimeDivisorActor(int value, NActors::TActorIdentity readActor, NActors::TActorId writeActor);
THolder<NActors::IActor> CreateSelfTWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

class checkIsPrimeNumberReturns {
public:
    bool status;
    bool result;
    int received;

    checkIsPrimeNumberReturns(bool status, bool result, int receivedValue)
        : status(status), result(result), received(receivedValue) {}
};

checkIsPrimeNumberReturns* checkIsPrimeNumber(int n, std::chrono::steady_clock::time_point startTime, int received);