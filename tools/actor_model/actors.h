#include "events.h"
#include <iostream>
#include <library/cpp/actors/core/actor.h>
#include <util/generic/ptr.h>
#include <library/cpp/actors/util/should_continue.h>
#include <memory>
#include <util/system/types.h>

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId& writerId);

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t number, const NActors::TActorIdentity& readerId, const NActors::TActorId& writerId);

THolder<NActors::IActor> CreateTWriteActor();

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& interval);

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue();
