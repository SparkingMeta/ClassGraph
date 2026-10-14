#pragma once
#include "Export.h"
#include "message.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cg::model {
enum NodeStatus{
	NS_SILENT,
	NS_ERROR,
	NS_WARN,
	NS_OK,
};

struct BaseMeta {
	std::string name{};
	std::string guid{};
};

struct PinMeta : BaseMeta {
	int kind{};
	int direct{};
	std::string type{};
	std::string value{};
};

struct NodeMeta : BaseMeta {
	double x{};
	double y{};
	NodeStatus status{NS_SILENT};
	std::unordered_map<std::string, std::shared_ptr<PinMeta>> pins{};
};

struct LineMeta : BaseMeta {
	std::string from{};
	std::string to{};
};

struct FlowMeta : BaseMeta {
	std::unordered_map<std::string, std::shared_ptr<NodeMeta>> nodes{};
	std::unordered_map<std::string, std::shared_ptr<LineMeta>> lines{};
};

struct VarMeta : BaseMeta {
	std::string type{};
	std::string value{};
};

struct TypeMeta : BaseMeta {
};

struct FuncMeta : BaseMeta {
	std::string group{};
	std::string key{};
};

struct AssetMeta : BaseMeta {
	std::string path{};
	std::string space{};
	std::unordered_map<std::string, std::shared_ptr<FlowMeta>> flows{};
	std::unordered_map<std::string, std::shared_ptr<VarMeta>> vars{};
};

// 界面命令进来，同步命令出去，引擎结果回来，再通知界面。
class META_API Model {
public:
	Model();
	~Model();

	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;

	void Submit(const Message& msg);
	void Reply(const Message& msg);
	void SetRise(std::function<void(const Message&)> rise);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

}
