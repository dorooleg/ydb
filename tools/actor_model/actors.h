#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
// Function to create an actor for reading
THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor);
// Function to create an actor to find the maximum prime divisor
THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(const NActors::TActorIdentity readActor, const NActors::TActorId writeActor, int64_t value);
// Function to create an actor for writing
THolder<NActors::IActor> CreateTWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

