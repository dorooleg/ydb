#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDivisorActor, TWriteActor
*/

/*
Требования к TReadActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup
3. После получения этого сообщения считывается новое int64_t значение из strm
4. После этого порождается новый TMaximumPrimeDivisorActor который занимается вычислениями
5. Далее актор посылает себе сообщение NActors::TEvents::TEvWakeup чтобы не блокировать поток этим актором
6. Актор дожидается завершения всех TMaximumPrimeDivisorActor через TEvents::TEvDone
7. Когда чтение из файла завершено и получены подтверждения от всех TMaximumPrimeDivisorActor,
этот актор отправляет сообщение NActors::TEvents::TEvPoisonPill в TWriteActor

TReadActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)

    NActors::TEvents::TEvWakeup:
        if read(strm) -> value:
            register(TMaximumPrimeDivisorActor(value, self, receipment))
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
    bool Finish;
    const NActors::TActorId WriteActorId;
    int Count = 0;

public:
    TReadActor(const NActors::TActorId writeActorId)
        : Finish(false), WriteActorId(writeActorId)
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
        if (std::cin >> value) {
            Register(CreateMaximumPrimeDivisorActor(value, SelfId(), WriteActorId).Release());
            Count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Finish = true;
            if (Count == 0) {
                Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone() {
        Count--;
        if (Finish && Count == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}

/*
Требования к TMaximumPrimeDivisorActor:
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

TMaximumPrimeDivisorActor
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

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
    const NActors::TActorId ReadActorId;
    const NActors::TActorId WriteActorId;
    int64_t InitialValue;
    int64_t Value;
    int64_t CurrentDivisor = 2;
    int64_t MaxDivisor = 0;

public:
    TMaximumPrimeDivisorActor(const int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId)
            : ReadActorId(readActorId), WriteActorId(writeActorId), InitialValue(abs(value)), Value(abs(value))
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto start = TInstant::Now();
        TInstant end;
        TDuration max_delay = TDuration::MilliSeconds(10);
        for (int64_t i=CurrentDivisor; i <= sqrt(Value); i+=(i==2?1:2)) {
            CurrentDivisor = i;

            while (Value % CurrentDivisor == 0) {
                MaxDivisor = CurrentDivisor;
                Value /= CurrentDivisor;
                end = TInstant::Now();
                if (end - start >= max_delay) {
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            end = TInstant::Now();
            if (end - start >= max_delay) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        if (Value >= 2) {
            MaxDivisor = Value;
        }

        if (InitialValue == 1) {
            MaxDivisor = 1;
        }

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }
};

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(const int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId) {
    return MakeHolder<TMaximumPrimeDivisorActor>(value, readActorId, writeActorId);
}

/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDivisorActor и прибавляет его к локальной сумме
4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway

TWriteActor
    TEvents::TEvWriteValueRequest ev:
        Sum += ev->Value

    NActors::TEvents::TEvPoisonPill::EventType:
        Cout << Sum << Endl;
        ShouldStop()
        PassAway()
*/

class TWriterActor : public NActors::TActorBootstrapped<TWriterActor> {
    int64_t Sum = 0;

public:
    TWriterActor()
    {}

    void Bootstrap() {
        Become(&TWriterActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr& event) {
        int64_t val = (*event->Get()).Value;
        Sum += val;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }


};

THolder<NActors::IActor> CreateWriterActor() {
    return MakeHolder<TWriterActor>();
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
