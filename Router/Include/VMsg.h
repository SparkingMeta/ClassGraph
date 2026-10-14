#pragma once

#include "IRouter.h"

#include <nlohmann/json.hpp>
#include <string>

namespace CG::Router {

	inline nlohmann::json MsgBody(const Message& msg)
	{
		const std::string text = MessageText(msg);
		if (text.empty())
			return nlohmann::json::object();
		auto json = nlohmann::json::parse(text, nullptr, false);
		if (json.is_discarded() || !json.is_object())
			return nlohmann::json::object();
		return json;
	}

	inline std::string JsonStr(const nlohmann::json& json, const char* key)
	{
		if (!json.contains(key) || !json[key].is_string())
			return {};
		return json[key].get<std::string>();
	}

	inline double JsonNum(const nlohmann::json& json, const char* key, double fallback = 0)
	{
		if (!json.contains(key) || !json[key].is_number())
			return fallback;
		return json[key].get<double>();
	}

	inline Message MakeReply(const Message& req, nlohmann::json body)
	{
		return MakeMessage(static_cast<Action>(req.act), req.token, body.dump());
	}

}