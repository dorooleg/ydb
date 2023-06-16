#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <fstream>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();


/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor(ok)
*/

// ok
// Класс TReadActor, чтение ввода с консоли. (v2)(ok)
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {

    bool Finish = false;
    const NActors::TActorId WriteActor;
    int Count;
    // Флаг первого запуска.
    bool FirstEnter;
public:
    TReadActor(const NActors::TActorId writeActor)
        : Finish(false), WriteActor(writeActor), Count(0), FirstEnter(true)
    {}

    void Bootstrap() {

        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    // Функция-обработчик состояния актора.
    STRICT_STFUNC(StateFunc, {

        // Обработчики
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    // Обработчик события "пробуждение".
    void HandleWakeUp() {
        int64_t value;
        if (std::cin >> value) {

            FirstEnter = false;
            // Регистрируем актор TMaximumPrimeDevisorActor.
            Register(CreateTMaximumPrimeDevisorActor(value, SelfId(), WriteActor).Release());
            Count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {

            if (FirstEnter) {

               // Регистрируем новый актор TMaximumPrimeDevisorActor (c 0)
               Register(CreateTMaximumPrimeDevisorActor(0, SelfId(), WriteActor).Release());

               Count++;
               // Cообщение себе.
               Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }

            Finish = true;
        }
    }

    // Обработчик события "завершение".
    void HandleDone() {

       Count--;
       // Если работа всех акторов завершена, то отправляем сообщение актору-писателю о завершении работы.
       if (Finish && Count == 0) {

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
// ok
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {

    int64_t Value;
    const NActors::TActorIdentity ReadActor;
    const NActors::TActorId WriteActor;
    int64_t Prime;
    int64_t CurrentStateFirst;
    int64_t CurrentStateSecond;
    bool flag;

public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor), CurrentStateFirst(1), Prime(0), flag(true), CurrentStateSecond(2)
    {}

    void Bootstrap() {

        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {

        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    void HandleWakeUp() {

    	auto startTime = std::chrono::steady_clock::now();
    	auto endTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        for (int64_t i = CurrentStateFirst; i <= Value; i++) {
            for (int64_t j = CurrentStateSecond; j * j <= i; j++) {
                if (i % j == 0) {
                    flag = false;
		            break;
		        }
		        endTime = std::chrono::steady_clock::now();
            	elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            	if (elapsedTime > 10) {
            	    CurrentStateFirst = i;
            	    CurrentStateSecond = j;
            	    Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
                    return;
                }
            }
            if (flag && Value % i == 0) {
                Prime = i;
            } else {
            	flag = true;
            }
        }
        Send(WriteActor, std::make_unique<TEvents::TEvWriteValueRequest>(Prime));
        Send(ReadActor, std::make_unique<TEvents::TEvDone>());
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
// ok
class TWriteActor : public NActors::TActor<TWriteActor> {
    int Sum;
public:
    // Использование TActor в качестве базового класса,
    // определение конструктора, который вызывает Handler и инициализирует Sum значением 0
    using TBase = NActors::TActor<TWriteActor>;
    TWriteActor() : TBase(&TWriteActor::Handler), Sum(0) {}

    // Описание функции-обработчика Handler с использованием STRICT_STFUNC,
    // которая обрабатывает события TEvWriteValueRequest и TEvPoisonPill::EventType
    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });

    // Обработчик события TEvWriteValueRequest, который увеличивает Sum на значение event.Value
    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        auto& event = *ev->Get();
        Sum = Sum + event.Value;
    }

    // Функция HandleDone, которая выводит значение Sum в консоль,
    // останавливает поток ShouldContinue и завершает работу актора
    void HandleDone() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};
///////////TSelfPingActor(base)
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
///////////
THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);//(ok)
}

THolder<NActors::IActor> CreateTReadActor(const NActors::TActorId writeActor) {
    return MakeHolder<TReadActor>(writeActor);
}

THolder<NActors::IActor> CreateTMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActor, writeActor);
}

THolder<NActors::IActor> CreateTWriteActor() {
    return MakeHolder<TWriteActor>();
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
    }
