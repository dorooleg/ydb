#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {

    struct EEv {
        enum {
            Done = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
            WriteValueRequest
        };
    };

    struct TEvDone: public NActors::TEventLocal<TEvDone, EEv::Done> {
        DEFINE_SIMPLE_LOCAL_EVENT(TEvDone, "EEv::Done");
    };

    struct TEvWriteValueRequest: public NActors::TEventLocal<TEvWriteValueRequest, EEv::WriteValueRequest> {
        int64_t value;

        TEvWriteValueRequest(int64_t value) {
            this->value = value;
        }
    };
};