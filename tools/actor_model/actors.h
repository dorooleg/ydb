#include "library/cpp/actors/interconnect/types.h"
#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/util/should_continue.h>
#include <util/generic/ptr.h>

THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency);
THolder<NActors::IActor> CreateMaximumDivisorActor(i64 targetValue,
                                                   TActorId readActorId,
                                                   TActorId writeActorId);
THolder<NActors::IActor> CreateReadActor(std::istream &in,
                                         TActorId writeActorId);
THolder<NActors::IActor> CreateWriteActor(std::ostream &out);
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
