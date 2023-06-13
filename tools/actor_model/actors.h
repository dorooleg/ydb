#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId writeActorId);
THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(const int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId);
THolder<NActors::IActor> CreateWriterActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
