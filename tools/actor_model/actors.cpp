#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <cmath>

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

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    NActors::TActorId WriteActorId;
    size_t PendingTasks = 0;
    bool InputEnded = false;

public:
    TReadActor(const NActors::TActorId& writeActorId)
        : WriteActorId(writeActorId) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            auto maxPrimeActor = Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActorId));
            PendingTasks++;
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        } else {
            InputEnded = true;
            if (PendingTasks == 0) {
                Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
            }
        }
    }

    void HandleDone() {
        PendingTasks--;
        if (InputEnded && PendingTasks == 0) {
            Send(WriteActorId, new NActors::TEvents::TEvPoisonPill());
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

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;
    NActors::TActorId ReadActorId;
    NActors::TActorId WriteActorId;
    int64_t CurrentDivisor = 2;
    int64_t MaxPrimeDivisor = 1;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId)
        : Value(value), ReadActorId(readActorId), WriteActorId(writeActorId) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        if (Value == 1) {
            HandleResult(1);
            return;
        }
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto startTime = TInstant::Now();
        int64_t n = Value;
        
        if (n % 2 == 0) {
            MaxPrimeDivisor = 2;
            while (n % 2 == 0) {
                n /= 2;
            }
        }
        
        for (int64_t i = 3; i <= std::sqrt(n); i += 2) {
            if (n % i == 0) {
                MaxPrimeDivisor = i;
                while (n % i == 0) {
                    n /= i;
                }
            }
            
            if (TInstant::Now() - startTime > TDuration::MilliSeconds(10)) {
                Send(SelfId(), new NActors::TEvents::TEvWakeup());
                return;
            }
        }
        
        if (n > 2) {
            MaxPrimeDivisor = n;
        }
        
        HandleResult(MaxPrimeDivisor);
    }

    void HandleResult(int64_t result) {
        Send(WriteActorId, new TEvents::TEvWriteValueRequest(result));
        Send(ReadActorId, new TEvents::TEvDone());
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

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : NActors::TActor<TWriteActor>() {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop(0);
        PassAway();
    }
};

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId& writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActorId, const NActors::TActorId& writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
}

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
