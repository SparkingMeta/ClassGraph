#pragma once

#include <cstddef>
#include <string>
#include <vector>

// 请求/响应信封。Client、Server、Service 只交换这种报文，不共享业务对象。
// act: 动作码（由 Service 解释）；token: 一次调用的关联 ID；data: 载荷。
struct Message {
  int act{};
  int code{};
  std::string token{};
  std::string data{};
};

inline const std::string g_key_status = "status";