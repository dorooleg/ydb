#pragma once

#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>


THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId outputActor);

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t number, const NActors::TActorIdentity inputActor, const NActors::TActorId outputActor);

THolder<NActors::IActor> CreateTWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();