#include "actors.h"
#include "events.h"
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
    int ActiveWorkers = 0;
    bool FinishInput = false;

    std::istream& stream;
    NActors::TActorId WriteActor;

    //определяем состояние актора
    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

public:
    TReadActor(std::istream& inputStream, NActors::TActorId writeActor) : stream(inputStream), WriteActor(writeActor) {}

    void Bootstrap() {
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        Become(&TReadActor::StateFunc);
    }

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        int64_t value;
        if (stream >> value) {

            //успешное чтение значения из потока
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            ActiveWorkers++;

        } else {

            //конец потока или ошибка
            FinishInput = true;
            if (ActiveWorkers == 0) {

                //нет активных рабочих, можно безопасно завершить работу
                Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone(TEvents::TEvDone::TPtr) {
        ActiveWorkers--;
        if (FinishInput && ActiveWorkers == 0) {

             // все данные обработаны, можно завершить работу
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};
// создаем актора чтения
THolder<NActors::IActor> CreateReadActor(std::istream& inputStream, NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(inputStream, writeActor);
}

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
    int64_t NumberToFactorize;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;
    int64_t CurrentPrimeCandidate = 2;

    STRICT_STFUNC(StateFunc, {
        hFunc(NActors::TEvents::TEvWakeup, HandleWakeup);
    });

    void HandleWakeup(NActors::TEvents::TEvWakeup::TPtr) {
        TInstant start = TInstant::Now();
        int64_t maxPrime = 1;

        while (CurrentPrimeCandidate <= NumberToFactorize) {
            if (NumberToFactorize % CurrentPrimeCandidate == 0) {
                maxPrime = ProcessPrimeCandidate();
            }
            CurrentPrimeCandidate++;
            if (ShouldYield(start)) {
                Yield();
                return;
            }
        }

        SendResultAndTerminate(maxPrime);
    }

    int64_t ProcessPrimeCandidate() {
        int64_t maxPrime = CurrentPrimeCandidate;
        do {
            NumberToFactorize /= CurrentPrimeCandidate;
        } while (NumberToFactorize % CurrentPrimeCandidate == 0);
        return maxPrime;
    }

    bool ShouldYield(const TInstant& start) const {
        return TInstant::Now() - start > TDuration::MilliSeconds(10);
    }

    void Yield() {
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

    void SendResultAndTerminate(int64_t maxPrime) {
        auto result = MakeHolder<TEvents::TEvWriteValueRequest>(maxPrime);
        Send(WriteActor, result.Release());
        Send(ReadActor, new TEvents::TEvDone());
        PassAway();
    }

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& readActor, const NActors::TActorId& writeActor):
        NumberToFactorize(value),
        ReadActor(readActor),
        WriteActor(writeActor) {
    }

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }
};
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t NumberToFactorize, NActors::TActorId ReadActor, NActors::TActorId WriteActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(NumberToFactorize, ReadActor, WriteActor);
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

class TWriteActor : public NActors::TActor<TWriteActor> {
    int64_t Sum = 0;

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValueRequest);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

public:

    TWriteActor() :
        TActor(&TWriteActor::StateFunc) {}
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
