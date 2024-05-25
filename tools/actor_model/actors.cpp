#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <memory>
#include <chrono>

static auto ShouldContinueFlag = std::make_shared<TProgramShouldContinue>();

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
private:
    const NActors::TActorId OutputActor;
    bool ReadingFinish;
    size_t ActiveWorkersCount;

public:
    TReadActor(const NActors::TActorId outputActor)
        : OutputActor(outputActor), ReadingFinish(false), ActiveWorkersCount(0) {}

    void Bootstrap() {
        Become(&TReadActor::StateHandler);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateHandler, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvProcessingFinished::EventType, HandleCalculationDone);
    });

    void HandleWakeUp() {
        int64_t value;
        if (std::cin >> value) {
            // Если чтение удачное
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), OutputActor).Release());
            ++ActiveWorkersCount;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            // Если ошибка
            ReadingFinish = true;
            CheckCompletion();
        }
    }

    void HandleCalculationDone() {
        --ActiveWorkersCount;
        CheckCompletion();
    }

    void CheckCompletion() {
        if (ReadingFinish && ActiveWorkersCount == 0) {
            Send(OutputActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
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
private:
    int64_t value;
    const NActors::TActorIdentity InputActor;
    const NActors::TActorId OutputActor;
    int64_t LargestPrimeDivisor;
    int64_t CurrentDivisor;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity inputActor, const NActors::TActorId outputActor)
        : value(value), InputActor(inputActor), OutputActor(outputActor),
          LargestPrimeDivisor(0), CurrentDivisor(2) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateHandler);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateHandler, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {
        auto startTime = std::chrono::high_resolution_clock::now();
        while (CurrentDivisor * CurrentDivisor <= value) {
            if (value % CurrentDivisor == 0) {
                LargestPrimeDivisor = CurrentDivisor;
                value /= CurrentDivisor;
            } else {
                ++CurrentDivisor;
            }

            if (std::chrono::high_resolution_clock::now() - startTime > std::chrono::milliseconds(10)) {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        if (value > 1) {
            LargestPrimeDivisor = value;
        }

        Send(OutputActor, std::make_unique<TEvents::TEvWriteValueRequest>(LargestPrimeDivisor));
        Send(InputActor, std::make_unique<TEvents::TEvProcessingFinished>());
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
private:
    int AccumulatedSum;

public:
    using TBase = NActors::TActor<TWriteActor>;
    TWriteActor() : TBase(&TWriteActor::Handler), AccumulatedSum(0) {}

    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, OnWriteValue);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, OnPoisonPill);
    });

    void OnWriteValue(TEvents::TEvWriteValueRequest::TPtr& ev) {
        AccumulatedSum += ev->Get()->Value;
    }

    void OnPoisonPill() {
        std::cout << AccumulatedSum << std::endl;
        ShouldContinueFlag->ShouldStop();
        PassAway();
    }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastCheck;

public:
    TSelfPingActor(const TDuration& latency) : Latency(latency) {}

    void Bootstrap() {
        LastCheck = TInstant::Now();
        Become(&TSelfPingActor::StateHandler);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateHandler, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto now = TInstant::Now();
        Y_VERIFY(now - LastCheck <= Latency, "Latency too big");
        LastCheck = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

// Всевозможные ретёрны

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinueFlag;
}

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId outputActor) {
    return MakeHolder<TReadActor>(outputActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity inputActor, const NActors::TActorId outputActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, inputActor, outputActor);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

