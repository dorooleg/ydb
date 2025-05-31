#include "actors.h"
#include "events.h"

#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/events.h>
#include <library/cpp/actors/core/hfunc.h>

#include <chrono>
#include <iostream>

using namespace std;
using namespace NActors;
using namespace MyActorModel;

static shared_ptr <TProgramShouldContinue> ShouldContinue =
        make_shared<TProgramShouldContinue>();

shared_ptr <TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
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
class TMaximumPrimeDevisorActor : public TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t value;
    int64_t outPrime = 1;
    int64_t currDiv = 2;

    NActors::TActorId ReaderActor;
    NActors::TActorId WriterActor;

public:
    TMaximumPrimeDevisorActor(int64_t value, TActorId readerActor, TActorId writerActor)
            : value(value), ReaderActor(readerActor), WriterActor(writerActor) {}

    void Bootstrap() {
        Become(&TThis::StateFunc);
        Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }

private:
    STRICT_STFUNC(StateFunc, {
            cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto start = chrono::steady_clock::now();

        while (currDiv <= value) {
            if (value % currDiv == 0 && IsPrime(currDiv)) {
                outPrime = currDiv;
            }
            currDiv++;

            auto now = chrono::steady_clock::now();
            auto timer = chrono::duration_cast<chrono::milliseconds>(now - start).count();
            if (timer >= 10) {
                Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }

        Send(WriterActor, new MyActorModel::TEvents::TEvWriteValueRequest(outPrime));
        Send(ReaderActor, new MyActorModel::TEvents::TEvDone());
        PassAway();
    }

private:
    static bool IsPrime(int64_t n) {
        if (n < 2) return false;
        if (n == 2) return true;
        if (n % 2 == 0) return false;
        for (int64_t i = 3; i * i <= n; i += 2) {
            if (n % i == 0) return false;
        }
        return true;
    }
};

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
class TReadActor : public TActorBootstrapped<TReadActor> {
    TActorId WriterActor;
    int64_t Pending = 0;
    bool Finished = false;

public:
    TReadActor(TActorId WriterActor) : WriterActor(WriterActor) {}

    void Bootstrap() {
        Become(&TThis::StateFunc);
        Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }

private:
    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(MyActorModel::TEvents::TEvDone, HandleDone);
    });


    void HandleWakeup() {
        int64_t value;
        if (cin >> value) {
            Pending++;
            Register(new TMaximumPrimeDevisorActor(value, SelfId(), WriterActor));
            Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Finished = true;
            if (Pending == 0) {
                Send(WriterActor, make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone(MyActorModel::TEvents::TEvDone::TPtr &) {
        Pending--;
        if (Finished && Pending == 0) {
            Send(WriterActor, make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

THolder <IActor> CreateReadActor(TActorId WriterActor) {
    return MakeHolder<TReadActor>(WriterActor);
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

class TWriteActor : public TActor<TWriteActor> {
    int64_t Sum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(MyActorModel::TEvents::TEvWriteValueRequest, HandleWrite);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoison);
    });


    void HandleWrite(MyActorModel::TEvents::TEvWriteValueRequest::TPtr &ev) {
        Sum += ev->Get()->Value;
    }

    void HandlePoison() {
        cout << Sum << endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};


THolder <IActor> CreateWriteActor() { return MakeHolder<TWriteActor>(); }


class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration &latency) : Latency(latency) {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto now = TInstant::Now();
        TDuration delta = now - LastTime;
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastTime = now;
        Send(SelfId(), make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder <NActors::IActor> CreateSelfPingActor(const TDuration &latency) {
    return MakeHolder<TSelfPingActor>(latency);
}



