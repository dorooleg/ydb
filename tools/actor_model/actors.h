#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

#include "events.h"

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writerActorId);
THolder<NActors::IActor> CreateWriteActor();
THolder<NActors::IActor> CreateMaxPrimeDivisorActor(int64_t value, NActors::TActorId readerActorId, NActors::TActorId writerActorId);

THolder<NActors::IActor> CreateSystemMonitorActor(const TDuration& latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
