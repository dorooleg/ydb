#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
*/
class TReadActor : public NActors::TActorBootstrapped<TReadActor>
{
    bool finish_flag = false;
    const NActors::TActorId WriteActorId;
    int count = 0;

public:
    TReadActor(const NActors::TActorId writeActorId)
        : finish_flag(false), WriteActorId(writeActorId)
    {
    }
    /*
    2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup
    */
    void Bootstrap()
    {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
    /*
     3. После получения этого сообщения считывается новое int64_t значение из strm
     */
    void HandleWakeUp()
    {
        int64_t value;
        if (std::cin >> value)
        {
            /*
             4. После этого порождается новый TMaximumPrimeDevisorActor который занимается вычислениями
             */
            Register(CreateTMaximumPrimeDevisorActor(value, SelfId(), WriteActorId).Release());
            count++;
            /*
             5. Далее актор посылает себе сообщение NActors::TEvents::TEvWakeup чтобы не блокировать поток этим актором
             */
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }

        /*
        6. Актор дожидается завершения всех TMaximumPrimeDevisorActor через TEvents::TEvDone
        */
        else
        {
            finish_flag = true;
            /*
            7. Когда чтение из файла завершено и получены подтверждения от всех TMaximumPrimeDevisorActor,
            этот актор отправляет сообщение NActors::TEvents::TEvPoisonPill в TWriteActor
            */
            if (count == 0)
            {
                Send(WriteActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
                PassAway();
            }
        }
    }

    void HandleDone()
    {
        count--;
        if (finish_flag && count == 0)
        {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });
};

/*
Требования к TMaximumPrimeDevisorActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
*/
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>
{
    /*
    2. В конструкторе этот актор принимает:
     - значение для которого нужно вычислить простое число
     - ActorId отправителя (ReadActor)
     - ActorId получателя (WriteActor)
    */
    int64_t Value;
    const NActors::TActorId ReadActorId;
    const NActors::TActorId WriteActorId;
    int64_t CurrentDivisor = 2;
    int64_t MaxDivisor = 1;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId readActorId, const NActors::TActorId writeActorId)
        : Value(value), ReadActorId(readActorId), WriteActorId(writeActorId)
    {
    }
    /*
    2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup по вызову которого происходит вызов Handler для вычислений
    */
    void Bootstrap()
    {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
    /*
    3. Вычисления нельзя проводить больше 10 миллисекунд
    */
    void HandleWakeUp()
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = std::chrono::high_resolution_clock::now();
        ;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        while (CurrentDevisor * CurrentDevisor <= Value)
        {
            if (Value % CurrentDevisor == 0)
            {
                MaxDivisor = CurrentDevisor;
                Value /= CurrentDevisor;
            }
            else
            {
                CurrentDevisor++;
            }
            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            /*
            4. По истечении этого времени нужно сохранить текущее состояние вычислений в акторе и отправить себе NActors::TEvents::TEvWakeup
            */
            if (duration > 10)
            {
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }
        }
        /*
        5. Когда результат вычислен он посылается в TWriteActor c использованием сообщения TEvWriteValueRequest
        6. Далее отправляет ReadActor сообщение TEvents::TEvDone
        7. Завершает свою работу
        */

        if (Value > 1)
        {
            MaxDivisor = Value;
        }

        Send(WriteActorId, std::make_unique<TEvents::TEvWriteValueRequest>(MaxDivisor));
        Send(ReadActorId, std::make_unique<TEvents::TEvDone>());
        PassAway();
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });
};

/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
*/
class TWriteActor : public NActors::TActor<TWriteActor>
{
    int64_t Sum = 0;
    /*
    2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
    2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
    4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway
    */
public:
    TWriteActor() : TActor(&TWriteActor::StateFunc), Sum(0) {}

    void HandleWriteValueRequest(TEvents::TEvWriteValueRequest::TPtr &ev)
    {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill()
    {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor>
{
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration &latency)
        : Latency(latency)
    {
    }

    void Bootstrap()
    {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup()
    {
        auto now = TInstant::Now();
        TDuration delta = now - LastTime;
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration &latency)
{
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue()
{
    return ShouldContinue;
}
