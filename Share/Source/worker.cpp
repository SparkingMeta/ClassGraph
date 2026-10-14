#include "worker.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>

IWorker::~IWorker() = default;

class CWorker final : public IWorker {
 public:
  explicit CWorker(std::function<bool()> tick,std::function<void()> interrupt = nullptr)
      : tick_(std::move(tick)), interrupt_(std::move(interrupt)) {}

  ~CWorker() override { Stop(); }

  void Start() override {
    std::lock_guard lock(mutex_);
    if (thread_.joinable()) {
      return;
    }
    running_.store(true);
    try {
      thread_ = std::thread([this] { Loop(); });
    } catch (...) {
      running_.store(false);
      throw;
    }
  }

  void Stop() override {
    running_.store(false);
    if (interrupt_) {
      interrupt_();
    }
    std::thread joining;
    {
      std::lock_guard lock(mutex_);
      if (thread_.joinable()) {
        joining.swap(thread_);
      }
    }
    if (joining.joinable()) {
      joining.join();
    }
  }

 private:
  void Loop() {
    while (running_.load()) {
      if (!tick_ || !tick_()) {
        break;
      }
    }
    running_.store(false);
  }

  std::function<bool()> tick_;
  std::function<void()> interrupt_;
  std::atomic<bool> running_{false};
  std::mutex mutex_;
  std::thread thread_;
};

std::shared_ptr<IWorker> CreateWorker(std::function<bool()> tick,std::function<void()> interrupt) {
  return std::make_shared<CWorker>(std::move(tick), std::move(interrupt));
}
