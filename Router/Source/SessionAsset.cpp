#include "SessionInternal.h"

namespace CG::Router {
using namespace CG::Core;

void Session::RegisterAsset()
{
	handlers_.Subscribe(Action::LoadAssets, Handler<Message>(std::bind(&Session::ReadAsset, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::CreateBP, Handler<Message>(std::bind(&Session::CreateBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DeleteBP, Handler<Message>(std::bind(&Session::DeleteBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RenameBP, Handler<Message>(std::bind(&Session::RenameBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetSpace, Handler<Message>(std::bind(&Session::SetSpace, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::OpenBP, Handler<Message>(std::bind(&Session::OpenBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::CloseBP, Handler<Message>(std::bind(&Session::CloseBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::GetBlueprint, Handler<Message>(std::bind(&Session::GetBlueprint, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::GetAssets, Handler<Message>(std::bind(&Session::GetAssets, this, std::placeholders::_1)));
}

void Session::CreateBP(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do {
		const auto body = Body(msg);
		std::string space = Trim(JsonStr(body, "space"));
		std::string name = Trim(JsonStr(body, "name"));
		if (space.empty() || name.empty()) {
			desc = "space or name is empty!";
			break;
		}

		if (!IsIdent(space) || !IsIdent(name)) {
			desc = "CreateBP: space/name must be valid C++ identifiers!";
			break;
		}
		
		std::string key = AddAsset(space, name);
		if (key.empty()) {
			desc = "Failed to create asset file!";
			break;
		}

		code = 0;
		desc = std::format("Created asset {}", key);
		data = nlohmann::json({{"guid", key}, {"name", name}, {"space", space}}).dump();
	}while(0);
	
	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::DeleteBP(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		std::string guid = Trim(JsonStr(body, "guid"));
		if (guid.empty()) {
			desc = "DeleteBP: empty guid";
			break;
		}

		if (!DelAsset(guid)) {
			desc = "Failed to delete asset file";
			break;
		}

		if (current_asset_guid_ == guid)
			Clear();

		code = 0;
		desc = "Deleted asset is successful";
		data = nlohmann::json({{"guid", guid}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::UpdateBP(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		std::string space = Trim(JsonStr(body, "space"));
		std::string name = Trim(JsonStr(body, "name"));
		if (space.empty() && name.empty()) {
			desc = "UpdateBP: space or name is valid!";
			break;
		}

		std::string guid = Trim(JsonStr(body, "guid"));
		if (guid.empty()) guid = current_asset_guid_;

		auto asset = FindAsset(guid);
		if (!asset.has_value()) {
			desc = "UpdateBP: asset is not exist!";
			break;
		}

		if (space.empty()) space = asset->space;
		if (name.empty()) name = asset->name;
		if (!IsIdent(space) || !IsIdent(name)) {
			desc = "UpdateBP: space/name must be valid C++ identifiers!";
			break;
		}

		if (space == asset->space && name == asset->name)
			break;

		const std::string new_key = AddAsset(space, name);
		if (new_key.empty()) {
			desc = "Failed to create asset file!";
			break;
		}
		if (!DelAsset(guid)) {
			desc = "Failed to delete old asset file";
			break;
		}

		if (current_asset_guid_ == guid)
			current_asset_guid_ = new_key;
		if (blueprint_) {
			blueprint_->SetSpace(space);
			blueprint_->SetName(name);
		}

		code = 0;
		desc = std::format("Updated asset {}", new_key);
		data = nlohmann::json({{"guid", new_key}, {"name", name}, {"space", space}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::OpenBP(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};

	do
	{
		const auto body = Body(msg);
		std::string key = Trim(JsonStr(body, "guid"));
		if (key.empty()) {
			desc = "OpenBP: empty guid!";
			break;
		}

		auto bp = FindAsset(key);
		if (!bp.has_value()) {
			desc = std::format("OpenBP: {} asset is not exist!", key);
			break;
		}

		blueprint_ = std::make_shared<VBlueprint>();
		blueprint_->SetSpace(bp->space);
		blueprint_->SetName(bp->name);
		current_flow_guid_.clear();
		selected_node_guid_.clear();
		current_asset_guid_ = key;

		const std::string guid = blueprint_->AddFlow("EventGraph");
		if (auto flow = blueprint_->FindFlow(guid)) {
			MakeEntry(flow);
			current_flow_guid_ = guid;
		}

		code = 0;
		desc = std::format("Opened Blueprint {}", key);
		data = nlohmann::json({{"guid", key}, {"name", bp->name}, {"space", bp->space}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::ReadAsset(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		data = MakeAssets();
		code = 0;
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::CloseBP(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		Clear();
		code = 0;
		desc = "Blueprint closed";
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::RenameBP(const Message& msg)
{
	UpdateBP(msg);
}

void Session::SetSpace(const Message& msg)
{
	UpdateBP(msg);
}


void Session::GetBlueprint(const Message& msg)
{
	Reply(msg.act, msg.token, ToJson(MakeBp()).dump());
}

void Session::GetAssets(const Message& msg)
{
	Reply(msg.act, msg.token, MakeAssets());
}

void Session::LoadAssets()
{
	do {
		std::unordered_map<std::string, AssetItem> assets;
		std::string asset_root = config_.asset_root;
		if (asset_root.empty()) break;

		std::error_code ec;
		if (!fs::exists(asset_root, ec)) break;

		for (auto it = fs::recursive_directory_iterator(asset_root, ec);
			it != fs::recursive_directory_iterator(); ++it) {
			if (ec)
				break;
			if (!it->is_regular_file(ec))
				continue;
			const auto& path = it->path();
			if (path.extension() != ".vbp")
				continue;
			std::string space;
			std::string name;
			if (!ReadAssetFile(path.string(), space, name))
				continue;
			std::string key = std::format("{}:{}", space, name);
			assets.insert({key, {path.string(), space, name, key}});
		}
	
		assets_ = assets;
	}while(0);
}

void Session::MakeAssetDir() const
{
	if (config_.asset_root.empty())
		return;
	std::error_code ec;
	fs::create_directories(config_.asset_root, ec);
}

std::string Session::MakeAssets() const
{
	const auto assets = assets_;
	nlohmann::json j;
	j["asset_root"] = config_.asset_root;
	j["items"] = nlohmann::json::array();
	for (const auto& [key, item] : assets) {
		j["items"].push_back({
			{"path", item.path},
			{"space", item.space},
			{"name", item.name},
			{"key", key},
			});
	}
	return j.dump();
}

std::optional<AssetItem> Session::FindAsset(const std::string& key) const
{
	auto it = assets_.find(key);
	if(it == assets_.end()) return{};
	else return it->second;
}

std::string Session::AssetPath(const std::string& space, const std::string& name) const
{
	fs::path root = config_.asset_root.empty() ? fs::path("Content/Blueprints") : fs::path(config_.asset_root);
	return (root / space / (name + ".vbp")).string();
}

std::string Session::AddAsset(const std::string& space, const std::string& name)
{
	std::string ret{};

	do {
		const std::string path = AssetPath(space, name);
		if (fs::exists(path)) break;

		std::string key = std::format("{}:{}",space,name);
		if(auto it = assets_.find(key); it != assets_.end())
			break;

		std::error_code ec;
		fs::create_directories(fs::path(path).parent_path(), ec);
		if(ec) break;

		nlohmann::json j{ {"key", key}, {"space", space}, {"name", name}};
		if(!WriteFile(path, j.dump())) break;
		
		assets_.insert({key,{path, space, name, key}});

		ret = key;
	}while(0);

	return ret;
}

bool Session::DelAsset(const std::string& key)
{
	bool ret = false;

	do {
		auto it = assets_.find(key);
		if (it == assets_.end()) {
			ret = true;
			break;
		}

		std::string path = it->second.path;
		std::error_code ec;
		if (fs::exists(path)) fs::remove(path, ec);
		if(ec) break;

		assets_.erase(it);

		ret = true;
	}while(0);

	return ret;
}

bool Session::ReadAssetFile(const std::string& path, std::string& space, std::string& name) const
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	try {
		const auto j = nlohmann::json::parse(ss.str());
		space = j.value("space", "");
		name = j.value("name", "");
		if (space.empty() || name.empty()) {
			const fs::path p(path);
			if (name.empty())
				name = p.stem().string();
			if (space.empty() && p.has_parent_path())
				space = p.parent_path().filename().string();
		}
		return !space.empty() && !name.empty();
	} catch (...) {
		return false;
	}
}

Blueprint Session::MakeBp() const
{
	Blueprint dto;
	if (!blueprint_)
		return dto;
	dto.guid = blueprint_->GetGuid();
	dto.space = blueprint_->GetSpace();
	dto.name = blueprint_->GetName();
	dto.current_flow_guid = current_flow_guid_;
	dto.self_type_name = SelfType();
	if (auto flow = blueprint_->FindFlow(current_flow_guid_))
		dto.current_flow_name = flow->GetName();

	for (const auto& [guid, flow] : blueprint_->GetFlows()) {
		if (!flow)
			continue;
		FlowItem item;
		item.guid = guid;
		item.name = flow->GetName();
		dto.flows.push_back(std::move(item));
	}
	for (const auto& [guid, var] : blueprint_->GetVars()) {
		if (!var)
			continue;
		VarItem item;
		item.guid = guid;
		item.name = var->GetName();
		item.type_name = var->GetType() ? var->GetType()->name_ : "?";
		dto.vars.push_back(std::move(item));
	}
	const std::string self = dto.self_type_name;
	for (const auto* t : VSystem::Get().GetTypes()) {
		if (!t || t->name_.empty())
			continue;
		if (!self.empty() && t->name_ == self)
			continue;
		dto.types.push_back(TypeItem{ t->name_ });
	}
	return dto;
}


} // namespace CG::Router
