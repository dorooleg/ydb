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
    const NActors::TActorId WriteActorId;
    bool EndOfInput = false;
    size_t PendingActors = 0;

public:
    TReadActor(const NActors::TActorId writeActorId)
        : WriteActorId(writeActorId), EndOfInput(false), PendingActors(0) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeUp() {
        int64_t value;
        if (std::cin >> value) {
            Register(CreateTMaximumPrimeDivisorActor(value, SelfId(), WriteActorId).Release());
            ++PendingActors;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            EndOfInput = true;
            if(PendingActors == 0) {
                Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone() {
        --PendingActors;
        if (EndOfInput && PendingActors == 0) {
            Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActorId) {
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
    int64_t Value;
    const NActors::TActorId ReadActorId;
    const NActors::TActorId WriteActorId;
    int64_t CurrentDivisor;
    int64_t MaximumPrimeDivisor;

public:
    TMaximumPrimeDivisorActor(int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId)
        : Value(value), ReadActorId(readActorId), WriteActorId(writeActorId), CurrentDivisor(2), MaximumPrimeDivisor(1) {}

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void HandleWakeup() {
        TInstant start = Now();
        while (Value > 1 && Now() - start < TDuration::MilliSeconds(10)) {
            while (Value % CurrentDivisor == 0) {
                MaximumPrimeDivisor = CurrentDivisor;
                Value /= CurrentDivisor;
            }
            ++CurrentDivisor;
        }

        if (Value > 1) {
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (CurrentDivisor > MaximumPrimeDivisor) {
                MaximumPrimeDivisor = CurrentDivisor;
            }
            Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaximumPrimeDivisor));
            Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
            PassAway();
        }
    }
};

THolder<NActors::IActor> CreateTMaximumPrimeDivisorActor(int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId) {
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

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum;

public:
    using TBase = NActors::TActor<TWriteActor>;
    TWriteActor() : TBase(&TWriteActor::StateFunc), Sum(0) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });

    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        auto& event = *ev->Get();
        Sum += event.Value;
    }

    void HandleDone() {
        std::cout << Sum << "\n";
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

THolder<NActors::IActor> CreateTWriteActor() {
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
