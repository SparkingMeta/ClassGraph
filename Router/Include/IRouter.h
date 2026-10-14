#pragma once

#include "Export.h"
#include "message.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace CG::Router {

	enum class Action : int32_t {
		None = 0,

		CreateBP,
		DeleteBP,
		RenameBP,
		SetSpace,
		OpenBP,
		ListAssets,
		CloseBP,

		AddFlow,
		DelFlow,
		RenameFlow,
		SelectFlow,
		RunFlow,

		AddVar,
		DelVar,
		RenameVar,
		SetVarType,
		SetVarValue,
		SelectVar,
		PlaceVarGet,
		PlaceVarSet,

		AddNode,
		DelNode,
		MoveNode,
		SelectNode,
		AddExitNode,

		ConnectPin,
		DisconnectPin,
		DisconnectLine,
		SetPinValue,

		AddParam,
		DelParam,
		RenameParam,
		SetParamType,

		GenerateCode,
		CompileType,
		LoadType,
		UnloadType,

		Print,
		RefreshGraph,
		RefreshDetails,
		RefreshPalette,
		RefreshMyBlueprint,
		RefreshAssets,
	};

	inline Message MakeMessage(Action action, std::string token = {}, std::string data = {})
	{
		Message msg;
		msg.act = static_cast<int>(action);
		msg.token = std::move(token);
		if (!data.empty()) {
			const auto* p = reinterpret_cast<const std::byte*>(data.data());
			msg.data.assign(p, p + data.size());
		}
		return msg;
	}

	inline std::string MessageText(const Message& msg)
	{
		if (msg.data.empty())
			return {};
		return std::string(reinterpret_cast<const char*>(msg.data.data()), msg.data.size());
	}

	class ROUTER_API IRouter {
	public:
		virtual ~IRouter() = default;

		virtual void DoAction(const Message& msg) = 0;
		virtual void SetRise(std::function<void(const Message&)> rise) = 0;
	};

}