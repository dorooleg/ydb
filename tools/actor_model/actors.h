#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>

class TReadActor;
class TMaximumPrimeDevisorActor;
class TWriteActor;

THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency);

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActorId, std::istream &inputStream);
THolder<NActors::IActor> CreateWriteActor();

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();