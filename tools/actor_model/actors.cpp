#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ProgramContinue = std::make_shared<TProgramShouldContinue>();

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
    bool Completed = false;
    const NActors::TActorId WriterActorId;
    int RunningActors;
    bool InitialRun;
public:
    TReadActor(const NActors::TActorId& writerId) : WriterActorId(writerId), RunningActors(0), InitialRun(true) {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    void HandleWakeup() {
        int64_t value;
        if (std::cin >> value) {
            InitialRun = false;
            Register(CreateTMaximumPrimeDevisorActor(value, SelfId(), WriterActorId).Release());
            RunningActors++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (InitialRun) {
                Register(CreateTMaximumPrimeDevisorActor(0, SelfId(), WriterActorId).Release());
                RunningActors++;
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
            Completed = true;
        }
    }

    void HandleDone() {
        RunningActors--;
        if (Completed && RunningActors == 0) {
            Send(WriterActorId, std::make_unique<TEvents::TEvPoisonPill>());
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
    int64_t ValueToProcess;
    const NActors::TActorIdentity ReaderActor;
    const NActors::TActorId WriterActor;
    int64_t LargestPrime;
    int64_t NextCheckValue;
    int64_t Divider;
    bool IsPrime;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity& readerId, const NActors::TActorId& writerId)
        : ValueToProcess(value), ReaderActor(readerId), WriterActor(writerId), LargestPrime(0), NextCheckValue(1), Divider(2), IsPrime(true) {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto StartTime = std::chrono::high_resolution_clock::now();

        while (NextCheckValue <= ValueToProcess) {
            for (int64_t i = Divider; i * i <= NextCheckValue; i++) {
                if (NextCheckValue % i == 0) {
                    IsPrime = false;
                    break;
                }
                auto CurrentTime = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - StartTime).count() > 10) {
                    Divider = i;
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }

            if (IsPrime && ValueToProcess % NextCheckValue == 0) {
                LargestPrime = NextCheckValue;
            }
            IsPrime = true;
            NextCheckValue++;
        }

        Send(WriterActor, std::make_unique<TEvents::TEvWriteValueRequest>(LargestPrime));
        Send(ReaderActor, std::make_unique<TEvents::TEvDone>());
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
    int64_t TotalSum;

public:
    using TBase = NActors::TActor<TWriteActor>;

    TWriteActor() : TBase(&TWriteActor::Handler), TotalSum(0) {}

    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        TotalSum += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << TotalSum << std::endl;
        ProgramContinue->ShouldStop();
        PassAway();
    }
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration &latency)
            : Latency(latency) {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc,
    {
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

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder <NActors::IActor> CreateSelfPingActor(const TDuration &latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr <TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
