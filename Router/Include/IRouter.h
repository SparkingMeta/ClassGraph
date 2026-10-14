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
		LoadAssets,
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
		GetGraph,
		GetDetails,
		GetPalette,
		GetBlueprint,
		GetAssets,
	};

	inline Message MakeMessage(Action action, const std::string& token, const std::string& data)
	{
		Message msg;
		msg.act = static_cast<int>(action);
		msg.token = token;
		msg.data = data;
		
		return msg;
	}

	inline std::string MessageText(const Message& msg)
	{
		return msg.data;
	}

	class ROUTER_API IRouter {
	public:
		virtual ~IRouter() = default;

		virtual void DoAction(const Message& msg) = 0;
		virtual void SetRise(std::function<void(const Message&)> rise) = 0;
	};

}

template <>
struct std::hash<CG::Router::Action> {
	std::size_t operator()(CG::Router::Action id) const noexcept {
		return static_cast<std::size_t>(id);
	}
};