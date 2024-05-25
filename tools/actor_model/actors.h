#include "events.h"
#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
#include <memory>
#include <util/system/types.h>

// Creates an actor for reading data
THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId& writerId);

// Creates an actor for calculating the maximum prime divisor
THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t number, const NActors::TActorIdentity& readerId, const NActors::TActorId& writerId);

// Creates an actor for writing data
THolder<NActors::IActor> CreateTWriteActor();

// Creates an actor that regularly sends messages to itself to maintain activity
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& interval);

// Returns a shared_ptr to an object controlling the continuation of the program
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
