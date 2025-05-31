#pragma once

#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/util/should_continue.h>
#include <util/generic/ptr.h>

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId& writerId);
THolder<NActors::IActor> CreateWriteActor();
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();