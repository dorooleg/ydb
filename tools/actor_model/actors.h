#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
#include <util/stream/input.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

THolder<NActors::IActor> CreateReadActor(IInputStream& strm, const NActors::TActorId& writeActor);
THolder<NActors::IActor> CreateMaxPrimeDevActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor);
THolder<NActors::IActor> CreateWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();