#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <iostream>
#include <thread> 
#include <memory>
#include <future> 

static auto programShouldContinue = std::make_shared<TProgramShouldContinue>();

class ReadActor : public NActors::TActorBootstrapped<ReadActor> {
private:
  bool flag;
  int64_t count;
  NActors::TActorId wr;

public:
  ReadActor(NActors::TActorId wr) : wr(wr) {
    count = 0;
    flag = false;
  }

  void Bootstrap() {
    Become(&ReadActor::StateFunc);
    Send(SelfId(), new NActors::TEvents::TEvWakeup());
  }

  STFUNC(StateFunc) {
    switch(ev->GetTypeRewrite()) {
        case NActors::TEvents::TEvWakeup::EventType:
            WakeupHandler();
            break;
        case TEvents::TEvDone::EventType:
            DoneHandler();
            break;
        default:
            break;
    }
  }

  void WakeupHandler() {
      int64_t value;
      while (std::cin >> value) {
          Register(CreateDivideActor(value, SelfId(), wr));
          count++;
      }
      
      if (std::cin.eof()) {
          flag = true;
          if (count == 0) {
              Send(wr, new NActors::TEvents::TEvPoisonPill());
          }
      } else {
          Send(SelfId(), new NActors::TEvents::TEvWakeup());
      }
  }

  void DoneHandler() {
    count--;
    if (count == 0 && flag) {
      Send(wr, new NActors::TEvents::TEvPoisonPill());
    }
  }
};

THolder<NActors::IActor> CreateRActor(NActors::TActorId wr) {
    return MakeHolder<ReadActor>(wr);
}

class TMaximumPrimeDivisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDivisorActor> {
private:
  NActors::TActorId Rd;
  NActors::TActorId Wr;
  int64_t Val;
  bool Flag;
  TInstant LastTime;

  const int64_t MAX_TIMEOUT = 10;

public:
  TMaximumPrimeDivisorActor(int64_t Val, NActors::TActorId Rd, NActors::TActorId Wr)
      : Rd(Rd), Wr(Wr), Val(Val) {}

  void Bootstrap() {
    Become(&TMaximumPrimeDivisorActor::StateFunc);
    Send(SelfId(), new NActors::TEvents::TEvWakeup());
  }

STFUNC(StateFunc) {
  switch (ev->GetTypeRewrite()) {
    case NActors::TEvents::TEvWakeup::EventType:
      WakeupHandler();
      break;
    default:
      break;
  }
}

void WakeupHandler() {
  LastTime = TInstant::Now();
  Flag = false;

  int64_t maxDivider = CalculateMaxPrimeDivisor(Val);

  if (Flag) {
    // Timeout
    Send(SelfId(), new NActors::TEvents::TEvWakeup());
  } else {
    Send(Wr, new TEvents::TEvWriteValueRequest(maxDivider));
    Send(Rd, new TEvents::TEvDone());
    PassAway();
  }
}

int64_t CalculateMaxPrimeDivisor(int64_t val) {
  int64_t max_divisor = -1;

  // **Проверка на квадратное число:**
  int64_t sqrt_val = sqrt(val);
  if (sqrt_val * sqrt_val == val) {
    max_divisor = sqrt_val;
    return max_divisor;
  }

  // Проверка на четность
  while (val % 2 == 0) {
    max_divisor = 2;
    val /= 2;

    if (CheckExecutionTime()) {
      Flag = true;
      return 1; 
    }
  }

  for (int64_t i = 3; i * i <= val; ++i) {
    while (val % i == 0) {
      max_divisor = i;
      val /= i;

      if (CheckExecutionTime()) {
        Flag = true;
        return 1; 
      }
    }
  }

  if (val > 2) {
    max_divisor = val;
  }

  return max_divisor;
}

bool CheckExecutionTime() {
  auto now = TInstant::Now();
  return static_cast<int64_t>(now.MilliSeconds() - LastTime.MilliSeconds()) >= MAX_TIMEOUT;
}
};

NActors::IActor* CreateDivideActor(int64_t val, NActors::TActorId rd, NActors::TActorId wr) {
    return new TMaximumPrimeDivisorActor(val, rd, wr);
}

class TWriteActor : public NActors::TActor<TWriteActor> {
private:
  int64_t list;

public:
  TWriteActor() : TActor(&TWriteActor::StateFunc), list(0) {}
    STRICT_STFUNC(StateFunc, {
        hFunc(TEvents::TEvWriteValueRequest, WriteValueRequestHandler);
        cFunc(NActors::TEvents::TEvPoisonPill::EventType, PoisonPillHandler);
    });


  void WriteValueRequestHandler(TEvents::TEvWriteValueRequest::TPtr& ev) {
    list += ev->Get()->value;
  }

  void PoisonPillHandler() {
    std::cout << list << std::endl;

    GetProgramShouldContinue()->ShouldStop();


    PassAway();
  }
};

THolder<NActors::IActor> CreateWActor() {
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
        Send(SelfId(), new NActors::TEvents::TEvWakeup());
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
    return programShouldContinue;
}
