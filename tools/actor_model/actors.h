#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateReadActor(std::istream& stream, NActors::TActorId recipient);
THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(int64_t num, NActors::TActorId readActorID, NActors::TActorId writeActorID);
THolder<NActors::IActor> CreateWriteActor();
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
