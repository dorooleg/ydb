#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
  struct TPrivate {
    enum {
      Start = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
      Done,
      WriteValueRequest,
      End
    };

    static_assert(End < EventSpaceEnd(NActors::TEvents::ES_PRIVATE),
                  "expect End < EventSpaceEnd(ES_PRIVATE)");
  };

  struct TEvDone : public NActors::TEventBase<TEvDone, TPrivate::Done> {
    DEFINE_SIMPLE_LOCAL_EVENT(TEvDone, "Private: TEvDone");
  };

  struct TEvWriteValueRequest
      : public NActors::TEventBase<TEvWriteValueRequest,
                                   TPrivate::WriteValueRequest> {
    DEFINE_SIMPLE_LOCAL_EVENT(TEvWriteValueRequest,
                              "Private: TEvWriteValueRequest");
    TEvWriteValueRequest(int64_t value) : Value(value) {}
    int64_t Value;
  };
};
