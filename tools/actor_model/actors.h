#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writerId);
THolder<NActors::IActor> CreateTMaximumPrimeDivisorActor(const NActors::TActorId readerId, const NActors::TActorId writerId, int64_t val);
THolder<NActors::IActor> CreateTWriterActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
