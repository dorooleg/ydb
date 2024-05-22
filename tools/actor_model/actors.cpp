#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

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

// TODO: напишите реализацию TMaximumPrimeDevisorActor
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId)
        : Value(value)
       , ReadActorId(readActorId)
       , WriteActorId(writeActorId)
       , StartTime(TInstant::Now())
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        int64_t result = CalculateMaximumPrimeDivisor(Value);
        TDuration elapsed = TInstant::Now() - StartTime;
        if (elapsed > TDuration::MilliSeconds(10)) {
            // Save current state and send TEvWakeup to self
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            // Send result to WriteActor and TEvDone to ReadActor
            Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(result));
            Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
            PassAway();
        }
    }

private:
    int64_t Value;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    TInstant StartTime;

    int64_t CalculateMaximumPrimeDivisor(int64_t value) {
        if (value == 1) {
            return 1;
        }
        int64_t maxDivisor = 0;
        while (value % 2 == 0){
            value = value / 2;
            maxDivisor = 2;
        }
        for (int64_t i = 3; i <= std::sqrt(value); i += 2) {
            if (value % i == 0){
                maxDivisor = i;
                value = value / i;
            }
        }
        if (value > 1) {
            maxDivisor = value;
        }
        return maxDivisor;
    }
};

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

// TODO: напишите реализацию TReadActor
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
public:
    TReadActor(std::istream& strm, NActors::TActorId write_actor_id)
        : Strm(strm), actors_counter(0), done(false), WriteActorId(write_actor_id)
    {}

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
        while (Strm >> value) {
            auto maxPrimeDevisorActor = MakeHolder<TMaximumPrimeDevisorActor>(value, SelfId(), WriteActorId);
            Register(maxPrimeDevisorActor.Release());
            actors_counter++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
        done = true;
    }
    
    void HandleDone() {
        actors_counter--;
        if (done && actors_counter==0){
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }


private:
    std::istream& Strm;
    NActors::TActorId WriteActorId;
    int actors_counter;
    bool done;
};

THolder<NActors::IActor> CreateReadActor(std::istream& strm, NActors::TActorId write_actor_id) {
    return MakeHolder<TReadActor>(strm, write_actor_id);
}

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

// TODO: напишите реализацию TWriteActor
class TWriteActor : public NActors::TActor<TWriteActor> {
public:
    TWriteActor():TActor(&TWriteActor::StateFunc), Sum(0)
    {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleValue); 
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleValue(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->value;
    }

    void HandlePoisonPill() {
        Cout << Sum << Endl;
        GetProgramShouldContinue()->ShouldStop();
        PassAway();
    }

private:
    int64_t Sum;
};

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

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
