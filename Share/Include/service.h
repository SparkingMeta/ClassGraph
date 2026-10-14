#pragma once

#include "export.h"
#include "message.h"

// 能力契约（提供方）。
// 语义：我能处理这类请求。不管谁连过来、报文怎么运。
// 正确用法：被 Server::Bind，或同进程直接调用。
// 不是 Client，也不能当成传输端点去 Listen/Connect。
class MACRO_SHARED_API Service {
 public:
  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;
  virtual ~Service();

  // 处理一次请求。需要回包时写入 response；不需要则传 nullptr。
  virtual bool Handle(const Message& request, Message* response) = 0;

 protected:
  Service() = default;
};
