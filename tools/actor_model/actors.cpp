#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

class TMaximumPrimeDevisorActor;

class TReadActor : public NActors::TActorBootstrapped<TReadActor>
{
    bool IsFifnished = false;
    size_t MacCountPrimeDivision = 0;
    const NActors::TActorId WriterActorId;

public:
    TReadActor(NActors::TActorId writerActorId)
        : WriterActorId(writerActorId)
    {
    }

    void Bootstrap()
    {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
    }

private:
    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
        cFunc(TEvents::EvDone, HandleDone);
    });

    void HandleWakeup()
    {
        int64_t value;
        if (std::cin >> value)
        {
            MacCountPrimeDivision++;
            Register(CreateMaximumPrimeDevisorActor(value, SelfId(), WriterActorId).Release());
            Send(SelfId(), new NActors::TEvents::TEvWakeup());
        }
        else
        {
            IsFifnished = true;
            if (MacCountPrimeDivision == 0)
            {
                Finish();
            }
        }
    }

    void HandleDone()
    {
        MacCountPrimeDivision--;
        if (IsFifnished && MacCountPrimeDivision == 0)
        {
            Finish();
        }
    }

    void Finish()
    {
        Send(WriterActorId, new NActors::TEvents::TEvPoisonPill());
        PassAway();
    }
};

class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor>
{
    static constexpr double TIMEOUT_MS = 10.0;
    const int64_t Value;
    const NActors::TActorId WriterActorId;
    const NActors::TActorId ReaderActorId;

public:
    TMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readerActorId, NActors::TActorId writerActorId)
        : Value(value), ReaderActorId(readerActorId), WriterActorId(writerActorId)
    {
    }

    void Bootstrap()
    {
        Calculate();
    }

private:
    void Calculate()
    {
        StartTime = std::chrono::high_resolution_clock::now();
        int64_t largestPrime = GetMaxPrimeDivisor(Value, StartTime);
        Send(WriterActorId, new TEvents::TEvWriteValueRequest(largestPrime));
        Send(ReaderActorId, new TEvents::TEvDone());
        PassAway();
    }
    int64_t GetMaxPrimeDivisor(int64_t number, std::chrono::high_resolution_clock::time_point startTime)
    {
        if (number <= 1)
            return 1;

        int64_t result = 1;
        auto DeadlineExceeded = [&]() -> bool
        {
            auto now = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() > TIMEOUT_MS;
        };

        if ((number & 1) == 0)
        {
            result = 2;
            while ((number & 1) == 0)
            {
                if (DeadlineExceeded())
                    return -1;

                number >>= 1;
            }
        }

        for (int64_t div = 3; div * div <= number; div += 2)
        {
            if (DeadlineExceeded())
                return -1;

            if (number % div == 0)
            {
                result = div;
                while (number % div == 0)
                {
                    if (DeadlineExceeded())                
                        return -1;
                    number /= div;
                }
            }
        }

        if (number > 1)
            result = number;
        return result;
    }
};

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, NActors::TActorId readerActorId, NActors::TActorId writerActorId)
{
    return MakeHolder<TMaximumPrimeDevisorActor>(value, readerActorId, writerActorId);
}

class TWriteActor : public NActors::TActor<TWriteActor>
{
    int64_t Sum = 0;

public:
    TWriteActor() : TActor(&TWriteActor::StateFunc) {}

    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, Handle);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, HandlePoisonPill);
    });

    void Handle(TEvents::TEvWriteValueRequest::TPtr &ev)
    {
        Sum += ev->Get()->Value;
    }

    void HandlePoisonPill()
    {
        std::cout << Sum << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }
};
THolder<NActors::IActor> CreateReadActor(NActors::TActorId writerActorId)
{
    return MakeHolder<TReadActor>(writerActorId);
}

THolder<NActors::IActor> CreateWriteActor()
{
    return MakeHolder<TWriteActor>();
}

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
