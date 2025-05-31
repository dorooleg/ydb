#pragma once
#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

namespace NActors {
    THolder<IActor> CreateReadActor(std::istream& strm, const TActorId& writeActorId);
    THolder<IActor> CreateWriteActor();
    THolder<IActor> CreateMaximumPrimeDevisorActor(int64_t value, const TActorId& readActorId, const TActorId& writeActorId);
    THolder<IActor> CreateSelfPingActor(const TDuration& latency);

    std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
}