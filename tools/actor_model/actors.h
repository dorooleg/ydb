#pragma once

#include <istream>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/util/should_continue.h>
#include <util/generic/ptr.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency);
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

THolder<NActors::IActor> CreateWriteActor();
THolder<NActors::IActor> CreateReadActor(std::istream &strm,
                                         NActors::TActorId writeActor);
