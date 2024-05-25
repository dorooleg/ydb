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

// TODO: напишите реализацию TReadActor

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    const NActors::TActorId WriterId;
    bool createFlag;
    int64_t MaxActors;

    public:
        TReadActor(const NActors::TActorId writerId) : WriterId(writerId), createFlag(false), MaxActors(0) {
        }

        void Bootstrap() {
            Become(&TReadActor::StateFunc);
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            cFunc(NActors::TEvents::TEvWakeup::EventType, WakeupHandler);
            cFunc(TEvents::TEvDone::EventType, DoneHandler);
        default:
            break;
        }
    }

    void WakeupHandler() {
        int64_t value;

        if (std::cin >> value) {
            Register(CreateMaximumPrimeDivisorActor(value, SelfId(), WriterId));
            MaxActors++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
        else {
            createFlag = true;

            if (MaxActors == 0) {
                Send(WriterId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            }
        }
    }

    bool isFinished(int MaxActors, bool createFlag) {
        return (MaxActors == 0 && createFlag);
    }

    void DoneHandler() {
        MaxActors--;

        if (isFinished(MaxActors, createFlag)) {
            Send(WriterId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

THolder<NActors::IActor> CreateReadActor(NActors::TActorId writerId) {
    return MakeHolder<TReadActor>(writerId);
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

// TODO: напишите реализацию TMaximumPrimeDevisorActor

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {

    NActors::TActorId ReaderId;
    NActors::TActorId WriterId;
    int64_t Value;

    public:
        TMaximumPrimeDivisorActor(int64_t value, NActors::TActorId readerId, NActors::TActorId writerId) : ReaderId(readerId), WriterId(writerId) {
            Value = value;
        }

        void Bootstrap() {
            Become(&TMaximumPrimeDivisorActor::StateFunc);
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }

    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            cFunc(NActors::TEvents::TEvWakeup::EventType, WakeupHandler);

        default:
            break;
        }
    }

    void WakeupHandler() {
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = std::chrono::steady_clock::now();

        int64_t result = largestPrimeDivisor(Value, startTime, endTime);

        if (checkElapsedTime(startTime, endTime))
        {
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        }
        else {
            Send(WriterId, std::make_unique<TEvents::TEvWriteValueRequest>(result));
            Send(ReaderId, std::make_unique<TEvents::TEvDone>());
            PassAway();
        }
    }

    bool isPrime(int n) {
        if (n <= 1) return false;
        if (n <= 3) return true;

        if (n % 2 == 0 || n % 3 == 0) return false;

        for (int i = 5; i * i <= n; i += 6) {
            if (n % i == 0 || n % (i + 2) == 0)
                return false;
        }

        return true;
    }

    int64_t largestPrimeDivisor(int64_t n, auto startTime, auto endTime) {

        if (Value == 1) return 1;
        if (Value % 2 == 0) return 2;
        if (Value % 3 == 0) return 3;
        int64_t largestPrime = -1;

        while (n % 2 == 0) {
            if (checkElapsedTime(startTime, endTime))
            {
                return -1;
            }

            largestPrime = 2;
            n /= 2;
        }

        for (int i = 3; i <= sqrt(n); i += 2) {
            while (n % i == 0) {
                if (checkElapsedTime(startTime, endTime))
                {
                    return -1;
                }

                if (isPrime(i))
                    largestPrime = i;
                n /= i;
            }
        }

        if (n > 2 && isPrime(n))
            largestPrime = n;

        return largestPrime;
    }

    bool checkElapsedTime(std::chrono::steady_clock::time_point startTime, std::chrono::steady_clock::time_point endTime) {
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        if (elapsedTime > 10) {
            return true;
        }
        return false;
    }
};

NActors::IActor* CreateMaximumPrimeDivisorActor(int64_t value, NActors::TActorId readerId, NActors::TActorId writerId) {
    return new TMaximumPrimeDivisorActor(value, readerId, writerId);
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
    int64_t sum;


    public:
        TWriteActor() : TActor(&TThis::StateFunc) {
            sum = 0;
        }

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, WriteValueRequestAction);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, EndHandler);
        });

    void WriteValueRequestAction(TEvents::TEvWriteValueRequest::TPtr& ev) {
        sum += ev->Get()->value;
    }

    void EndHandler() {
        Cout << sum << Endl;
        GetProgramShouldContinue()->ShouldStop();
        PassAway();
    }
};

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
