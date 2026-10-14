#pragma once
#include "Export.h"
#include "message.h"

#include <functional>
#include <memory>
#include <string>

namespace cg::engine {

// 同步命令进来，落到 Core 真实对象上，结果写回同一条报文。
class ENGINE_API Engine {
public:
	Engine();
	~Engine();

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	void Apply(const Message& msg);
	void SetRoot(const std::string& root);
	void SetTypeDir(const std::string& dir);
	void LoadTypes();
	void SetRise(std::function<void(const Message&)> rise);
private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

}
