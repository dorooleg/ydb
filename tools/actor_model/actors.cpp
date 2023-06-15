#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <typeinfo>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor
*/

/*
Требования к TReadActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup
3. После получения этого сообщения считывается новое int64_t значение из strm
4. После этого порождается новый TMaximumPrimeDevisorActor который занимается вычислениями
5. Далее актор посылает себе сообщение NActors::TEvents::TEvWakeup чтобы не блокировать поток этим актором
6. Актор дожидается завершения всех TMaximumPrimeDevisorActor через TEvents::TEvDone
7. Когда чтение из файла завершено и получены подтверждения от всех TMaximumPrimeDevisorActor,
этот актор отправляет сообщение NActors::TEvents::TEvPoisonPill в TWriteActor

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

class TReadActor: public NActors::TActorBootstrapped<TReadActor>{
    const NActors::TActorId writeActor;

    bool isDone;
    bool isEmpty;
    int divisors;
    

public:
    TReadActor(const NActors::TActorId writeActor)
            : writeActor(writeActor), isDone(false), isEmpty(true), divisors(0)
    {}

    void Bootstrap() {
      Become(&TReadActor::StateFunc);
      Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
      cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
      cFunc(TEvents::TEvDone::EventType, HandleDivisor);
    });

    void HandleWakeup() {
      int64_t value;
      if (std::cin >> value) {
        isEmpty = false;
        Register(CreateTMaximumPrimeDevisorActor(SelfId(), writeActor, value).Release());
        divisors = divisors + 1;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
      } else {
        if (isEmpty){
          divisors = divisors + 1;
          Register(CreateTMaximumPrimeDevisorActor(SelfId(), writeActor, 0).Release());
        }
        isDone = true;
      }
    }

    void HandleDivisor(){
      divisors = divisors - 1;
      if(isDone && !divisors){
        Send(writeActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
      }
    }
};


/*
Требования к TMaximumPrimeDevisorActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В конструкторе этот актор принимает:
 - значение для которого нужно вычислить простое число
 - ActorId отправителя (ReadActor)
 - ActorId получателя (WriteActor)
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup по вызову которого происходит вызов Handler для вычислений
3. Вычисления нельзя проводить больше 10 миллисекунд
4. По истечении этого времени нужно сохранить текущее состояние вычислений в акторе и отправить себе NActors::TEvents::TEvWakeup
5. Когда результат вычислен он посылается в TWriteActor c использованием сообщения TEvWriteValueRequest
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

class TMaximumPrimeDevisorActor: public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>{
    const NActors::TActorId readActor;
    const NActors::TActorId writeActor;

    int64_t copy;
    int64_t value;
    int64_t largest_div;

public:
    TMaximumPrimeDevisorActor(const NActors::TActorId readActor, const NActors::TActorId writeActor, int64_t value)
            : readActor(readActor), writeActor(writeActor), copy(value), value(value), largest_div(2)
    {}

    void Bootstrap() {
      Become(&TMaximumPrimeDevisorActor::StateFunc);
      Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
      cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
      auto start = std::chrono::system_clock::now();

      if(value != 0 || value != 1 || value != -1){
        for (; largest_div <= copy; largest_div++) {
          if (copy % largest_div == 0) {
            copy = copy / largest_div;
            largest_div = largest_div - 1;
          }
          auto current = std::chrono::system_clock::now();
          if(std::chrono::duration_cast<std::chrono::milliseconds>(current - start).count() > 10){
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            return;
          }
        }
      }
      else{
        largest_div = value;
      }

      Send(writeActor, std::make_unique<TEvents::TEvWriteValueRequest>(largest_devisor));
      Send(readActor, std::make_unique<TEvents::TEvDone>());
      PassAway();
    }
};

/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway

TWriteActor
    TEvents::TEvWriteValueRequest ev:
        Sum += ev->Value

    NActors::TEvents::TEvPoisonPill::EventType:
        Cout << Sum << Endl;
        ShouldStop()
        PassAway()
*/

class TWriteActor: public NActors::TActorBootstrapped<TWriteActor>{
    int64_t sum;

public:
    TWriteActor()
            : sum(0)
    {}

    void Bootstrap() {
      Become(&TWriteActor::StateFunc);
    }

    STRICT_STFUNC(StateFunc, {
      cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleFinish);
      hFunc(TEvents::TEvWriteValueRequest, HandleDivisor)
    });

    void HandleFinish() {
      std::cout << sum << std::endl;
      ShouldContinue->ShouldStop();
      PassAway();
    }

    void HandleDivisor(TEvents::TEvWriteValueRequest::TPtr& message){
      sum += message->Get()->value;
    }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency)
        : Latency(latency)
    {}

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

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor) {
  return MakeHolder<TReadActor>(writeActor);
}
THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(const NActors::TActorIdentity readActor, const NActors::TActorId writeActor, int64_t value) {
  return MakeHolder<TMaximumPrimeDevisorActor>(readActor, writeActor, value);
}
THolder<NActors::IActor> CreateTWriteActor() {
  return MakeHolder<TWriteActor>();
}