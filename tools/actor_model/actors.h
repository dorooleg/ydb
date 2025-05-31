#pragma once

#include <istream>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/util/should_continue.h>
#include <util/generic/ptr.h>

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

THolder<NActors::IActor> CreateWriteActor();
THolder<NActors::IActor> CreateReadActor(NActors::TActorId writerActor);
THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency);
