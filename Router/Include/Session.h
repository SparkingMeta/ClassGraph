#pragma once

#include "VApi.h"
#include "VDto.h"
#include "IRouter.h"
#include "VMsg.h"
#include "bus.h"
#include "service.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace CG::Core {
	class VBlueprint;
	class VNode;
	class AFlow;
	class AGraph;
	class APin;
}

namespace CG::Router {

	using CG::Core::VBlueprint;
	using CG::Core::VNode;
	using CG::Core::AFlow;
	using CG::Core::AGraph;
	using CG::Core::APin;

	class ROUTER_API Session : public Service {
	public:
		explicit Session(HostConfig config);
		~Session() override;

		void Init();
		void SetRise(std::function<void(const Message&)> rise);
		bool Handle(const Message& request, Message* response) override;

	private:
		void RegisterAction();
		void RegisterAsset();
		void RegisterFlow();
		void RegisterVar();
		void RegisterGraph();
		void RegisterType();

		void CreateBP(const Message& msg);
		void DeleteBP(const Message& msg);
		void UpdateBP(const Message& msg);
		void RenameBP(const Message& msg);
		void SetSpace(const Message& msg);
		void OpenBP(const Message& msg);
		void ReadAsset(const Message& msg);
		void CloseBP(const Message& msg);
		void GetBlueprint(const Message& msg);
		void GetAssets(const Message& msg);

		void AddFlow(const Message& msg);
		void DelFlow(const Message& msg);
		void RenameFlow(const Message& msg);
		void SelectFlow(const Message& msg);
		void RunFlow(const Message& msg);
		void GetGraph(const Message& msg);

		void AddVar(const Message& msg);
		void DelVar(const Message& msg);
		void RenameVar(const Message& msg);
		void SetVarType(const Message& msg);
		void SetVarValue(const Message& msg);
		void SelectVar(const Message& msg);
		void PlaceGet(const Message& msg);
		void PlaceSet(const Message& msg);

		void AddNode(const Message& msg);
		void DelNode(const Message& msg);
		void MoveNode(const Message& msg);
		void SelectNode(const Message& msg);
		void AddExit(const Message& msg);
		void ConnectPin(const Message& msg);
		void DisconnectPin(const Message& msg);
		void DisconnectLine(const Message& msg);
		void SetPinValue(const Message& msg);
		void AddParam(const Message& msg);
		void DelParam(const Message& msg);
		void RenameParam(const Message& msg);
		void SetParamType(const Message& msg);
		void GetDetails(const Message& msg);

		void GenerateCode(const Message& msg);
		void CompileType(const Message& msg);
		void LoadType(const Message& msg);
		void UnloadType(const Message& msg);
		void GetPalette(const Message& msg);

		void MakeEntry(const std::shared_ptr<AFlow>& flow);
		std::shared_ptr<AFlow> FindFlow() const;
		std::shared_ptr<AGraph> FindGraph() const;
		std::shared_ptr<VNode> FindNode(const std::string& guid) const;
		std::shared_ptr<APin> FindPin(const std::string& guid) const;

		std::string NewFlow(const std::string& base) const;
		std::string NewVar(const std::string& base) const;
		std::string SelfType() const;
		bool IsIdent(const std::string& name) const;

		bool PlaceNode(const std::shared_ptr<VNode>& node, float x, float y);
		bool PlaceKey(const std::string& key, float x, float y);

		void Rise(Action action, std::string token = {}, std::string data = {});
		void Reply(int act, const std::string& token, const std::string& data);
		void Print(const std::string& text);
		std::string MakeDetailsJson() const;
		void Clear();

		Graph MakeGraph() const;
		Node MakeNode(const std::shared_ptr<VNode>& node) const;
		Blueprint MakeBp() const;
		Palette MakePalette() const;
		void LoadAssets();
		NodeDetails MakeNodeDetails(const std::shared_ptr<VNode>& node) const;

		bool Compile();
		bool WriteFile(const std::string& path, const std::string& text) const;
		std::string AssetPath(const std::string& space, const std::string& name) const;
		std::string AddAsset(const std::string& space, const std::string& name);
		bool DelAsset(const std::string& key);
		bool ReadAssetFile(const std::string& path, std::string& space, std::string& name) const;
		void MakeAssetDir() const;


	  std::string MakeAssets() const;
		std::optional<AssetItem> FindAsset(const std::string& key) const;

		HostConfig config_;
		std::shared_ptr<VBlueprint> blueprint_;
		std::string current_flow_guid_;
		std::string selected_node_guid_;
		std::string current_asset_guid_;
		std::function<void(const Message&)> rise_;
		std::unordered_map<std::string, AssetItem> assets_;
		TBus<TEvent<Action, Message>> handlers_;
	};

}