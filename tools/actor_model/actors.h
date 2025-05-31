#pragma once

#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
#include <util/stream/input.h>
#include <util/stream/output.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

THolder<NActors::IActor> CreateReadActor(IInputStream& strm, const NActors::TActorId& writeActorId);
THolder<NActors::IActor> CreateWriteActor(IOutputStream& outStream);
