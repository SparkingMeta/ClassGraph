#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

class IQueue {
 public:
  virtual ~IQueue() = default;

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual void Clear() = 0;
  virtual bool Empty() const = 0;
  virtual std::size_t Size() const = 0;
};

template <typename T>
class TQueue : public IQueue {
 public:
  void Start() override { 
    std::lock_guard lock(mutex_);
    running_ = true;
  }

  void Stop() override {
    {
      std::lock_guard lock(mutex_);
      running_ = false;
    }
    cv_.notify_all();
  }

  void Push(T item) {
    {
      std::lock_guard lock(mutex_);
      items_.push(std::move(item));
    }
    cv_.notify_one();
  }

  std::optional<T> WaitPop() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return !items_.empty() || !running_; });
    if (items_.empty()) {
      return std::nullopt;
    }
    T item = std::move(items_.front());
    items_.pop();
    return item;
  }

  std::optional<T> TryPop() {
    std::lock_guard lock(mutex_);
    if (items_.empty()) {
      return std::nullopt;
    }
    T item = std::move(items_.front());
    items_.pop();
    return item;
  }

  void Clear() override {
    std::lock_guard lock(mutex_);
    std::queue<T> empty;
    items_.swap(empty);
  }

  bool Empty() const override {
    std::lock_guard lock(mutex_);
    return items_.empty();
  }

  std::size_t Size() const override {
    std::lock_guard lock(mutex_);
    return items_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<T> items_;
  bool running_{false};
};

template <typename T>
std::shared_ptr<TQueue<T>> CreateQueue() {
  return std::make_shared<TQueue<T>>();
}
