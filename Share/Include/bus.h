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
    handlers_.insert({id, handler});
  }
  void Publish(const TEvent<Id, Msg>& event) {
    Handler<Msg> handler;
    {
      std::shared_lock lock(mutex_);
      if(auto it = handlers_.find(event.id); it != handlers_.end())
        handler = it->second;
    }
    if (handler.func) handler.func(event.message);
      
  }

  private:
  std::shared_mutex mutex_;
  std::unordered_map<Id, Handler<Msg>> handlers_;
};

template<typename T>
std::shared_ptr<TBus<T>> CreateBus() {
  return std::make_shared<TBus<T>>();
};
