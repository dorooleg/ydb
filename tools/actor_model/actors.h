#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>


THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<NActors::IActor> CreateSelfTReadActor(NActors::TActorId writeActor);
THolder <NActors::IActor> CreateSelfTMaximumPrimeDevisorActor(int value, NActors::TActorIdentity readActor, NActors::TActorId writeActor);
THolder<NActors::IActor> CreateSelfTWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

class checkIsPrimeNumberReturns{
public:
    bool status;
    bool result;
    int Obtained;

    checkIsPrimeNumberReturns(bool status, bool result, int obtainedValue) : status(status), result(result),
                                                                             Obtained(obtainedValue){};
};

checkIsPrimeNumberReturns* checkIsPrimeNumber(int n, auto startTime, int Obtained);
