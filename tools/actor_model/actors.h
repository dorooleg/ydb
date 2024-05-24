#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);
THolder<NActors::IActor> CreateReadActor(std::istream& strm, NActors::TActorId recipient);
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId);
THolder<NActors::IActor> CreateWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
