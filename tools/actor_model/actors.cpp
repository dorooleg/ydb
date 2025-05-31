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
    NActors::TActorId WriteActor;
    int ActiveWorkers = 0;
    bool InputFinished = false;

public:
    TReadActor(NActors::TActorId writer)
            : WriteActor(writer)
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
        std::string line;
        if (std::getline(std::cin, line)) {
            // Удаляем пробельные символы в начале и конце
            line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
            line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));

            if (line.empty()) {
                // Пропускаем пустые строки
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }

            // Проверяем что строка содержит только цифры и опциональный минус
            bool isNumber = true;
            size_t startPos = 0;
            if (line[0] == '-') {
                startPos = 1;
                if (line.length() == 1) {
                    isNumber = false; // Только минус
                }
            }

            for (size_t i = startPos; i < line.length(); ++i) {
                if (!isdigit(line[i])) {
                    isNumber = false;
                    break;
                }
            }

            if (!isNumber) {
                // Некорректный ввод - пропускаем строку
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                return;
            }

            try {
                int64_t value = std::stoll(line);
                if (value == 0) {
                    // Особый случай для нуля - отправляем напрямую писателю
                    Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(0));
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                } else {
                    Register(CreateMaximumPrimeDivisorActor(value, SelfId(), WriteActor));
                    ActiveWorkers++;
                    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                }
            } catch (const std::out_of_range&) {
                // Число слишком большое - пропускаем
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
        } else {
            // Конец ввода
            InputFinished = true;
            if (ActiveWorkers == 0) {
                Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            }
        }
    }

    void HandleDone() {
        ActiveWorkers--;
        if (ActiveWorkers == 0 && InputFinished) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
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
    int64_t OriginalNumber;
    int64_t CurrentNumber;
    NActors::TActorId ReadActor;
    NActors::TActorId WriteActor;

    int64_t DivisorCounter = 5;
    TInstant ComputationStart;


    bool TimeLimitExceeded() const {
        return (TInstant::Now() - ComputationStart).MilliSeconds() >= 10;
    }


    int64_t FindMinDivisor() {
        if (CurrentNumber % 2 == 0) return 2;
        if (CurrentNumber % 3 == 0) return 3;


        const int64_t sqrtNum = static_cast<int64_t>(std::sqrt(CurrentNumber));

        while (DivisorCounter <= sqrtNum) {
            if (TimeLimitExceeded()) return -1;

            if (CurrentNumber % DivisorCounter == 0) return DivisorCounter;
            if (CurrentNumber % (DivisorCounter + 2) == 0) return DivisorCounter + 2;

            DivisorCounter += 6;
        }

        return CurrentNumber;
    }


    int64_t ComputeMaxPrimeDivisor() {
        int64_t maxPrimeDivisor = 1;

        while (CurrentNumber > 1) {
            int64_t divisor = FindMinDivisor();
            if (divisor == -1) return -1;

            maxPrimeDivisor = std::max(maxPrimeDivisor, divisor);
            while (CurrentNumber % divisor == 0) {
                CurrentNumber /= divisor;
            }
        }

        return maxPrimeDivisor;
    }

    void HandleWakeup() {
        ComputationStart = TInstant::Now();
        int64_t result = ComputeMaxPrimeDivisor();

        if (result == -1) {
            // Не успели вычислить - продолжаем позже
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            // Отправляем результат и завершаем работу
            Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(result));
            Send(ReadActor, std::make_unique<TEvents::TEvDone>());
            PassAway();
        }
    }

public:
    TMaximumPrimeDivisorActor(int64_t value, NActors::TActorId reader, NActors::TActorId writer)
            : OriginalNumber(value)
            , CurrentNumber(std::abs(value))  // Работаем с модулем числа
            , ReadActor(reader)
            , WriteActor(writer)
    {}

    void Bootstrap() {
        Become(&TThis::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });
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
class TWriteActor: public NActors::TActor<TWriteActor> {

    int64_t Sum = 0;

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, WriteValueRequestHandler);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, PoisonPillHandler);
    });

    void WriteValueRequestHandler(TEvents::TEvWriteValueRequest::TPtr &ev) {
        Sum += ev->Get()->value;
    }

    void PoisonPillHandler() {
        Cout << Sum << Endl;
        GetProgramShouldContinue()->ShouldStop();
        PassAway();
    }

public:
    TWriteActor(): TActor(&TThis::StateFunc) {
        Sum = 0;
    }

};

// Фабричные функции для создания акторов
THolder<NActors::IActor> CreateReadActor(NActors::TActorId writer) {
    return MakeHolder<TReadActor>(writer);
}

NActors::IActor* CreateMaximumPrimeDivisorActor(int64_t number, NActors::TActorId readActor, NActors::TActorId writeActor) {
    return new TMaximumPrimeDivisorActor(number, readActor, writeActor);
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
