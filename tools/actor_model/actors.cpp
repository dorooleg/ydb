#include "actors.h"
#include "events.h"
#include "library/cpp/actors/interconnect/types.h"
#include "util/datetime/base.h"
#include "util/system/types.h"
#include "util/system/yassert.h"
#include <cmath>
#include <istream>
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor
*/

/*
Требования к TReadActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup
3. После получения этого сообщения считывается новое int64_t значение из strm
4. После этого порождается новый TMaximumPrimeDevisorActor который занимается
вычислениями
5. Далее актор посылает себе сообщение NActors::TEvents::TEvWakeup чтобы не
блокировать поток этим актором
6. Актор дожидается завершения всех TMaximumPrimeDevisorActor через
TEvents::TEvDone
7. Когда чтение из файла завершено и получены подтверждения от всех
TMaximumPrimeDevisorActor, этот актор отправляет сообщение
NActors::TEvents::TEvPoisonPill в TWriteActor

TReadActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)

    NActors::TEvents::TEvWakeup:
        if read(strm) -> value:
            register(TMaximumPrimeDevisorActor(value, self, receipment))
            send(self, NActors::TEvents::TEvWakeup)
        else:
            ...

    TEvents::TEvDone:
        if Finish:
            send(receipment, NActors::TEvents::TEvPoisonPill)
        else:
            ...
*/

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
  std::reference_wrapper<std::istream> In;
  ui32 AliveActors = 0;
  TActorId WriteActorId;

public:
  TReadActor(std::istream &in, TActorId writeActorId)
      : In(in), WriteActorId(writeActorId) {}

  void Bootstrap() {
    Become(&TReadActor::StateFunc);
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    cFunc(TEvents::TEvDone::EventType, HandleMaxDivisorDone);
  });

private:
  void HandleWakeup() {
    i64 val;
    if (In.get() >> val) {
      Register(
          CreateMaximumDivisorActor(val, SelfId(), WriteActorId).Release());
      AliveActors++;
      Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    } else if (AliveActors == 0)
      // Empty input
      Finish();
  }

  void HandleMaxDivisorDone() {
    Y_VERIFY(AliveActors != 0);
    if (--AliveActors == 0) {
      Finish();
    }
  }

  void Finish() {
    Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
    PassAway();
  }
};

THolder<NActors::IActor> CreateReadActor(std::istream &in,
                                         TActorId writeActorId) {
  return MakeHolder<TReadActor>(in, writeActorId);
}

/*
Требования к TMaximumPrimeDevisorActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В конструкторе этот актор принимает:
 - значение для которого нужно вычислить простое число
 - ActorId отправителя (ReadActor)
 - ActorId получателя (WriteActor)
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup по вызову
которого происходит вызов Handler для вычислений
3. Вычисления нельзя проводить больше 10 миллисекунд
4. По истечении этого времени нужно сохранить текущее состояние вычислений в
акторе и отправить себе NActors::TEvents::TEvWakeup
5. Когда результат вычислен он посылается в TWriteActor c использованием
сообщения TEvWriteValueRequest
6. Далее отправляет ReadActor сообщение TEvents::TEvDone
7. Завершает свою работу

TMaximumPrimeDevisorActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)

    NActors::TEvents::TEvWakeup:
        calculate
        if > 10 ms:
            Send(SelfId(), NActors::TEvents::TEvWakeup)
        else:
            Send(WriteActor, TEvents::TEvWriteValueRequest)
            Send(ReadActor, TEvents::TEvDone)
            PassAway()
*/

class TMaximumDivisorActor
    : public NActors::TActorBootstrapped<TMaximumDivisorActor> {
  i64 TargetValue;
  i64 PotentialDivisor = 2;
  TActorId ReadActorId;
  TActorId WriteActorId;

  constexpr static TDuration WORK_TIME = TDuration::MilliSeconds(10);

public:
  TMaximumDivisorActor(i64 targetValue, TActorId readActorId,
                       TActorId writeActorId)
      : TargetValue(abs(targetValue)), ReadActorId(readActorId),
        WriteActorId(writeActorId) {
    Y_VERIFY(TargetValue != 0, "Target value must not be zero");
  }

  void Bootstrap() {
    if (TargetValue == 1) {
      Finish(1);
      return;
    }
    Become(&TMaximumDivisorActor::StateFunc);
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
  });

private:
  void HandleWakeup() {
    auto start = TInstant::Now();
    while (TInstant::Now() - start < WORK_TIME) {
      if (maxPrimeDivisorCycle()) {
        Finish(PotentialDivisor);
        return;
      }
    }
    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
  }

  bool maxPrimeDivisorCycle() {
    if (TargetValue % PotentialDivisor == 0) {
      do {
        TargetValue /= PotentialDivisor;
      } while (TargetValue % PotentialDivisor == 0);
      if (TargetValue == 1)
        return true;
    }

    PotentialDivisor++;

    if (PotentialDivisor * PotentialDivisor > TargetValue) {
      PotentialDivisor = TargetValue;
      return true;
    }

    return false;
  }

  void Finish(i64 divisor) {
    Send(WriteActorId,
         std::make_unique<TEvents::TEvWriteValueRequest>(divisor));
    Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
    PassAway();
  }
};

THolder<NActors::IActor> CreateMaximumDivisorActor(i64 targetValue,
                                                   TActorId readActorId,
                                                   TActorId writeActorId) {
  return MakeHolder<TMaximumDivisorActor>(targetValue, readActorId,
                                          writeActorId);
}

/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
2. Этот актор получает два типа сообщений
NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в
TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout
посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение
через PassAway

TWriteActor
    TEvents::TEvWriteValueRequest ev:
        Sum += ev->Value

    NActors::TEvents::TEvPoisonPill::EventType:
        Cout << Sum << Endl;
        ShouldStop()
        PassAway()
*/

class TWriteActor : public NActors::TActor<TWriteActor> {
  std::reference_wrapper<std::ostream> Out;
  i64 Sum = 0;

public:
  TWriteActor(std::ostream &out) : TActor(&TWriteActor::StateFunc), Out(out) {
    // empty
  }

  STRICT_STFUNC(StateFunc, {
    cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    hFunc(TEvents::TEvWriteValueRequest, HandleWriteValue);
  });

private:
  void HandlePoisonPill() {
    Out.get() << Sum << std::endl;
    ShouldContinue->ShouldStop();
    PassAway();
  }

  void HandleWriteValue(TEvents::TEvWriteValueRequest::TPtr &ev) {
    Sum += ev->Get()->Divisor;
  }
};

THolder<NActors::IActor> CreateWriteActor(std::ostream &out) {
  return MakeHolder<TWriteActor>(out);
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
