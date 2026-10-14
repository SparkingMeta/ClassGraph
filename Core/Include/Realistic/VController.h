#pragma once
#include "Abstract/AController.h"
#include "Base/VType.h"
#include <memory>
#include <string>
#include <vector>

namespace CG::Core {
	class AContext;
	class AFrame;
	class VContext;

	class FSwitchInt : public AController {
	public:
		FSwitchInt();
		virtual ~FSwitchInt() = default;
		ExecMeta GetExecMeta() const override;
		std::string AddCase();
		bool DelCase(const std::string& name);
	private:
		Status Init(AContext* ctx) override;
		void Invoke(AContext* ctx, Status& status) override;
	};

	class FBranch : public AController {
	public:
		FBranch();
		ExecMeta GetExecMeta() const override;
	private:
		Status Init(AContext* ctx) override;
		void Invoke(AContext* ctx, Status& status) override;
	};

	
	class FWhile : public AController {
	public:
		FWhile();
		ExecMeta GetExecMeta() const override;
	private:
		Status Init(AContext* ctx) override;
		void Invoke(AContext* ctx, Status& status) override;
	};

	class FLoop : public AController {
	public:
		FLoop();
		ExecMeta GetExecMeta() const override;
	private:
		Status Init(AContext* ctx) override;
		void Invoke(AContext* ctx, Status& status) override;
	};

	class FSequence : public AController {
	public:
		FSequence();
		ExecMeta GetExecMeta() const override;
	private:
		Status Init(AContext* ctx) override;
		void Invoke(AContext* ctx, Status& status) override;
	};

}
