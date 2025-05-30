#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TMaximumPrimeDivisorActor
    : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
  int64_t Number;
  NActors::TActorId ReadActorId;
  NActors::TActorId WriteActorId;
  int64_t LastGuess = 1;
  int64_t MaxLowGuess = 1;

  TInstant InvokedAt;

  bool is_prime(int64_t number) {
    if (number == 1) {
      return false;
    } else if (number == 2) {
      return true;
    }
    for (int d = 2; d * d <= number; d++) {
      if (number % d == 0) {
        return false;
      }
    }
    return true;
  }

  bool calculate(int64_t &result) {
    for (int64_t guess = LastGuess; guess * guess <= Number; guess++) {
      int64_t guess_upper = Number / guess;
      if (Number % guess == 0) {
        if (is_prime(guess_upper)) {
          result = guess_upper;
          return true;
          return guess;
        } else if (is_prime(guess)) {
          MaxLowGuess = guess;
        }
      }
      LastGuess = guess;
      if (TInstant::Now() - InvokedAt > TDuration::MilliSeconds(10)) {
        return false;
      }
    }
    result = MaxLowGuess;
    return true;
  }

public:
  TMaximumPrimeDivisorActor(int64_t number, NActors::TActorId read_actor_id,
                            NActors::TActorId write_actor_id)
      : Number(number), ReadActorId(read_actor_id),
        WriteActorId(write_actor_id) {}

  void Bootstrap() {
    Become(&TMaximumPrimeDivisorActor::StateFunc);
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
  });

  void HandleWakeup() {
    InvokedAt = TInstant::Now();
    int64_t result = -1;
    if (calculate(result)) {
      Y_VERIFY(result > 0, "weird result");
      Send(WriteActorId,
           std::make_unique<TEvents::TEvWriteValueRequest>(result));
      Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
      PassAway();
    } else {
      Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
  }
};

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
  NActors::TActorId WriteActorId;
  int64_t WaitingOn = 1;  // Dummy wait for EOF

public:
  TReadActor(NActors::TActorId write_actor_id) : WriteActorId(write_actor_id) {}

  void Bootstrap() {
    Become(&TReadActor::StateFunc);
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    cFunc(TEvents::TEvDone::EventType, HandleDone);
  });

  void HandleWakeup() {
    int64_t value;
    if (std::cin >> value) {
      WaitingOn++;
      Register(new TMaximumPrimeDivisorActor(value, SelfId(), WriteActorId));
      Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    } else {
      // Release the dummy EOF wait
      Send(SelfId(), std::make_unique<TEvents::TEvDone>());
    }
  }

  void HandleDone() {
    WaitingOn--;
    Y_VERIFY(WaitingOn >= 0, "weird WaitingOn");
    if (WaitingOn == 0) {
      Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
    }
  }
};

THolder<NActors::IActor> CreateReadActor(NActors::TActorId write_actor_id) {
  return MakeHolder<TReadActor>(write_actor_id);
}

class TWriteActor : public NActors::TActorBootstrapped<TWriteActor> {
  int64_t Sum = 0;

public:
  void Bootstrap() { Become(&TWriteActor::StateFunc); }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
  });

  void HandlePoisonPill() {
    std::cout << Sum << std::endl;
    ShouldContinue->ShouldStop();
    PassAway();
  }

  void HandleWriteValueRequest(const TEvents::TEvWriteValueRequest::TPtr &ev) {
    Sum += ev->Get()->Value;
  }
};

THolder<NActors::IActor> CreateWriteActor() {
  return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
  return ShouldContinue;
}
