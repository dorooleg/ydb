#include "actors.h"
#include "events.h"

#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <util/datetime/base.h>

namespace {
constexpr auto TIME_SLICE = TDuration::MilliSeconds(10);
auto ShouldContinue = std::make_shared<TProgramShouldContinue>();
} // namespace

class TMaximumPrimeDevisorActor
    : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
  const int64_t Number;
  const NActors::TActorId OriginatorId;
  const NActors::TActorId CollectorId;

  int64_t Value{Number};
  int64_t Divisor{2};
  int64_t MaxFactor{1};

public:
  TMaximumPrimeDevisorActor(int64_t number, NActors::TActorId originator,
                            NActors::TActorId collector)
      : Number(number), OriginatorId(originator), CollectorId(collector) {
    MaxFactor = Number <= 0 ? 0 : (Number == 1 ? 1 : 1);
    Value = Number <= 0 ? 0 : (Number == 1 ? 1 : Number);
  }

  void Bootstrap() {
    Become(&TMaximumPrimeDevisorActor::StateFunc);
    ScheduleNextCycle();
  }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvWakeup::EventType, ProcessFactorization);
  });

private:
  void ProcessFactorization() {
    if (Value <= 1) {
      SendResultAndDie(Value == 0 ? 0 : MaxFactor);
      return;
    }

    auto startTime = TInstant::Now();

    // Обработка четных чисел
    if (Divisor == 2) {
      DivideByFactor(2);
      Divisor = 3;
      if (IsTimeUp(startTime))
        return;
    }

    // Проверка нечетных делителей
    while (Divisor * Divisor <= Value) {
      if (IsTimeUp(startTime))
        return;
      DivideByFactor(Divisor);
      Divisor += 2;
    }

    // Если остался простой остаток
    if (Value > 1) {
      MaxFactor = Value;
    }

    SendResultAndDie(MaxFactor);
  }

  void DivideByFactor(int64_t factor) {
    if (Value % factor == 0) {
      MaxFactor = factor;
      while (Value % factor == 0) {
        Value /= factor;
      }
    }
  }

  bool IsTimeUp(TInstant startTime) {
    if (TInstant::Now() - startTime > TIME_SLICE) {
      ScheduleNextCycle();
      return true;
    }
    return false;
  }

  void ScheduleNextCycle() {
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  void SendResultAndDie(int64_t result) {
    Send(CollectorId, MakeHolder<TEvents::TEvWriteValueRequest>(result));
    Send(OriginatorId, MakeHolder<TEvents::TEvDone>());
    PassAway();
  }
};

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
  std::istream &Input;
  const NActors::TActorId AggregatorId;

  int PendingTasks{0};
  bool InputExhausted{false};

public:
  TReadActor(std::istream &input, NActors::TActorId aggregator)
      : Input(input), AggregatorId(aggregator) {}

  void Bootstrap() {
    Become(&TReadActor::StateFunc);
    ScheduleNextRead();
  }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvWakeup::EventType, ReadNext);
    hFunc(TEvents::TEvDone, HandleTaskComplete);
  });

private:
  void ReadNext() {
    if (InputExhausted) {
      TryFinish();
      return;
    }

    int64_t value;
    if (Input >> value) {
      Register(new TMaximumPrimeDevisorActor(value, SelfId(), AggregatorId));
      PendingTasks++;
      ScheduleNextRead();
    } else {
      InputExhausted = true;
      TryFinish();
    }
  }

  void HandleTaskComplete(TEvents::TEvDone::TPtr &) {
    PendingTasks--;
    TryFinish();
  }

  void TryFinish() {
    if (InputExhausted && PendingTasks == 0) {
      Send(AggregatorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
      PassAway();
    }
  }

  void ScheduleNextRead() {
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }
};

class TWriteActor : public NActors::TActor<TWriteActor> {
  int64_t Sum{0};

public:
  TWriteActor() : NActors::TActor<TWriteActor>(&TWriteActor::StateFunc) {}

  STRICT_STFUNC(StateFunc, {
    hFunc(TEvents::TEvWriteValueRequest, AccumulateValue);
    cFunc(NActors::TEvents::TEvPoisonPill::EventType, OutputAndShutdown);
  });

private:
  void AccumulateValue(TEvents::TEvWriteValueRequest::TPtr &ev) {
    Sum += ev->Get()->Value;
  }

  void OutputAndShutdown() {
    Cout << Sum << Endl;
    GetProgramShouldContinue()->ShouldStop(0);
    PassAway();
  }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
  TDuration Latency;
  TInstant LastTime;

public:
  TSelfPingActor(const TDuration &latency) : Latency(latency) {}

  void Bootstrap(const NActors::TActorContext &ctx) {
    LastTime = TInstant::Now();
    Become(&TSelfPingActor::StateFunc);
    ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr &ev,
                    const NActors::TActorContext &ctx) {
    Y_UNUSED(ev);
    auto now = TInstant::Now();
    TDuration delta = now - LastTime;
    Y_VERIFY(delta <= Latency, "Latency too big");
    LastTime = now;
    ctx.Send(ctx.SelfID, std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  STRICT_STFUNC(StateFunc,
                { HFunc(NActors::TEvents::TEvWakeup, HandleWakeup); })
};

THolder<NActors::IActor> CreateReadActor(std::istream &input,
                                         NActors::TActorId aggregatorId) {
  return MakeHolder<TReadActor>(input, aggregatorId);
}

THolder<NActors::IActor> CreateWriteActor() {
  return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency) {
  return MakeHolder<TSelfPingActor>(latency);
}

THolder<NActors::IActor>
CreateMaximumPrimeDevisorActor(int64_t number, NActors::TActorId originator,
                               NActors::TActorId collector) {
  return MakeHolder<TMaximumPrimeDevisorActor>(number, originator, collector);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
  return ShouldContinue;
}