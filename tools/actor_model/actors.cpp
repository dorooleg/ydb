#include "actors.h"
#include "events.h" // Подключаем наши события
#include <library/cpp/actors/core/hfunc.h> // Для STRICT_STFUNC и hFunc/cFunc
#include <iostream>     // Для std::cin и std.cout
#include <chrono>       // Для TInstant (хотя в акторах уже есть)

// Этот объект будет управлять завершением программы
static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

// --- TWriteActor ---
class TWriteActor : public NActors::TActor<TWriteActor> {
    long long CurrentSum_ = 0;

public:
    static constexpr char ActorName[] = "TWriteActor";

    TWriteActor() : NActors::TActor<TWriteActor>(&TWriteActor::StateWork) {}

    STRICT_STFUNC(StateWork, {
        hFunc(TEvents::TEvWriteData, HandleWriteData);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void HandleWriteData(TEvents::TEvWriteData::TPtr& ev) {
        CurrentSum_ += ev->Get()->Value;
    }

    void HandlePoisonPill() {
        std::cout << CurrentSum_ << std::endl;
        ShouldContinue->ShouldStop(); // Сигнализируем main, что можно завершаться
        PassAway();                   // Актор завершает свою работу
    }
};

// Фабрика для TWriteActor
THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

// --- TMaximumPrimeDevisorActor ---
class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    const NActors::TActorId ReadActorId_;
    const NActors::TActorId WriteActorId_;
    const int64_t OriginalValue_;

    // Состояние для пошагового вычисления
    int64_t NumberToFactor_;
    int64_t CurrentDivisor_;
    int64_t LargestPrimeFactor_;
    bool IsFirstWakeup_ = true;

    static constexpr TDuration MAX_CALCULATION_SLICE_ = TDuration::MilliSeconds(10);

public:
    static constexpr char ActorName[] = "TMaximumPrimeDevisorActor";

    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId)
        : ReadActorId_(readActorId)
        , WriteActorId_(writeActorId)
        , OriginalValue_(value)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateWork);
        Send(SelfId(), MakeHolder<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateWork, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        if (IsFirstWakeup_) {
            NumberToFactor_ = OriginalValue_;
            LargestPrimeFactor_ = 0;
            CurrentDivisor_ = 2;
            IsFirstWakeup_ = false;

            if (OriginalValue_ <= 1) {
                int64_t value_to_send = 0; // Значение по умолчанию для чисел <= 0
                if (OriginalValue_ == 1) {
                    value_to_send = 1;    // Специальный случай для числа 1
                }
                Send(WriteActorId_, MakeHolder<TEvents::TEvWriteData>(value_to_send));
                Send(ReadActorId_, MakeHolder<TEvents::TEvDone>());
                PassAway();
                return;
            }
        }

        auto startTime = TInstant::Now();
        bool calculationDone = false;

        while (true) { // Внутренний цикл для одной "порции" вычислений
            if (NumberToFactor_ == 1) { // Все разложили
                calculationDone = true;
                break;
            }

            // Оптимизация: если квадрат текущего делителя больше оставшегося числа,
            // то оставшееся число (если оно > 1) и есть наибольший простой делитель.
            if (CurrentDivisor_ * CurrentDivisor_ > NumberToFactor_) {
                if (NumberToFactor_ > 1) {
                    LargestPrimeFactor_ = NumberToFactor_;
                }
                calculationDone = true;
                break;
            }

            if (NumberToFactor_ % CurrentDivisor_ == 0) {
                LargestPrimeFactor_ = CurrentDivisor_;
                while (NumberToFactor_ % CurrentDivisor_ == 0) {
                    NumberToFactor_ /= CurrentDivisor_;
                }
                // Если после деления число стало 1, мы закончили
                if (NumberToFactor_ == 1) {
                    calculationDone = true;
                    break;
                }
            }
            
            // Переходим к следующему предполагаемому делителю
            if (CurrentDivisor_ == 2) {
                CurrentDivisor_ = 3;
            } else {
                CurrentDivisor_ += 2; // Проверяем только нечетные числа после 2
            }

            // Проверяем, не вышло ли время для текущей "порции"
            if (TInstant::Now() - startTime > MAX_CALCULATION_SLICE_) {
                break; // Выходим из внутреннего цикла, calculationDone останется false
            }
        } // Конец while(true) для "порции" вычислений
        
        if (calculationDone) {
            if (LargestPrimeFactor_ == 0 && OriginalValue_ > 1) {
                LargestPrimeFactor_ = OriginalValue_;
            }
            Send(WriteActorId_, MakeHolder<TEvents::TEvWriteData>(LargestPrimeFactor_));
            Send(ReadActorId_, MakeHolder<TEvents::TEvDone>());
            PassAway();
        } else {
            // Время для "порции" истекло, но вычисления не завершены. Отправляем себе Wakeup.
            Send(SelfId(), MakeHolder<NActors::TEvents::TEvWakeup>());
        }
    }
};

// Фабрика для TMaximumPrimeDevisorActor
THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readActorId, NActors::TActorId writeActorId) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readActorId, writeActorId);
}


// --- TReadActor ---
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    const NActors::TActorId WriteActorId_;
    int PendingDevisorActors_ = 0;
    bool EofReached_ = false;

public:
    static constexpr char ActorName[] = "TReadActor";

    TReadActor(NActors::TActorId writeActorId)
        : WriteActorId_(writeActorId)
    {}

    void Bootstrap() {
        Become(&TReadActor::StateRead);
        Send(SelfId(), MakeHolder<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateRead, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        hFunc(TEvents::TEvDone, HandleDone);
    });

    void HandleWakeup() {
        if (EofReached_) { // Если уже достигли конца файла, но проснулись (например, по TEvDone), просто проверяем завершение
            CheckCompletion();
            return;
        }

        int64_t value;
        if (std::cin >> value) {
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriteActorId_).Release());
            PendingDevisorActors_++;
            Send(SelfId(), MakeHolder<NActors::TEvents::TEvWakeup>()); // Запланировать следующее чтение
        } else {
            EofReached_ = true;
            // Если при чтении произошла ошибка или достигнут EOF, и нет ожидающих акторов,
            // то можно сразу попытаться завершить работу.
            CheckCompletion();
        }
    }

    void HandleDone(TEvents::TEvDone::TPtr& /*ev*/) {
        PendingDevisorActors_--;
        Y_VERIFY(PendingDevisorActors_ >= 0, "PendingDevisorActors_ count cannot be negative.");
        CheckCompletion();
    }

    void CheckCompletion() {
        if (EofReached_ && PendingDevisorActors_ == 0) {
            Send(WriteActorId_, MakeHolder<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

// Фабрика для TReadActor
THolder<NActors::IActor> CreateReadActor(NActors::TActorId writeActorId) {
    return MakeHolder<TReadActor>(writeActorId);
}

// --- TSelfPingActor (из примера) ---
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
        // Y_VERIFY(delta <= Latency * 1.1, TStringBuilder() << "Latency too big: " << delta << " > " << Latency); // Добавим небольшой запас
        LastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

// Функция для получения объекта синхронизации завершения
std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
