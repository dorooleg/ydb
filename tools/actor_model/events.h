#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
  enum EEv : ui32 {
    EvDone = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
    EvWriteValueRequest,
    EvEnd
  };

  static_assert(EvEnd < EventSpaceEnd(NActors::TEvents::ES_PRIVATE),
                "expect EvEnd < EventSpaceEnd(NActors::TEvents::ES_PRIVATE)");

  struct TEvDone : public NActors::TEventLocal<TEvDone, EvDone> {
    // empty
  };

  struct TEvWriteValueRequest
      : public NActors::TEventLocal<TEvWriteValueRequest, EvWriteValueRequest> {
    i64 Divisor;
    TEvWriteValueRequest(i64 divisor) : Divisor(divisor) {
      // empty
    }
  };
};
