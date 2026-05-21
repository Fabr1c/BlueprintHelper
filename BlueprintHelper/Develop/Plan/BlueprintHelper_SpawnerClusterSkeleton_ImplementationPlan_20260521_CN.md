# BlueprintHelper SpawnerCluster Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 先修正 GraphStatement Framework 的统一骨架，使 GraphWrite 主入口符合 `AgentFace semantic intent -> SpawnerClusterResolver -> BlueprintActionResolutionCore -> UBlueprintNodeSpawner -> NodeFragment` 的最新架构。

**Architecture:** 本计划只建立统一骨架和主入口，不一次性迁移全部节点创建逻辑。现有 `call` 能力先作为 `FunctionActionCluster` 内部 provider 被包裹；`get/set/get_property/set_property/op/construct/deconstruct/select` 先进入 cluster 路由和诊断模型，后续再逐步迁移到真正的 NodeSpawner family provider。

**Tech Stack:** Unreal Engine 5.6, C++17, BlueprintGraph, Kismet2 ActionDatabase, BlueprintHelper GraphWrite, AgentFace TaskSpec.

---

## 0. 执行状态（2026-05-21）

- [x] 已新增 `ActionResolutionCore`、`SpawnerClusterResolver` 和四个 Spawner family cluster 骨架。
- [x] `call` 主路径已改为 `GraphStatementBuilder -> GraphWriteFacade -> ActionResolutionCore -> FunctionActionCluster -> CallFunctionResolver`。
- [x] `get/set/get_property/set_property/op/construct/deconstruct/select/control` 已进入 cluster 路由；未迁移的 provider 统一返回 `UnsupportedClusterMigration`。
- [x] 已切断 GraphStatementBuilder 内 first-batch data-flow 的旧直接节点创建路径，不再尝试旧 fallback。
- [ ] Field / Variable / Property / Struct / Select / Control 的真实 `UBlueprintNodeSpawner` provider 尚未迁移完成。
- [ ] 非 call 的 NodeFragment adapter 尚未完成，因此当前只能给出明确诊断，不能虚标为可写入成功。

距离期望差距：当前完成的是统一骨架和旧路径切断；距离完整期望还差各 cluster 的真实 ActionDatabase / BlueprintActionFilter provider、NodeFragment adapter、FragmentDAG emission 和端到端覆盖测试。

## 0. 执行边界

本计划修复的是“统一骨架”，不是完整功能迁移。

本计划必须做到：

1. 新增 `BlueprintActionResolutionCore` 作为统一 action resolution 门面。
2. 新增 `SpawnerClusterResolver` 作为 `AgentFace kind` 到 cluster 的统一入口。
3. 新增四个 cluster 类：`FunctionActionCluster`、`FieldVariableActionCluster`、`EventDelegateActionCluster`、`GenericAssetStructControlActionCluster`。
4. 将现有 `FBlueprintHelperCallFunctionResolver` 包进 `FunctionActionCluster`。
5. 让 `GraphStatementBuilder` 的 `call` 主路径通过 cluster resolver 进入，不再直接以 call resolver 作为架构主入口。
6. 让首批 DataFlowCore kind 能被 cluster resolver 识别，并返回明确的支持状态或尚未迁移诊断。
7. 不新增旧 fallback，不恢复旧 `NodeHandler` / `OperationHandler`。

本计划不做：

1. 不把 `get/set` 立即迁移到 `UBlueprintVariableNodeSpawner`。
2. 不把 `construct/deconstruct/select` 立即完全迁移到 `UBlueprintNodeSpawner` / `UBlueprintAssetNodeSpawner`。
3. 不修改 AgentFace 字段形态。
4. 不新增 schema 兼容旧字段。

## 1. 目标文件结构

### 新增文件

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFunctionActionCluster.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFunctionActionCluster.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFieldVariableActionCluster.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFieldVariableActionCluster.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperEventDelegateActionCluster.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperEventDelegateActionCluster.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionCluster.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionCluster.cpp`

### 修改文件

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.cpp`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_DataFlowCore_FirstBatch_ImplementationPlan_20260521_CN.md`

## 2. 统一类型设计

新增统一请求类型：

```cpp
enum class EBlueprintHelperActionIntent : uint8
{
	Call,
	Get,
	Set,
	GetProperty,
	SetProperty,
	Op,
	Construct,
	Deconstruct,
	Select,
	Event,
	ComponentBoundEvent,
	Bind,
	Control,
	Create,
	Convert,
	Schedule,
	Unknown
};

enum class EBlueprintHelperSpawnerClusterKind : uint8
{
	FunctionAction,
	FieldVariableAction,
	EventDelegateAction,
	GenericAssetStructControlAction,
	Unknown
};

enum class EBlueprintHelperActionResolutionStatus : uint8
{
	Resolved,
	Ambiguous,
	UnsupportedIntent,
	UnsupportedClusterMigration,
	NotFound,
	InvalidRequest
};
```

新增统一请求结构：

```cpp
struct FBlueprintHelperActionResolutionRequest
{
	EBlueprintHelperActionIntent Intent = EBlueprintHelperActionIntent::Unknown;
	UEdGraph* TargetGraph = nullptr;
	FString Query;
	FString StableId;
	FString TargetPath;
	FString PropertyPath;
	FString TypeName;
	TMap<FString, FString> DefaultValues;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
};
```

新增统一结果结构：

```cpp
struct FBlueprintHelperActionResolutionResult
{
	EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	FString Message;
	FString SelectedStableId;
	TWeakObjectPtr<UBlueprintNodeSpawner> SelectedSpawner;
	TWeakObjectPtr<UFunction> SelectedFunction;
	TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateActions;

	bool IsResolved() const
	{
		return Status == EBlueprintHelperActionResolutionStatus::Resolved;
	}
};
```

## 3. Task 1: 新增 `BlueprintActionResolutionCore`

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`

- [ ] **Step 1: 创建 public header**

写入统一类型和 Core class。`FBlueprintHelperActionResolutionCore` 当前只负责通用 envelope、状态转换和调用 cluster resolver 的稳定入口，不直接知道具体 cluster 内部实现。

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shared/GraphWrite/BlueprintHelperCallFunctionCandidateTypes.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

class UBlueprintNodeSpawner;
class UEdGraph;

enum class EBlueprintHelperActionIntent : uint8
{
	Call,
	Get,
	Set,
	GetProperty,
	SetProperty,
	Op,
	Construct,
	Deconstruct,
	Select,
	Event,
	ComponentBoundEvent,
	Bind,
	Control,
	Create,
	Convert,
	Schedule,
	Unknown
};

enum class EBlueprintHelperSpawnerClusterKind : uint8
{
	FunctionAction,
	FieldVariableAction,
	EventDelegateAction,
	GenericAssetStructControlAction,
	Unknown
};

enum class EBlueprintHelperActionResolutionStatus : uint8
{
	Resolved,
	Ambiguous,
	UnsupportedIntent,
	UnsupportedClusterMigration,
	NotFound,
	InvalidRequest
};

struct FBlueprintHelperActionResolutionRequest
{
	EBlueprintHelperActionIntent Intent = EBlueprintHelperActionIntent::Unknown;
	UEdGraph* TargetGraph = nullptr;
	FString Query;
	FString StableId;
	FString TargetPath;
	FString PropertyPath;
	FString TypeName;
	TMap<FString, FString> DefaultValues;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
};

struct FBlueprintHelperActionResolutionResult
{
	EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	FString Message;
	FString SelectedStableId;
	TWeakObjectPtr<UBlueprintNodeSpawner> SelectedSpawner;
	TWeakObjectPtr<UFunction> SelectedFunction;
	TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateActions;

	bool IsResolved() const
	{
		return Status == EBlueprintHelperActionResolutionStatus::Resolved;
	}
};

class BLUEPRINTHELPER_API FBlueprintHelperActionResolutionCore
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);
	static FString IntentToString(EBlueprintHelperActionIntent Intent);
	static FString ClusterKindToString(EBlueprintHelperSpawnerClusterKind ClusterKind);
};
```

- [ ] **Step 2: 创建 cpp**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperActionResolutionCore::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	if (!Request.TargetGraph)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.Message = TEXT("action_resolution_invalid_request:missing_target_graph");
		return Result;
	}

	return FBlueprintHelperSpawnerClusterResolver::Resolve(Request);
}

FString FBlueprintHelperActionResolutionCore::IntentToString(EBlueprintHelperActionIntent Intent)
{
	switch (Intent)
	{
	case EBlueprintHelperActionIntent::Call: return TEXT("call");
	case EBlueprintHelperActionIntent::Get: return TEXT("get");
	case EBlueprintHelperActionIntent::Set: return TEXT("set");
	case EBlueprintHelperActionIntent::GetProperty: return TEXT("get_property");
	case EBlueprintHelperActionIntent::SetProperty: return TEXT("set_property");
	case EBlueprintHelperActionIntent::Op: return TEXT("op");
	case EBlueprintHelperActionIntent::Construct: return TEXT("construct");
	case EBlueprintHelperActionIntent::Deconstruct: return TEXT("deconstruct");
	case EBlueprintHelperActionIntent::Select: return TEXT("select");
	case EBlueprintHelperActionIntent::Event: return TEXT("event");
	case EBlueprintHelperActionIntent::ComponentBoundEvent: return TEXT("component_bound_event");
	case EBlueprintHelperActionIntent::Bind: return TEXT("bind");
	case EBlueprintHelperActionIntent::Control: return TEXT("control");
	case EBlueprintHelperActionIntent::Create: return TEXT("create");
	case EBlueprintHelperActionIntent::Convert: return TEXT("convert");
	case EBlueprintHelperActionIntent::Schedule: return TEXT("schedule");
	default: return TEXT("unknown");
	}
}

FString FBlueprintHelperActionResolutionCore::ClusterKindToString(EBlueprintHelperSpawnerClusterKind ClusterKind)
{
	switch (ClusterKind)
	{
	case EBlueprintHelperSpawnerClusterKind::FunctionAction: return TEXT("FunctionActionCluster");
	case EBlueprintHelperSpawnerClusterKind::FieldVariableAction: return TEXT("FieldVariableActionCluster");
	case EBlueprintHelperSpawnerClusterKind::EventDelegateAction: return TEXT("EventDelegateActionCluster");
	case EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction: return TEXT("GenericAssetStructControlActionCluster");
	default: return TEXT("UnknownCluster");
	}
}
```

## 4. Task 2: 新增四个 Cluster 骨架

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFunctionActionCluster.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFunctionActionCluster.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFieldVariableActionCluster.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFieldVariableActionCluster.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperEventDelegateActionCluster.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperEventDelegateActionCluster.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionCluster.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionCluster.cpp`

- [ ] **Step 1: Function cluster header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperFunctionActionCluster
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);
};
```

- [ ] **Step 2: Function cluster cpp**

`call` 先承接现有 resolver。`op` 先返回明确迁移诊断，避免误以为已符合新架构。

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFunctionActionCluster.h"

#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperFunctionActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;

	if (Request.Intent == EBlueprintHelperActionIntent::Call)
	{
		FBlueprintHelperCallFunctionResolveRequest CallRequest;
		CallRequest.TargetGraph = Request.TargetGraph;
		CallRequest.FunctionQuery = Request.Query;
		CallRequest.DefaultValues = Request.DefaultValues;
		CallRequest.ArgumentPinTypes = Request.ArgumentPinTypes;
		CallRequest.TargetObjectPinType = Request.TargetObjectPinType;
		CallRequest.ExpectedReturnPinType = Request.ExpectedReturnPinType;

		const FBlueprintHelperCallFunctionResolveResult CallResult = FBlueprintHelperCallFunctionResolver::Resolve(CallRequest);
		Result.CandidateActions = CallResult.CandidateFunctions;

		if (CallResult.Status == EBlueprintHelperCallFunctionResolveStatus::Resolved)
		{
			Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
			Result.SelectedStableId = CallResult.Selected.StableId;
			Result.SelectedSpawner = CallResult.Selected.NodeSpawner;
			Result.SelectedFunction = CallResult.Selected.Function;
			Result.Message = TEXT("resolved");
			return Result;
		}

		if (CallResult.Status == EBlueprintHelperCallFunctionResolveStatus::Ambiguous)
		{
			Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
			Result.Message = CallResult.Message;
			return Result;
		}

		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.Message = CallResult.Message;
		return Result;
	}

	if (Request.Intent == EBlueprintHelperActionIntent::Op)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
		Result.Message = TEXT("op_resolution_pending_function_action_cluster_provider");
		return Result;
	}

	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.Message = TEXT("function_action_cluster_unsupported_intent");
	return Result;
}
```

- [ ] **Step 3: 其他 cluster header 采用相同接口**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperFieldVariableActionCluster
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);
};
```

`EventDelegateActionCluster` 与 `GenericAssetStructControlActionCluster` 只替换类名。

- [ ] **Step 4: Field cluster cpp 返回明确迁移诊断**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFieldVariableActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;

	switch (Request.Intent)
	{
	case EBlueprintHelperActionIntent::Get:
	case EBlueprintHelperActionIntent::Set:
	case EBlueprintHelperActionIntent::GetProperty:
	case EBlueprintHelperActionIntent::SetProperty:
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
		Result.Message = TEXT("field_variable_action_cluster_provider_pending");
		return Result;
	default:
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.Message = TEXT("field_variable_action_cluster_unsupported_intent");
		return Result;
	}
}
```

- [ ] **Step 5: Event cluster cpp 返回明确迁移诊断**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperEventDelegateActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;

	switch (Request.Intent)
	{
	case EBlueprintHelperActionIntent::Event:
	case EBlueprintHelperActionIntent::ComponentBoundEvent:
	case EBlueprintHelperActionIntent::Bind:
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
		Result.Message = TEXT("event_delegate_action_cluster_provider_pending");
		return Result;
	default:
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.Message = TEXT("event_delegate_action_cluster_unsupported_intent");
		return Result;
	}
}
```

- [ ] **Step 6: Generic cluster cpp 返回明确迁移诊断**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;

	switch (Request.Intent)
	{
	case EBlueprintHelperActionIntent::Construct:
	case EBlueprintHelperActionIntent::Deconstruct:
	case EBlueprintHelperActionIntent::Select:
	case EBlueprintHelperActionIntent::Control:
	case EBlueprintHelperActionIntent::Create:
	case EBlueprintHelperActionIntent::Convert:
	case EBlueprintHelperActionIntent::Schedule:
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
		Result.Message = TEXT("generic_asset_struct_control_action_cluster_provider_pending");
		return Result;
	default:
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.Message = TEXT("generic_asset_struct_control_action_cluster_unsupported_intent");
		return Result;
	}
}
```

## 5. Task 3: 新增 `SpawnerClusterResolver`

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.cpp`

- [ ] **Step 1: 创建 header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperSpawnerClusterResolver
{
public:
	static EBlueprintHelperSpawnerClusterKind SelectCluster(EBlueprintHelperActionIntent Intent);
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);
};
```

- [ ] **Step 2: 创建 cpp**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperEventDelegateActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFieldVariableActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperFunctionActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionCluster.h"

EBlueprintHelperSpawnerClusterKind FBlueprintHelperSpawnerClusterResolver::SelectCluster(EBlueprintHelperActionIntent Intent)
{
	switch (Intent)
	{
	case EBlueprintHelperActionIntent::Call:
	case EBlueprintHelperActionIntent::Op:
		return EBlueprintHelperSpawnerClusterKind::FunctionAction;
	case EBlueprintHelperActionIntent::Get:
	case EBlueprintHelperActionIntent::Set:
	case EBlueprintHelperActionIntent::GetProperty:
	case EBlueprintHelperActionIntent::SetProperty:
		return EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	case EBlueprintHelperActionIntent::Event:
	case EBlueprintHelperActionIntent::ComponentBoundEvent:
	case EBlueprintHelperActionIntent::Bind:
		return EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	case EBlueprintHelperActionIntent::Construct:
	case EBlueprintHelperActionIntent::Deconstruct:
	case EBlueprintHelperActionIntent::Select:
	case EBlueprintHelperActionIntent::Control:
	case EBlueprintHelperActionIntent::Create:
	case EBlueprintHelperActionIntent::Convert:
	case EBlueprintHelperActionIntent::Schedule:
		return EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	default:
		return EBlueprintHelperSpawnerClusterKind::Unknown;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperSpawnerClusterResolver::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	const EBlueprintHelperSpawnerClusterKind ClusterKind = SelectCluster(Request.Intent);
	switch (ClusterKind)
	{
	case EBlueprintHelperSpawnerClusterKind::FunctionAction:
		return FBlueprintHelperFunctionActionCluster::Resolve(Request);
	case EBlueprintHelperSpawnerClusterKind::FieldVariableAction:
		return FBlueprintHelperFieldVariableActionCluster::Resolve(Request);
	case EBlueprintHelperSpawnerClusterKind::EventDelegateAction:
		return FBlueprintHelperEventDelegateActionCluster::Resolve(Request);
	case EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction:
		return FBlueprintHelperGenericAssetStructControlActionCluster::Resolve(Request);
	default:
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
		Result.Message = TEXT("spawner_cluster_resolver_unknown_intent");
		return Result;
	}
}
```

## 6. Task 4: 将 `call` 主路径接入统一骨架

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`

- [ ] **Step 1: 在 cpp include 新 core**

替换直接依赖时保留 call resolver include 用于 spawn。新增：

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
```

- [ ] **Step 2: 在 `BuildCallFunctionFragment` 中构造统一请求**

将原本直接 `FBlueprintHelperCallFunctionResolver::Resolve(ResolveRequest)` 的语义改为：

```cpp
FBlueprintHelperActionResolutionRequest ActionRequest;
ActionRequest.Intent = EBlueprintHelperActionIntent::Call;
ActionRequest.TargetGraph = TargetGraph;
ActionRequest.Query = NodeData.FunctionName;
ActionRequest.DefaultValues = NodeData.DefaultValues;
ActionRequest.ArgumentPinTypes = NodeData.ArgumentPinTypes;
ActionRequest.TargetObjectPinType = NodeData.TargetObjectPinType;
ActionRequest.ExpectedReturnPinType = NodeData.ExpectedReturnPinType;

const FBlueprintHelperActionResolutionResult ActionResult =
	FBlueprintHelperActionResolutionCore::Resolve(ActionRequest);
```

- [ ] **Step 3: 将统一结果转换回现有 spawn 所需 candidate**

短期内 `FunctionActionCluster` 仍通过 `FBlueprintHelperCallFunctionResolver` 选择 function/spawner，`BuildCallFunctionFragment` 可以继续用 call resolver 的 spawn 函数，但不能再自己做 resolve。实现方式是新增一个局部 helper，将 `ActionResult.SelectedStableId` 反查或让 `FunctionActionCluster` 结果携带兼容 candidate。

首选实现是扩展 `FBlueprintHelperActionResolutionResult`，新增字段：

```cpp
FBlueprintHelperCallFunctionCandidate FunctionCandidate;
```

然后 `FunctionActionCluster` resolved 时填充：

```cpp
Result.FunctionCandidate = CallResult.Selected;
```

`BuildCallFunctionFragment` 使用：

```cpp
if (!ActionResult.IsResolved())
{
	OutError = ActionResult.Message;
	if (OutCandidateFunctions)
	{
		*OutCandidateFunctions = ActionResult.CandidateActions;
	}
	return false;
}

UK2Node* SpawnedNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
	TargetGraph,
	ActionResult.FunctionCandidate,
	FVector2D(NodeData.X, NodeData.Y),
	OutError);
```

## 7. Task 5: 为首批 DataFlowCore kind 建立 cluster-aware 诊断入口

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`

- [ ] **Step 1: 在 `BuildExpressionFragment` 内为 `op/construct/deconstruct/select` 构造统一 action request**

在现有专项 builder 前先调用 core，记录 cluster status。第一轮不能阻断现有功能，但要确保 Debug/错误信息可以说明当前仍走专项 builder。

示例：

```cpp
FBlueprintHelperActionResolutionRequest ActionRequest;
ActionRequest.Intent = EBlueprintHelperActionIntent::Construct;
ActionRequest.TargetGraph = TargetGraph;
ActionRequest.TypeName = Expression.Type;

const FBlueprintHelperActionResolutionResult ActionResult =
	FBlueprintHelperActionResolutionCore::Resolve(ActionRequest);
```

当 `ActionResult.Status == UnsupportedClusterMigration` 时必须直接返回诊断错误，不允许继续当前专项 builder，也不允许保留任何旧 fallback 或过渡执行路径。

- [ ] **Step 2: 在 `BuildVariableSetFragment` 与 `get_property/set_property` 路径添加同样的 cluster-aware 诊断**

`set` 映射：

```cpp
ActionRequest.Intent = EBlueprintHelperActionIntent::Set;
ActionRequest.TargetPath = NodeData.VariableReference.VariableName;
```

`get` 映射：

```cpp
ActionRequest.Intent = EBlueprintHelperActionIntent::Get;
ActionRequest.TargetPath = NodeData.VariableReference.VariableName;
```

`set_property` 映射：

```cpp
ActionRequest.Intent = EBlueprintHelperActionIntent::SetProperty;
ActionRequest.PropertyPath = NodeData.PropertyPath;
```

这一步只建立主入口可见性，不改变节点创建结果。

## 8. Task 6: Facade 口径调整

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.cpp`

- [ ] **Step 1: 新增统一 action resolution facade**

```cpp
static FBlueprintHelperActionResolutionResult ResolveActionForGraph(const FBlueprintHelperActionResolutionRequest& Request);
```

- [ ] **Step 2: cpp 代理到 core**

```cpp
FBlueprintHelperActionResolutionResult FBlueprintGraphWriteFacade::ResolveActionForGraph(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	return FBlueprintHelperActionResolutionCore::Resolve(Request);
}
```

- [ ] **Step 3: 标记旧 facade 方法为非主路径**

`ResolveFunctionForGraph` 暂时保留给旧 pipeline 调用，但注释必须明确：

```cpp
// Compatibility inside current GraphWrite pipeline only. New semantic graph statement code must use ResolveActionForGraph.
```

`FindFunctionByName` 不新增调用点。本计划不删除该函数，因为仍可能被非本轮迁移范围代码调用。

## 9. Task 7: 文档同步

**Files:**

- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_DataFlowCore_FirstBatch_ImplementationPlan_20260521_CN.md`

- [ ] **Step 1: 增加骨架阶段状态**

追加章节：

```markdown
## 骨架修正阶段

当前阶段目标是先使 GraphWrite 主入口符合：

`AgentFace semantic intent -> SpawnerClusterResolver -> BlueprintActionResolutionCore -> Cluster -> NodeSpawner candidate`

本阶段完成后：

1. `call` 通过 `FunctionActionCluster` 包裹现有 resolver。
2. `get/set/get_property/set_property` 进入 `FieldVariableActionCluster` 路由，但具体变量节点创建仍处于后续迁移阶段。
3. `op` 进入 `FunctionActionCluster` 路由，但 typed operator provider 仍处于后续迁移阶段。
4. `construct/deconstruct/select` 进入 `GenericAssetStructControlActionCluster` 路由，但专项 builder 暂时保留为过渡实现。
5. 不恢复旧 `NodeHandler` / `OperationHandler`。
```

## 10. Task 8: 编译验证

**Files:**

- No source edits in this task.

- [ ] **Step 1: 编译插件**

使用当前项目既有 UE 编译入口执行。预期结果：

```text
Compile succeeded
```

- [ ] **Step 2: 如果编译失败，按错误归属修复**

允许修复：

1. include 路径错误。
2. forward declaration 缺失。
3. `BLUEPRINTHELPER_API` 导出遗漏。
4. enum / struct 字段名不一致。

不允许修复：

1. 恢复旧 `NodeHandler`。
2. 绕过 `SpawnerClusterResolver`。
3. 为通过编译删除新骨架入口。

## 11. Task 9: 最小行为验证

**Files:**

- No source edits in this task.

- [ ] **Step 1: 执行一个已有 `call` smoke**

预期：

1. `call` 仍能生成节点。
2. candidate ambiguity 行为不退化。
3. Debug/preview 中不泄漏 `UBlueprintFunctionNodeSpawner` 低层类名给 AgentFace。

- [ ] **Step 2: 执行一个 `set` 或 `set_property` smoke**

预期：

1. 现有节点创建结果不退化。
2. 日志或 Debug evidence 能看到该 intent 已进入 `FieldVariableActionCluster` 路由。
3. 若该 smoke 仍使用旧 direct spawn，文档中保留“具体 NodeSpawner 迁移未完成”的差距说明。

## 12. 完成标准

本计划完成时必须满足：

1. 编译通过。
2. `call` 主路径从 `GraphStatementBuilder` 进入 `BlueprintActionResolutionCore`。
3. `SpawnerClusterResolver` 能识别首批 DataFlowCore kind。
4. 四个 cluster 类都存在，且每个类职责单一。
5. 非 `call` kind 不再被误描述为“已经完成最新架构”，而是能返回明确迁移状态。
6. 未恢复旧 `NodeHandler` / `OperationHandler` / parsed-node fallback。
7. 文档同步说明“骨架已修正”和“具体 NodeSpawner provider 迁移剩余差距”。

## 13. 阻塞处理

若 UE API 无法在某个 cluster 中直接创建期望的 `UBlueprintNodeSpawner`，处理规则：

1. 不绕回旧 fallback。
2. 在 cluster provider 中返回 `UnsupportedClusterMigration`。
3. 在 DebugBundle / preview diagnostics 中返回可行动错误。
4. 在进度文档记录“该 intent 需要专用 provider 或 UE ActionDatabase 适配器”。

## 14. 提交规则

执行完成后不要自动提交。按仓库规则输出建议 commit message 和用户手动执行命令。
