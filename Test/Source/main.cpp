#include "Abstract/ACaller.h"
#include "Abstract/AContext.h"
#include "Abstract/AFrame.h"
#include "Abstract/APin.h"
#include "Base/VRegister.h"
#include "Base/VSystem.h"
#include "Base/VConvert.h"
#include "CodeGen/CCompiler.h"
#include "Engine.h"
#include "Realistic/VAsset.h"
#include "Realistic/VController.h"
#include "Realistic/VFlow.h"
#include "Realistic/VFunction.h"
#include "Realistic/VGraph.h"
#include "Realistic/VLine.h"
#include "Realistic/VNode.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using namespace CG::Core;
namespace fs = std::filesystem;

namespace {

class RecordingCaller final : public ACaller {
public:
	explicit RecordingCaller(int& calls) : calls_(calls) { SetName("Recorder"); }

	bool IsPure() const override { return false; }
	int GetKind() const override { return CK_Operator; }
	CallMeta GetCallMeta() const override { return {}; }
	ExecMeta GetExecMeta() const override { return ExecMeta{ {}, true, false }; }
	Result Call(VObject*, AContext*) override
	{
		++calls_;
		return Result{ {}, 0, 0, false };
	}

private:
	int& calls_;
};

int DynamicBound()
{
	return 7;
}

using Json = nlohmann::json;

::Message Send(::cg::engine::Engine& engine, std::vector<::Message>& messages,
	::cg::Action action, const std::string& token, const Json& data)
{
	const size_t first_new_message = messages.size();
	::Message request;
	request.act = action;
	request.token = token;
	request.data = data.dump();
	engine.Apply(request);
	for (size_t i = messages.size(); i > first_new_message; --i) {
		const auto& message = messages[i - 1];
		if (message.act == action && message.token == token)
			return message;
	}
	::Message missing;
	missing.code = 1;
	return missing;
}

bool HasPin(const Json& node, const std::string& name)
{
	for (const auto& pin : node.value("pins", Json::array()))
		if (pin.is_object() && pin.value("name", std::string{}) == name)
			return true;
	return false;
}

Json FindPinRow(const Json& node, const std::string& name, int direct, int kind)
{
	for (const auto& pin : node.value("pins", Json::array())) {
		if (pin.is_object() && pin.value("name", std::string{}) == name
			&& pin.value("direct", -1) == direct && pin.value("kind", -1) == kind)
			return pin;
	}
	return Json::object();
}

Json FindNodeRow(const Json& flow, const std::string& guid)
{
	for (const auto& node : flow.value("nodes", Json::array()))
		if (node.is_object() && node.value("guid", std::string{}) == guid)
			return node;
	return Json::object();
}

void Check(bool condition, const std::string& name, int& failures)
{
	if (condition) return;
	std::cerr << "FAIL: " << name << '\n';
	++failures;
}

std::shared_ptr<APin> FindPin(const std::shared_ptr<VNode>& node, EDirect direct, const std::string& name)
{
	const auto pins = direct == EDirect::In ? node->GetInputPins() : node->GetOutputPins();
	for (const auto& pin : pins) {
		if (pin && pin->GetPinMeta().name == name)
			return pin;
	}
	return {};
}

template<typename T>
bool SetFrameValue(const std::shared_ptr<VNode>& node, const std::string& name, T value)
{
	auto* context = node ? node->GetContext() : nullptr;
	auto frame = context ? context->GetFrame(name) : nullptr;
	const VType* type = FindType(typeid(T));
	if (!frame || !type || !frame->SetValue(type, &value))
		return false;
	if (auto pin = FindPin(node, EDirect::In, name)) {
		auto meta = pin->GetPinMeta();
		meta.value = ToString(frame);
		pin->SetPinMeta(meta);
	}
	return true;
}

template<typename T>
bool ReadFrameValue(const std::shared_ptr<VNode>& node, const std::string& name, T& value)
{
	auto* context = node ? node->GetContext() : nullptr;
	auto frame = context ? context->GetFrame(name) : nullptr;
	const VType* type = FindType(typeid(T));
	return frame && type && frame->GetValue(type, &value);
}

std::shared_ptr<VNode> MakeRecorder(int& calls)
{
	return VNode::Make(std::make_shared<RecordingCaller>(calls));
}

void TestBranchExecution(int& failures)
{
	int true_calls = 0;
	int false_calls = 0;
	auto branch = VNode::Make(std::make_shared<FBranch>());
	auto on_true = MakeRecorder(true_calls);
	auto on_false = MakeRecorder(false_calls);
	Check(SetFrameValue(branch, "cond", true), "branch condition frame initialized", failures);
	Check(static_cast<bool>(MakeLine(FindPin(branch, EDirect::Out, "true"), FindPin(on_true, EDirect::In, ""))),
		"branch true path connects", failures);
	Check(static_cast<bool>(MakeLine(FindPin(branch, EDirect::Out, "false"), FindPin(on_false, EDirect::In, ""))),
		"branch false path connects", failures);
	Check(branch->Next(0) == on_true && branch->Next(1) == on_false, "branch output indices resolve to paths", failures);

	const auto true_result = branch->Work();
	Check(true_result.code == 0, "branch execution succeeds", failures);
	Check(true_calls == 1 && false_calls == 0, "branch selects true path", failures);
	Check(SetFrameValue(branch, "cond", false), "branch condition can be updated", failures);
	const auto false_result = branch->Work();
	Check(false_result.code == 0, "branch execution succeeds for false condition", failures);
	Check(true_calls == 1 && false_calls == 1, "branch selects false path", failures);
}

void TestDynamicControllers(int& failures)
{
	auto sequence_caller = std::make_shared<FSequence>();
	auto sequence = VNode::Make(sequence_caller);
	const auto then_2 = sequence_caller->AddThen();
	sequence->ApplyMeta();
	Check(then_2 == "then_2", "sequence chooses the next free pin name", failures);
	Check(FindPin(sequence, EDirect::Out, "then_0") && FindPin(sequence, EDirect::Out, "then_1")
		&& FindPin(sequence, EDirect::Out, then_2), "sequence rebuilds dynamic pins", failures);

	int first_calls = 0;
	int second_calls = 0;
	int third_calls = 0;
	auto first = MakeRecorder(first_calls);
	auto second = MakeRecorder(second_calls);
	auto third = MakeRecorder(third_calls);
	const auto first_pin = FindPin(sequence, EDirect::Out, "then_0");
	const auto second_pin = FindPin(sequence, EDirect::Out, "then_1");
	const auto third_pin = FindPin(sequence, EDirect::Out, then_2);
	Check(static_cast<bool>(MakeLine(first_pin, FindPin(first, EDirect::In, ""))), "first sequence path connects", failures);
	Check(static_cast<bool>(MakeLine(second_pin, FindPin(second, EDirect::In, ""))), "second sequence path connects", failures);
	const auto third_line = MakeLine(third_pin, FindPin(third, EDirect::In, ""));
	Check(static_cast<bool>(third_line), "dynamic sequence path connects", failures);

	sequence->Work();
	Check(first_calls == 1 && second_calls == 1 && third_calls == 1, "sequence executes each path once", failures);

	Check(sequence_caller->DelThen(then_2), "sequence deletes a dynamic pin", failures);
	sequence->ApplyMeta();
	Check(!FindPin(sequence, EDirect::Out, then_2), "deleted sequence pin disappears", failures);
	Check(!third_line->GetFrom() && !third_line->GetTo(), "deleting a pin disconnects its line", failures);
	Check(sequence_caller->DelThen("then_0"), "sequence can remove an output while multiple remain", failures);
	sequence->ApplyMeta();
	Check(!sequence_caller->DelThen("then_1"), "sequence refuses to delete its last output", failures);

	auto switch_caller = std::make_shared<FSwitchInt>();
	Check(switch_caller->AddCase() == "case_1", "switch adds a uniquely named case", failures);
	Check(switch_caller->SetCaseValue("case_0", "10"), "switch accepts an integer case value", failures);
	Check(switch_caller->SetCaseValue("case_1", "20"), "switch accepts a second case value", failures);
	Check(!switch_caller->SetCaseValue("case_1", "10"), "switch rejects duplicate values", failures);
	Check(!switch_caller->SetCaseValue("case_1", "20x"), "switch rejects malformed values", failures);
	Check(!switch_caller->SetCaseValue("default", "30"), "switch default is not editable as a case", failures);
	const auto case_2 = switch_caller->AddCase();
	Check(case_2 == "case_2" && switch_caller->GetCallMeta().args[3].value == "0",
		"switch assigns an unused case value", failures);

	auto switch_node = VNode::Make(switch_caller);
	switch_node->ApplyMeta();
	int case_calls = 0;
	int default_calls = 0;
	auto case_recorder = MakeRecorder(case_calls);
	auto default_recorder = MakeRecorder(default_calls);
	Check(SetFrameValue(switch_node, "cond", 20), "switch condition frame initialized", failures);
	Check(static_cast<bool>(MakeLine(FindPin(switch_node, EDirect::Out, "case_1"), FindPin(case_recorder, EDirect::In, ""))),
		"switch case path connects", failures);
	Check(static_cast<bool>(MakeLine(FindPin(switch_node, EDirect::Out, "default"), FindPin(default_recorder, EDirect::In, ""))),
		"switch default path connects", failures);
	Check(switch_node->Next(1) == case_recorder && switch_node->Next(3) == default_recorder,
		"switch output indices resolve to case and default", failures);
	const auto case_result = switch_node->Work();
	Check(case_result.code == 0, "switch execution succeeds", failures);
	Check(case_calls == 1 && default_calls == 0, "switch selects the matching case", failures);
	Check(SetFrameValue(switch_node, "cond", 99), "switch condition can be updated", failures);
	const auto default_result = switch_node->Work();
	Check(default_result.code == 0, "switch execution succeeds for unmatched values", failures);
	Check(case_calls == 1 && default_calls == 1, "switch selects default for unmatched values", failures);
}

void TestLoopExecution(int& failures)
{
	auto loop = VNode::Make(std::make_shared<FLoop>());
	int body_calls = 0;
	int completed_calls = 0;
	auto body = MakeRecorder(body_calls);
	auto completed = MakeRecorder(completed_calls);
	Check(SetFrameValue(loop, "cond", true), "loop condition frame initialized", failures);
	Check(SetFrameValue(loop, "first", 2), "loop first frame initialized", failures);
	Check(SetFrameValue(loop, "last", 5), "loop last frame initialized", failures);
	Check(static_cast<bool>(MakeLine(FindPin(loop, EDirect::Out, "body"), FindPin(body, EDirect::In, ""))),
		"loop body path connects", failures);
	Check(static_cast<bool>(MakeLine(FindPin(loop, EDirect::Out, "completed"), FindPin(completed, EDirect::In, ""))),
		"loop completion path connects", failures);
	const auto loop_result = loop->Work();
	Check(loop_result.code == 0, "loop execution succeeds", failures);
	int index = -1;
	Check(body_calls == 3 && ReadFrameValue(loop, "index", index) && index == 4,
		"loop executes a half-open range and exposes its final index", failures);
	Check(completed_calls == 1, "loop executes completion after its final iteration", failures);
	Check(SetFrameValue(loop, "cond", false), "loop condition can be disabled", failures);
	Check(loop->Next(0) == completed && loop->Next(1) == body, "loop output indices resolve to completion and body", failures);
	const auto empty_loop_result = loop->Work();
	Check(empty_loop_result.code == 0, "loop execution succeeds when disabled", failures);
	Check(body_calls == 3 && completed_calls == 2, "false loop condition skips body and completes", failures);
}

void TestCodeGeneration(int& failures)
{
	VBlueprint switch_asset;
	switch_asset.SetName("SwitchRegression");
	const auto switch_flow_id = switch_asset.AddFlow("Run");
	auto switch_flow = switch_asset.FindFlow(switch_flow_id);
	auto switch_graph = switch_flow ? std::dynamic_pointer_cast<VGraph>(switch_flow->GetGraph()) : nullptr;
	auto switch_caller = std::make_shared<FSwitchInt>();
	switch_caller->SetCaseValue("case_0", "10");
	const auto case_1 = switch_caller->AddCase();
	switch_caller->SetCaseValue(case_1, "20");
	auto switch_node = VNode::Make(switch_caller);
	if (switch_graph) {
		switch_graph->AddNode(switch_node);
		switch_graph->SetEntry(switch_node);
	}
	std::string source, header, generated_header, meta;
	const int switch_result = switch_asset.GenerateCode(source, header, generated_header, meta);
	Check(switch_graph && switch_result == 0, "switch blueprint compiles", failures);
	Check(source.find("case 10:") != std::string::npos && source.find("case 20:") != std::string::npos
		&& source.find("default:") != std::string::npos,
		"switch code generation preserves configured case labels", failures);

	VBlueprint loop_asset;
	loop_asset.SetName("LoopRegression");
	const auto loop_flow_id = loop_asset.AddFlow("Run");
	auto loop_flow = loop_asset.FindFlow(loop_flow_id);
	auto loop_graph = loop_flow ? std::dynamic_pointer_cast<VGraph>(loop_flow->GetGraph()) : nullptr;
	auto loop_caller = std::make_shared<FLoop>();
	auto loop_node = VNode::Make(loop_caller);
	SetFrameValue(loop_node, "cond", false);
	SetFrameValue(loop_node, "first", 2);
	SetFrameValue(loop_node, "last", 8);

	auto bound_function = std::make_shared<VFunction>();
	bound_function->SetName("DynamicBound");
	bound_function->SetMeta(Load(&DynamicBound));
	EmitMeta emit;
	emit.emit_kind = EmitKind::EmitCall;
	emit.callee = "GetDynamicBound";
	bound_function->SetEmit(emit);
	auto bound_node = VNode::Make(bound_function);
	const auto bound_line = MakeLine(FindPin(bound_node, EDirect::Out, "return"), FindPin(loop_node, EDirect::In, "last"));
	Check(static_cast<bool>(bound_line), "zero-argument function output connects to loop bound", failures);
	if (loop_graph) {
		loop_graph->AddNode(loop_node);
		loop_graph->AddNode(bound_node);
		loop_graph->SetEntry(loop_node);
	}
	source.clear();
	const int loop_result = loop_asset.GenerateCode(source, header, generated_header, meta);
	Check(loop_graph && loop_result == 0, "loop blueprint compiles", failures);
	Check(source.find("if (false)") != std::string::npos, "loop code generation honors its condition", failures);
	Check(source.find("= GetDynamicBound();") != std::string::npos
		&& source.find("< __cg_loop_last_") != std::string::npos,
		"loop code generation snapshots its bound before iteration", failures);
	const auto bound_call = source.find("GetDynamicBound()");
	Check(bound_call != std::string::npos && source.find("GetDynamicBound()", bound_call + 1) == std::string::npos,
		"zero-argument result function is emitted exactly once", failures);
}

void TestEngineControllerIsolationAndPersistence(int& failures)
{
	struct Cleanup {
		fs::path path;
		~Cleanup()
		{
			std::error_code error;
			fs::remove_all(path, error);
		}
	};

	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path root = fs::current_path() / ("BlueprintEngineTests-" + std::to_string(stamp));
	Cleanup cleanup{root};
	std::string asset_guid, flow_guid, switch_a_guid, switch_b_guid, sequence_a_guid, sequence_b_guid;
	std::string exit_guid;

	{
		::cg::engine::Engine engine;
		std::vector<::Message> messages;
		engine.SetRoot(root.string());
		engine.SetRise([&](const auto& message) { messages.push_back(message); });
		engine.Load();
		int token = 0;
		auto send = [&](::cg::Action action, const Json& data) {
			return Send(engine, messages, action, std::to_string(++token), data);
		};

		auto created = send(::cg::Action::AddBP, {{"name", "EngineRegression"}, {"space", "Tests"}});
		Check(created.code == 0, "engine creates a blueprint on disk", failures);
		if (created.code != 0) return;
		asset_guid = Json::parse(created.data).value("guid", std::string{});

		auto function = send(::cg::Action::AddFunc, {{"guid", asset_guid}, {"name", "Run"}});
		Check(function.code == 0, "engine creates a blueprint function", failures);
		if (function.code != 0) return;
		flow_guid = Json::parse(function.data).value("guid", std::string{});

		auto add_node = [&](const std::string& key) {
			return send(::cg::Action::AddNode, {{"guid", flow_guid}, {"key", key}});
		};
		auto switch_a = add_node("SwitchInt");
		auto switch_b = add_node("SwitchInt");
		auto sequence_a = add_node("Sequence");
		auto sequence_b = add_node("Sequence");
		Check(switch_a.code == 0 && switch_b.code == 0 && sequence_a.code == 0 && sequence_b.code == 0,
			"engine creates independent controller nodes", failures);
		if (switch_a.code != 0 || switch_b.code != 0 || sequence_a.code != 0 || sequence_b.code != 0) return;
		switch_a_guid = Json::parse(switch_a.data).value("guid", std::string{});
		switch_b_guid = Json::parse(switch_b.data).value("guid", std::string{});
		sequence_a_guid = Json::parse(sequence_a.data).value("guid", std::string{});
		sequence_b_guid = Json::parse(sequence_b.data).value("guid", std::string{});

		auto add_case = [&](const std::string& guid) {
			return send(::cg::Action::AltNode, {{"guid", guid}, {"param_action", "add_case"}});
		};
		auto case_a = add_case(switch_a_guid);
		auto case_b = add_case(switch_b_guid);
		Check(case_a.code == 0 && case_b.code == 0, "engine adds a switch case to each node", failures);
		if (case_a.code != 0 || case_b.code != 0) return;
		Check(HasPin(Json::parse(case_a.data), "case_1") && HasPin(Json::parse(case_b.data), "case_1"),
			"switch case lists do not leak between nodes", failures);
		Json case_a_body = Json::parse(case_a.data);
		const Json case_a_pin = FindPinRow(case_a_body, "case_1", 1, 0);
		auto set_case_value = send(::cg::Action::AltNode, {
			{"guid", switch_a_guid}, {"param_action", "set_case_value"},
			{"pin", case_a_pin.value("guid", std::string{})}, {"value", "42"}
		});
		Check(set_case_value.code == 0, "engine updates a switch case value", failures);
		if (set_case_value.code != 0) return;

		auto add_then = [&](const std::string& guid) {
			return send(::cg::Action::AltNode, {{"guid", guid}, {"param_action", "add_then"}});
		};
		auto then_a = add_then(sequence_a_guid);
		auto then_b = add_then(sequence_b_guid);
		Check(then_a.code == 0 && then_b.code == 0, "engine adds a sequence pin to each node", failures);
		if (then_a.code != 0 || then_b.code != 0) return;
		Check(HasPin(Json::parse(then_a.data), "then_2") && HasPin(Json::parse(then_b.data), "then_2"),
			"sequence output lists do not leak between nodes", failures);
		const Json switch_b_case = FindPinRow(Json::parse(case_b.data), "case_1", 1, 0);
		const Json sequence_a_input = FindPinRow(Json::parse(then_a.data), "", 0, 0);
		auto dynamic_line = send(::cg::Action::AddLine, {
			{"flow", flow_guid}, {"from_node", switch_b_guid},
			{"from_pin", switch_b_case.value("guid", std::string{})}, {"to_node", sequence_a_guid},
			{"to_pin", sequence_a_input.value("guid", std::string{})}
		});
		Check(dynamic_line.code == 0, "engine connects a dynamic controller pin", failures);
		if (dynamic_line.code != 0) return;

		auto delete_case = send(::cg::Action::AltNode, {
			{"guid", switch_b_guid}, {"param_action", "delete_case"},
			{"pin", switch_b_case.value("guid", std::string{})}
		});
		const Json delete_case_body = Json::parse(delete_case.data, nullptr, false);
		Check(delete_case.code == 0 && delete_case_body.is_object()
			&& delete_case_body.value("removed_lines", Json::array()).size() == 1,
			"deleting a switch case removes and reports its connection", failures);
		if (delete_case.code != 0) return;

		const Json sequence_b_pin = FindPinRow(Json::parse(then_b.data), "then_2", 1, 0);
		auto delete_then = send(::cg::Action::AltNode, {
			{"guid", sequence_b_guid}, {"param_action", "delete_then"},
			{"pin", sequence_b_pin.value("guid", std::string{})}
		});
		Check(delete_then.code == 0 && !HasPin(Json::parse(delete_then.data), "then_2"),
			"engine removes a sequence pin from only its target node", failures);
		if (delete_then.code != 0) return;

		auto exit = add_node("Exit");
		Check(exit.code == 0, "engine creates a function exit node", failures);
		if (exit.code != 0) return;
		const Json exit_data = Json::parse(exit.data);
		exit_guid = exit_data.value("guid", std::string{});
		const Json default_pin = FindPinRow(Json::parse(case_a.data), "default", 1, 0);
		const Json exit_pin = FindPinRow(exit_data, "", 0, 0);
		auto line = send(::cg::Action::AddLine, {
			{"flow", flow_guid}, {"from_node", switch_a_guid}, {"from_pin", default_pin.value("guid", std::string{})},
			{"to_node", exit_guid}, {"to_pin", exit_pin.value("guid", std::string{})}
		});
		Check(line.code == 0, "engine connects and persists controller flow lines: " + line.data, failures);
	}

	{
		::cg::engine::Engine engine;
		std::vector<::Message> messages;
		engine.SetRoot(root.string());
		engine.SetRise([&](const auto& message) { messages.push_back(message); });
		engine.Load();
		std::string loaded_guid;
		for (const auto& message : messages) {
			if (message.act != ::cg::Action::AddBP || message.code != 0) continue;
			const auto data = Json::parse(message.data, nullptr, false);
			if (!data.is_object() || data.value("name", std::string{}) != "EngineRegression") continue;
			loaded_guid = data.value("guid", std::string{});
		}
		Check(loaded_guid == asset_guid, "engine reloads the saved blueprint", failures);
		if (loaded_guid.empty()) return;
		auto opened = Send(engine, messages, ::cg::Action::OpenBP, "open", {{"guid", loaded_guid}});
		Check(opened.code == 0, "engine opens a reloaded blueprint", failures);
		if (opened.code != 0) return;
		const auto body = Json::parse(opened.data);
		Json flow;
		for (const auto& row : body.value("flows", Json::array()))
			if (row.is_object() && row.value("guid", std::string{}) == flow_guid) flow = row;
		Check(flow.is_object(), "reloaded function is present", failures);
		if (!flow.is_object()) return;

		const Json switch_a = FindNodeRow(flow, switch_a_guid);
		const Json switch_b = FindNodeRow(flow, switch_b_guid);
		const Json sequence_a = FindNodeRow(flow, sequence_a_guid);
		const Json sequence_b = FindNodeRow(flow, sequence_b_guid);
		Check(HasPin(switch_a, "case_1") && !HasPin(switch_b, "case_1")
			&& !HasPin(switch_a, "case_2") && !HasPin(switch_b, "case_2"),
			"reloaded switch nodes retain separate dynamic pins", failures);
		Check(FindPinRow(switch_a, "case_1", 1, 0).value("value", std::string{}) == "42"
			&& !HasPin(switch_b, "case_1"),
			"reloaded switch values and pin deletions remain node-local", failures);
		Check(HasPin(sequence_a, "then_2") && !HasPin(sequence_b, "then_2"),
			"reloaded sequence nodes retain node-local additions and deletions", failures);
		Check(flow.value("lines", Json::array()).size() == 1,
			"reloaded blueprint retains its execution connection", failures);
	}
}

void TestCompiledBlueprintTypeDependency(int& failures)
{
#ifndef _WIN32
	(void)failures;
#else
	struct Cleanup {
		fs::path path;
		~Cleanup()
		{
			std::error_code error;
			fs::remove_all(path, error);
		}
	};

	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path root = fs::temp_directory_path() / ("CGDep-" + std::to_string(stamp));
	const fs::path assets = root / "assets";
	const fs::path types = root / "types";
	Cleanup cleanup{root};
	constexpr const char* space = "BlueprintDeps";
	constexpr const char* producer_name = "ProducerType";
	constexpr const char* consumer_name = "ConsumerType";
	constexpr const char* producer_type = "BlueprintDeps::ProducerType";

	std::string consumer_guid;
	std::string consumer_flow_guid;
	std::string consumer_exit_guid;
	std::string consumer_result_pin_guid;
	{
		::cg::engine::Engine engine;
		std::vector<::Message> messages;
		engine.SetRoot(assets.string());
		engine.SetTypeDir(types.string());
		engine.SetRise([&](const auto& message) { messages.push_back(message); });
		engine.Load();
		int token = 0;
		auto send = [&](::cg::Action action, const Json& data) {
			return Send(engine, messages, action, std::to_string(++token), data);
		};
		auto connect = [&](const std::string& flow, const std::string& from_node, const Json& from_pin,
			const std::string& to_node, const Json& to_pin) {
			return send(::cg::Action::AddLine, {
				{"flow", flow}, {"from_node", from_node}, {"from_pin", from_pin.value("guid", std::string{})},
				{"to_node", to_node}, {"to_pin", to_pin.value("guid", std::string{})}
			});
		};

		auto producer = send(::cg::Action::AddBP, {{"name", producer_name}, {"space", space}});
		Check(producer.code == 0, "engine creates a blueprint type producer", failures);
		if (producer.code != 0) return;
		const std::string producer_guid = Json::parse(producer.data).value("guid", std::string{});
		auto producer_method = send(::cg::Action::AddFunc,
			{{"guid", producer_guid}, {"name", "GetAnswer"}});
		Check(producer_method.code == 0, "producer adds a method", failures);
		if (producer_method.code != 0) return;
		const Json producer_method_body = Json::parse(producer_method.data);
		const std::string producer_flow_guid = producer_method_body.value("guid", std::string{});
		const Json producer_entry = producer_method_body.value("node", Json::object());
		const std::string producer_entry_guid = producer_entry.value("guid", std::string{});

		auto producer_exit = send(::cg::Action::AddNode,
			{{"guid", producer_flow_guid}, {"key", "Exit"}});
		Check(producer_exit.code == 0, "producer adds a method exit", failures);
		if (producer_exit.code != 0) return;
		const Json producer_exit_body = Json::parse(producer_exit.data);
		const std::string producer_exit_guid = producer_exit_body.value("guid", std::string{});
		auto producer_result = send(::cg::Action::AltNode, {
			{"guid", producer_exit_guid}, {"param_action", "add"}, {"name", "answer"}, {"type", "int"}
		});
		Check(producer_result.code == 0, "producer method declares an integer result", failures);
		if (producer_result.code != 0) return;
		const Json producer_result_pin = FindPinRow(Json::parse(producer_result.data), "answer", 0, 1);
		auto set_answer = send(::cg::Action::AltNode, {
			{"guid", producer_exit_guid}, {"pin", producer_result_pin.value("guid", std::string{})}, {"value", "42"}
		});
		Check(set_answer.code == 0, "producer method sets its result value", failures);
		if (set_answer.code != 0) return;

		auto producer_exec_line = connect(producer_flow_guid, producer_entry_guid,
			FindPinRow(producer_entry, "", 1, 0), producer_exit_guid,
			FindPinRow(producer_exit_body, "", 0, 0));
		Check(producer_exec_line.code == 0, "producer method execution path connects", failures);
		if (producer_exec_line.code != 0) return;

		auto compiled_producer = send(::cg::Action::Compile, {{"guid", producer_guid}});
		Check(compiled_producer.code == 0, "producer blueprint compiles into a loadable type: " + compiled_producer.data, failures);
		if (compiled_producer.code != 0) return;
		Check(fs::is_regular_file(types / space / (std::string(producer_name) + ".dll"))
			&& fs::is_regular_file(types / space / (std::string(producer_name) + ".lib")),
			"producer publishes its DLL and import library", failures);

		auto consumer = send(::cg::Action::AddBP, {{"name", consumer_name}, {"space", space}});
		Check(consumer.code == 0, "engine creates a blueprint type consumer", failures);
		if (consumer.code != 0) return;
		consumer_guid = Json::parse(consumer.data).value("guid", std::string{});
		auto variable = send(::cg::Action::AddVar, {
			{"guid", consumer_guid}, {"name", "source"}, {"type", producer_type}
		});
		Check(variable.code == 0, "consumer accepts the compiled type as a variable", failures);
		if (variable.code != 0) return;
		const std::string variable_guid = Json::parse(variable.data).value("guid", std::string{});

		auto consumer_method = send(::cg::Action::AddFunc,
			{{"guid", consumer_guid}, {"name", "Run"}});
		Check(consumer_method.code == 0, "consumer adds a run method", failures);
		if (consumer_method.code != 0) return;
		const Json consumer_method_body = Json::parse(consumer_method.data);
		consumer_flow_guid = consumer_method_body.value("guid", std::string{});
		const Json consumer_entry = consumer_method_body.value("node", Json::object());
		const std::string consumer_entry_guid = consumer_entry.value("guid", std::string{});

		auto consumer_exit = send(::cg::Action::AddNode,
			{{"guid", consumer_flow_guid}, {"key", "Exit"}});
		Check(consumer_exit.code == 0, "consumer adds a method exit", failures);
		if (consumer_exit.code != 0) return;
		const Json consumer_exit_body = Json::parse(consumer_exit.data);
		consumer_exit_guid = consumer_exit_body.value("guid", std::string{});
		auto consumer_result = send(::cg::Action::AltNode, {
			{"guid", consumer_exit_guid}, {"param_action", "add"}, {"name", "answer"}, {"type", "int"}
		});
		Check(consumer_result.code == 0, "consumer method declares an integer result", failures);
		if (consumer_result.code != 0) return;
		consumer_result_pin_guid = FindPinRow(Json::parse(consumer_result.data), "answer", 0, 1)
			.value("guid", std::string{});

		auto getter = send(::cg::Action::AddNode, {
			{"guid", consumer_flow_guid}, {"key", std::string("GetVar:") + variable_guid}
		});
		Check(getter.code == 0, "consumer creates a getter for the compiled-type variable", failures);
		if (getter.code != 0) return;
		const Json getter_body = Json::parse(getter.data);
		const std::string getter_guid = getter_body.value("guid", std::string{});
		auto method = send(::cg::Action::AddNode, {
			{"guid", consumer_flow_guid},
			{"key", "ClassMethod:BlueprintDeps::ProducerType::GetAnswer"}
		});
		Check(method.code == 0, "consumer resolves a method on the compiled type", failures);
		if (method.code != 0) return;
		const Json method_body = Json::parse(method.data);
		const std::string method_guid = method_body.value("guid", std::string{});

		auto self_line = connect(consumer_flow_guid, getter_guid,
			FindPinRow(getter_body, "return", 1, 1), method_guid,
			FindPinRow(method_body, "self", 0, 1));
		Check(self_line.code == 0, "consumer connects its variable to the compiled-type method", failures);
		if (self_line.code != 0) return;
		auto result_line = connect(consumer_flow_guid, method_guid,
			FindPinRow(method_body, "return", 1, 1), consumer_exit_guid,
			FindPinRow(Json::parse(consumer_result.data), "answer", 0, 1));
		Check(result_line.code == 0, "consumer connects the compiled method result", failures);
		if (result_line.code != 0) return;
		auto entry_line = connect(consumer_flow_guid, consumer_entry_guid,
			FindPinRow(consumer_entry, "", 1, 0), method_guid,
			FindPinRow(method_body, "", 0, 0));
		Check(entry_line.code == 0, "consumer invokes the compiled method through its execution path", failures);
		if (entry_line.code != 0) return;
		auto exit_line = connect(consumer_flow_guid, method_guid,
			FindPinRow(method_body, "", 1, 0), consumer_exit_guid,
			FindPinRow(consumer_exit_body, "", 0, 0));
		Check(exit_line.code == 0, "consumer method reaches its exit", failures);
		if (exit_line.code != 0) return;

		auto compiled_consumer = send(::cg::Action::Compile, {{"guid", consumer_guid}});
		Check(compiled_consumer.code == 0, "consumer compiles and links against the producer type: " + compiled_consumer.data, failures);
		if (compiled_consumer.code != 0) return;

		const std::string run_token = "run-consumer";
		auto run = Send(engine, messages, ::cg::Action::RunFunc, run_token,
			{{"asset", consumer_guid}, {"flow", consumer_flow_guid}});
		Check(run.code == 0, "consumer invokes the compiled method at runtime", failures);
		bool result_is_42 = false;
		for (const auto& message : messages) {
			if (message.act != ::cg::Action::RuntimeSync || message.token != run_token)
				continue;
			const Json event = Json::parse(message.data, nullptr, false);
			if (!event.is_object() || event.value("phase", std::string{}) != "node"
				|| event.value("guid", std::string{}) != consumer_exit_guid)
				continue;
			for (const auto& pin : event.value("pins", Json::array())) {
				if (pin.is_object() && pin.value("guid", std::string{}) == consumer_result_pin_guid
					&& pin.value("value", std::string{}) == "42")
					result_is_42 = true;
			}
		}
		Check(result_is_42, "consumer receives the compiled method result", failures);
	}

	{
		::cg::engine::Engine engine;
		std::vector<::Message> messages;
		engine.SetRoot(assets.string());
		engine.SetTypeDir(types.string());
		engine.SetRise([&](const auto& message) { messages.push_back(message); });
		engine.Load();
		bool consumer_reloaded = false;
		for (const auto& message : messages) {
			if (message.act != ::cg::Action::AddBP || message.code != 0) continue;
			const Json body = Json::parse(message.data, nullptr, false);
			if (body.is_object() && body.value("guid", std::string{}) == consumer_guid)
				consumer_reloaded = true;
		}
		Check(consumer_reloaded, "engine reloads blueprints with compiled type dependencies", failures);
		if (!consumer_reloaded) return;
		const std::string run_token = "run-consumer-after-reload";
		auto run = Send(engine, messages, ::cg::Action::RunFunc, run_token,
			{{"asset", consumer_guid}, {"flow", consumer_flow_guid}});
		Check(run.code == 0, "reloaded consumer still invokes its compiled type method", failures);
		bool result_is_42 = false;
		for (const auto& message : messages) {
			if (message.act != ::cg::Action::RuntimeSync || message.token != run_token)
				continue;
			const Json event = Json::parse(message.data, nullptr, false);
			if (!event.is_object() || event.value("phase", std::string{}) != "node"
				|| event.value("guid", std::string{}) != consumer_exit_guid)
				continue;
			for (const auto& pin : event.value("pins", Json::array())) {
				if (pin.is_object() && pin.value("guid", std::string{}) == consumer_result_pin_guid
					&& pin.value("value", std::string{}) == "42")
					result_is_42 = true;
			}
		}
		Check(result_is_42, "reloaded consumer preserves the compiled method result", failures);
	}
#endif
}

} // namespace

int main()
{
	VSystem::Get().Init();
	int failures = 0;
	TestBranchExecution(failures);
	TestDynamicControllers(failures);
	TestLoopExecution(failures);
	TestCodeGeneration(failures);
	TestEngineControllerIsolationAndPersistence(failures);
	TestCompiledBlueprintTypeDependency(failures);
	if (failures == 0)
		std::cout << "All blueprint core tests passed.\n";
	return failures == 0 ? 0 : 1;
}
