#ifndef ACTORS_H
#define ACTORS_H

#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
#include "events.h"

THolder<NActors::IActor> CreateReadActor(std::istream& input, NActors::TActorId writeActor);
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActor, NActors::TActorId writeActor);
THolder<NActors::IActor> CreateWriteActor();

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

#endif
