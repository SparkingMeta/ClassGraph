#pragma once

#include "base.h"
#include "message.h"

#include <type_traits>

enum class EventId : uint32 { 
    EI_Recv, 
    EI_Send,
    EI_Rise,
    EI_Sink,
};

template <typename Id, typename Msg>
struct TEvent {
  Id id;
  Msg message;
};

using Event = TEvent<EventId, Message>;

template <>
struct std::hash<EventId> {
  std::size_t operator()(EventId id) const noexcept {
    return static_cast<std::size_t>(id);
  }
};
