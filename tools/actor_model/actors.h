#include <istream>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/util/should_continue.h>
#include <util/generic/ptr.h>
#include <util/system/types.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();

THolder<NActors::IActor> CreateReadActor(std::istream& in, NActors::TActorId writeActorId);

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(
    ui64 number,
    NActors::TActorId readActorId,
    NActors::TActorId writeActorId
);

THolder<NActors::IActor> CreateWriteActor();
