#pragma once

#include <functional>
#include <memory>

#include "export.h"

class MACRO_SHARED_API IWorker {
 public:
  IWorker(const IWorker&) = delete;
  IWorker& operator=(const IWorker&) = delete;
  IWorker(IWorker&&) = delete;
  IWorker& operator=(IWorker&&) = delete;
  virtual ~IWorker();

  virtual void Start() = 0;
  virtual void Stop() = 0;

 protected:
  IWorker() = default;
};

MACRO_SHARED_API std::shared_ptr<IWorker> CreateWorker(std::function<bool()> tick, std::function<void()> interrupt = nullptr);
