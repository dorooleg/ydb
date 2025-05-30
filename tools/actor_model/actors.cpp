#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <library/cpp/actors/core/executor_pool_basic.h>
#include <library/cpp/actors/core/scheduler_basic.h>
#include <util/generic/xrange.h>

static auto ProgramContinueFlag = std::make_shared<TProgramShouldContinue>();

class TMaxPrimeDivisorCalculator : public NActors::TActorBootstrapped<TMaxPrimeDivisorCalculator> {
public:
    TMaxPrimeDivisorCalculator(int64_t inputNumber, NActors::TActorId readerId, NActors::TActorId writerId)
        : NumberToProcess(inputNumber)
       , ReaderActorId(readerId)
       , WriterActorId(writerId)
       , ComputationStartTime(TInstant::Now())
    {}

    void Bootstrap() {
        Become(&TMaxPrimeDivisorCalculator::WorkState);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(WorkState, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, ProcessCalculation);
    });

    void ProcessCalculation() {
        int64_t largestDivisor = FindLargestPrimeDivisor(NumberToProcess);
        TDuration timeElapsed = TInstant::Now() - ComputationStartTime;
        
        if (timeElapsed > TDuration::MilliSeconds(10)) {
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            Send(WriterActorId, std::make_unique<TEvents::TWriteResultEvent>(largestDivisor));
            Send(ReaderActorId, std::make_unique<TEvents::TComputationDone>());
            PassAway();
        }
    }

private:
    int64_t NumberToProcess;
    NActors::TActorId ReaderActorId;
    NActors::TActorId WriterActorId;
    TInstant ComputationStartTime;

    int64_t FindLargestPrimeDivisor(int64_t num) {
        if (num == 1) return 1;
        
        int64_t maxPrime = 0;
        while (num % 2 == 0) {
            num /= 2;
            maxPrime = 2;
        }
        
        for (int64_t i = 3; i <= std::sqrt(num); i += 2) {
            while (num % i == 0) {
                maxPrime = i;
                num /= i;
            }
        }
        
        if (num > 1) maxPrime = num;
        return maxPrime;
    }
};

class TInputReader : public NActors::TActorBootstrapped<TInputReader> {
public:
    TInputReader(std::istream& inputStream, NActors::TActorId outputActor)
        : InputStream(inputStream), OutputActorId(outputActor), ActiveComputations(0), ReadingFinished(false)
    {
        int nextChar = InputStream.peek();
        EmptyInput = (nextChar == EOF);
    }

    void Bootstrap() {
        Become(&TInputReader::OperationalState);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(OperationalState, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, ReadNextValue); 
        cFunc(TEvents::TComputationDone::EventType, HandleCompletion);
    });

    void ReadNextValue() {
        int64_t currentValue;
        if (InputStream >> currentValue) {
            auto calculator = MakeHolder<TMaxPrimeDivisorCalculator>(currentValue, SelfId(), OutputActorId);
            Register(calculator.Release());
            ActiveComputations++;
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
        } else {
            ReadingFinished = true;
            CheckCompletion();
        }
    }
    
    void HandleCompletion() {
        ActiveComputations--;
        CheckCompletion();
    }

private:
    std::istream& InputStream;
    NActors::TActorId OutputActorId;
    int ActiveComputations;
    bool ReadingFinished;
    bool EmptyInput = false;

    void CheckCompletion() {
        if (EmptyInput || (ReadingFinished && ActiveComputations == 0)) {
            Send(OutputActorId, std::make_unique<NActors::TEvents::TEvPoisonPill>());
        }
    }
};

THolder<NActors::IActor> CreateInputReader(std::istream& inputStream, NActors::TActorId outputActor) {
    return MakeHolder<TInputReader>(inputStream, outputActor);
}

class TOutputWriter : public NActors::TActor<TOutputWriter> {
public:
    TOutputWriter() : TActor(&TOutputWriter::ProcessingState), TotalSum(0) {}

    STRICT_STFUNC(ProcessingState, {
        hFunc(TEvents::TWriteResultEvent, HandleResult); 
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, Finalize);
    });

    void HandleResult(TEvents::TWriteResultEvent::TPtr& event) {
        TotalSum += event->Get()->ResultValue;
    }

    void Finalize() {
        Cout << TotalSum << Endl;
        GetContinueFlag()->ShouldStop();
        PassAway();
    }

private:
    int64_t TotalSum;
};

THolder<NActors::IActor> CreateOutputWriter() {
    return MakeHolder<TOutputWriter>();
}

class THeartbeatActor : public NActors::TActorBootstrapped<THeartbeatActor> {
    TDuration ExpectedInterval;
    TInstant PreviousBeatTime;

public:
    THeartbeatActor(const TDuration& interval)
        : ExpectedInterval(interval)
    {}

    void Bootstrap() {
        PreviousBeatTime = TInstant::Now();
        Become(&THeartbeatActor::ActiveState);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(ActiveState, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, SendNextBeat);
    });

    void SendNextBeat() {
        auto currentTime = TInstant::Now();
        TDuration actualInterval = currentTime - PreviousBeatTime;
        Y_VERIFY(actualInterval <= ExpectedInterval, "Interval exceeded expected value");
        PreviousBeatTime = currentTime;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateHeartbeatActor(const TDuration& interval) {
    return MakeHolder<THeartbeatActor>(interval);
}

std::shared_ptr<TProgramShouldContinue> GetContinueFlag() {
    return ProgramContinueFlag;
}
