#pragma once

#include <ydb/core/base/events.h>
#include <ydb/core/protos/kqp.pb.h>

#include <ydb/library/actors/core/event_pb.h>

namespace NKikimr::NKqp {

struct TKqpLwTraceEvents {
    enum EKqpLwTraceEvents {
        EvKqpLwTrace = EventSpaceBegin(TKikimrEvents::ES_KQP) + 325,
    };
};

struct TEvKqpLwTrace : public TEventPB<TEvKqpLwTrace,
    NKikimrKqp::TEvKqpLwTrace, TKqpLwTraceEvents::EvKqpLwTrace> {};

} // namespace NKikimr::NKqp
