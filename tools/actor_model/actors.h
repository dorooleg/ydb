#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

THolder<NActors::IActor> CreateTReadActor(NActors::TActorId writer_id);

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(NActors::TActorId read_id, NActors::TActorId writer_id, int64_t value);

THolder<NActors::IActor> CreateTWriteActor();
