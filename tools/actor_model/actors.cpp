#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();
class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
    bool Finish;                    // Indicates if reading is finished
    const NActors::TActorId WriteActor;  // Actor ID of the writer actor
    int Count;                      // Counter for active divisor actors
    bool FirstEnter;                // Flag for the first entry

public:
    // Constructor to initialize member variables
    explicit TReadActor(const NActors::TActorId writeActor)
        : Finish(false), WriteActor(writeActor), Count(0), FirstEnter(true) {}

    // Bootstrap function to initialize actor's state
    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    // State function to handle incoming events
    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
        cFunc(TEvents::TEvDone::EventType, HandleDone);
    });

    // Handler for wakeup event
    void HandleWakeUp() {
        int64_t value;
        if (std::cin >> value) {
            FirstEnter = false;	
            Register(CreateTMaximumPrimeDevisorActor(SelfId(), WriteActor).Release(), value);
            Count++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            if (FirstEnter) {
                Register(CreateTMaximumPrimeDevisorActor(SelfId(), WriteActor).Release(), 0);
                Count++;
                Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            }
            Finish = true;
        }
    }

    // Handler for devisor completion event
    void HandleDone() {
        Count--;
        if (Finish && Count == 0) {
            Send(WriteActor, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
    int64_t Value;                  // Value to find the maximum prime divisor
    const NActors::TActorIdentity ReadActor; // Actor identity of the read actor
    const NActors::TActorId WriteActor;      // Actor ID of the write actor
    int64_t Prime;                  // Maximum prime divisor
    int64_t CurrentStateFirst;      // Current state for the outer loop
    int64_t CurrentStateSecond;     // Current state for the inner loop
    bool flag;                      // Flag to check prime status

public:
    // Constructor to initialize member variables
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorIdentity readActor, const NActors::TActorId writeActor)
        : Value(value), ReadActor(readActor), WriteActor(writeActor), Prime(0), CurrentStateFirst(1), CurrentStateSecond(2), flag(true) {}

    // Bootstrap function to initialize actor's state
    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    // State function to handle incoming events
    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeUp);
    });

    // Handler for wakeup event
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

class TWriteActor : public NActors::TActor<TWriteActor> {
    int Sum; // Sum of the maximum prime divisors

public:
    using TBase = NActors::TActor<TWriteActor>;

    // Constructor to initialize the base class and member variables
    TWriteActor() : TBase(&TWriteActor::Handler), Sum(0) {}

    // State function to handle incoming events
    STRICT_STFUNC(Handler, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandleDone);
    });

    // Handler for write value request event
    void Handle(TEvents::TEvWriteValueRequest::TPtr& ev) {
        auto& event = *ev->Get();
        Sum += event.Value;
    }

    // Handler for poison pill event
    void HandleDone() {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};

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
