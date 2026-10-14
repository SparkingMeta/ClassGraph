#pragma once

#include "event.h"
#include "export.h"

#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>


template <typename T>
struct Handler {
  std::function<void(const T&)> func;
};

template <typename T>
class TBus;

template <typename Id, typename Msg>
class TBus<TEvent<Id, Msg>> {
 public:
  void Subscribe(Id id, Handler<Msg> handler) {
    std::unique_lock lock(mutex_);
    handlers_[id].push_back(std::move(handler));
  }
  void Publish(const TEvent<Id, Msg>& event) {
    std::vector<Handler<Msg>> handlers;
    {
      std::shared_lock lock(mutex_);
      handlers = handlers_[event.id];
    }
    for (const auto& handler : handlers) {
      if (handler.func) {
        handler.func(event.message);
      }
    }
  }

  private:
  std::shared_mutex mutex_;
  std::unordered_map<EventId, std::vector<Handler<Msg>>> handlers_;
};

template<typename T>
std::shared_ptr<TBus<T>> CreateBus() {
  return std::make_shared<TBus<T>>();
};
