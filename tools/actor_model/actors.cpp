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

// Актор для чтения чисел из входного потока
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
private:
    IInputStream& Strm; // Входной поток
    const NActors::TActorId WriteActor; // ID актора-писателя
    size_t ActiveWorkers = 0; // Количество активных вычислителей
    bool InputFinished = false; // Флаг завершения ввода

public:
    TReadActor(IInputStream& strm, const NActors::TActorId writeActor)
            : Strm(strm)
            , WriteActor(writeActor)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        // Запускаем процесс чтения
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    // Обработка чтения числа
    void HandleWakeup() {
        int64_t value;
        if (!Strm.Read(&value, sizeof(value))) { // Правильное чтение
            InputFinished = true;
            if (ActiveWorkers == 0) {
                Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            }
            return;
        }

        auto maxPrimeDivActor = Register(CreateMaximumPrimeDivisorActor(value, SelfId(), WriteActor).Release());
        ActiveWorkers++;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    // Обработка завершения вычислений
    void HandleDone(TEvents::TEvDone::TPtr&) {
        ActiveWorkers--;
        // Если ввод завершен и все вычислители закончили
        if (InputFinished && ActiveWorkers == 0) {
            // Завершаем писателя
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
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

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
private:
    int64_t Number; // Число для обработки
    const NActors::TActorId ReadActor; // ID актора-читателя
    const NActors::TActorId WriteActor; // ID актора-писателя
    int64_t CurrentDivisor = 2; // Текущий делитель
    int64_t MaxPrimeDivisor = 1; // Максимальный простой делитель

public:
    TMaximumPrimeDivisorActor(int64_t number, const NActors::TActorId readActor, const NActors::TActorId writeActor)
            : Number(number)
            , ReadActor(readActor)
            , WriteActor(writeActor)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDivisorActor::StateFunc);
        // Начинаем вычисления
        Send(SelfId(), std::make_unique<TEvents::TEvCompute>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(TEvents::EvCompute, HandleCompute);
    });

    // Обработка вычислений
    void HandleCompute() {
        auto startTime = TInstant::Now();
        int64_t n = Number;

        // Поиск максимального простого делителя
        while (n > 1 && CurrentDivisor * CurrentDivisor <= n) {
            // Проверяем, не превысили ли лимит времени
            if (TInstant::Now() - startTime > TDuration::MilliSeconds(10)) {
                // Продолжаем вычисления позже
                Send(SelfId(), std::make_unique<TEvents::TEvCompute>());
                return;
            }

            // Проверяем делимость
            if (n % CurrentDivisor == 0) {
                MaxPrimeDivisor = CurrentDivisor;
                // Убираем все вхождения этого делителя
                while (n % CurrentDivisor == 0) {
                    n /= CurrentDivisor;
                }
            }

            // Переходим к следующему делителю
            if (CurrentDivisor == 2) {
                CurrentDivisor++;
            } else {
                CurrentDivisor += 2;
            }
        }

        // Если остаток больше 1, он сам простой
        if (n > 1) {
            MaxPrimeDivisor = n;
        }

        // Отправляем результат писателю
        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(MaxPrimeDivisor));
        // Уведомляем читателя о завершении
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
        // Завершаем работу
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

// Актор для записи и суммирования результатов
class TWriteActor : public NActors::TActor<TWriteActor> {
private:
    int64_t Sum = 0; // Накопленная сумма

public:
    // Явный конструктор
    explicit TWriteActor()
            : NActors::TActor<TWriteActor>(&TWriteActor::StateFunc)
    {}


    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, HandleWriteValue);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    // Обработка нового значения
    void HandleWriteValue(TEvents::TEvWriteValueRequest::TPtr& ev) {
        Sum += ev->Get()->Value;
    }

    // Обработка команды завершения
    void HandlePoisonPill() {
        // Выводим результат
        Cout << Sum << Endl;
        // Устанавливаем флаг завершения
        ShouldContinue->ShouldStop(0);
        // Завершаем работу
        PassAway();
    }
};

// Фабричные функции для создания акторов
THolder<NActors::IActor> CreateReadActor(IInputStream& strm, const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(strm, writeActor);
}

THolder<NActors::IActor> CreateMaximumPrimeDivisorActor(int64_t number, const NActors::TActorId readActor, const NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDivisorActor>(number, readActor, writeActor);
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
