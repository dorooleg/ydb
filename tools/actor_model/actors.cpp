#include "actors.h"
#include "events.h"

#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/events.h>
#include <library/cpp/actors/core/hfunc.h>

#include <chrono>
#include <iostream>

using namespace NActors;
using namespace MyActorModel;

static std::shared_ptr<TProgramShouldContinue> ShouldContinue =
    std::make_shared<TProgramShouldContinue>();

class TWriteActor : public TActor<TWriteActor> {
  int64_t Sum = 0;

public:
  TWriteActor() : TActor(&TWriteActor::StateFunc) {}

  STRICT_STFUNC(StateFunc,
                hFunc(MyActorModel::TEvents::TEvWriteValueRequest, HandleWrite);
                cFunc(NActors::TEvents::TEvPoisonPill::EventType,
                      HandlePoison);)

  void HandleWrite(MyActorModel::TEvents::TEvWriteValueRequest::TPtr &ev) {
    Sum += ev->Get()->Value;
  }

  void HandlePoison() {
    std::cout << Sum << std::endl;
    ShouldContinue->ShouldStop();
    PassAway();
  }
};

THolder<IActor> CreateWriteActor() { return MakeHolder<TWriteActor>(); }

class TMaximumPrimeDevisorActor
    : public TActorBootstrapped<TMaximumPrimeDevisorActor> {
  int64_t Number;
  int64_t MaxPrime = 1;
  int64_t CurrentDiv = 2;

  TActorId ReadActor;
  TActorId WriteActor;

public:
  TMaximumPrimeDevisorActor(int64_t number, TActorId readActor,
                            TActorId writeActor)
      : Number(number), ReadActor(readActor), WriteActor(writeActor) {}

  void Bootstrap() {
    Become(&TThis::StateFunc);
    Send(SelfId(), new NActors::TEvents::TEvWakeup());
  }

  STRICT_STFUNC(StateFunc,
                cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);)

  void HandleWakeup() {
    auto start = std::chrono::steady_clock::now();

    while (CurrentDiv <= Number) {
      if (Number % CurrentDiv == 0 && IsPrime(CurrentDiv)) {
        MaxPrime = CurrentDiv;
      }
      ++CurrentDiv;

      auto now = std::chrono::steady_clock::now();
      auto ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
              .count();
      if (ms >= 10) {
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
        return;
      }
    }

    Send(WriteActor, new MyActorModel::TEvents::TEvWriteValueRequest(MaxPrime));
    Send(ReadActor, new MyActorModel::TEvents::TEvDone());
    PassAway();
  }

private:
  static bool IsPrime(int64_t n) {
    if (n < 2)
      return false;
    for (int64_t i = 2; i * i <= n; ++i) {
      if (n % i == 0)
        return false;
    }
    return true;
  }
};

class TReadActor : public TActorBootstrapped<TReadActor> {
  std::istream &Strm;
  TActorId WriteActor;
  int Pending = 0;
  bool Finished = false;

public:
  TReadActor(std::istream &strm, TActorId writeActor)
      : Strm(strm), WriteActor(writeActor) {}

  void Bootstrap() {
    Become(&TThis::StateFunc);
    Send(SelfId(), new NActors::TEvents::TEvWakeup());
  }

  STRICT_STFUNC(StateFunc,
                cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
                hFunc(MyActorModel::TEvents::TEvDone, HandleDone);)

  void HandleWakeup() {
    int64_t value;
    if (Strm >> value) {
      ++Pending;
      Register(new TMaximumPrimeDevisorActor(value, SelfId(), WriteActor));
      Send(SelfId(), new NActors::TEvents::TEvWakeup());
    } else {
      Finished = true;
      if (Pending == 0) {
        Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
        PassAway();
      }
    }
  }

  void HandleDone(MyActorModel::TEvents::TEvDone::TPtr &) {
    --Pending;
    if (Finished && Pending == 0) {
      Send(WriteActor, new NActors::TEvents::TEvPoisonPill());
      PassAway();
    }
  }
};

THolder<IActor> CreateReadActor(std::istream &strm, TActorId writeActor) {
  return MakeHolder<TReadActor>(strm, writeActor);
}

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
  TDuration Latency;
  TInstant LastTime;

public:
  TSelfPingActor(const TDuration &latency) : Latency(latency) {}

  void Bootstrap() {
    LastTime = TInstant::Now();
    Become(&TSelfPingActor::StateFunc);
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
  });

  void HandleWakeup() {
    auto now = TInstant::Now();
    TDuration delta = now - LastTime;
    Y_VERIFY(delta <= Latency, "Latency too big");
    LastTime = now;
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency) {
  return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
  return ShouldContinue;
}
