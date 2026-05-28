# 匿名 Namespace 重构为 UBlueprintFunctionLibrary Utils 类 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 BlueprintHelper 插件 81 个非测试 .cpp 文件中的匿名 namespace (`namespace { ... }`) 全部重构为独立的 `UBlueprintFunctionLibrary` utils 类，每个类占用独立的 `.h` + `.cpp` 文件。

**Architecture:** 按子目录域分组，每组创建一个 utils 类。匿名 namespace 中的静态函数移入 utils 类成为 public static 方法。原始 .cpp 文件增加 include 并更新调用点（`FunctionName(...)` → `UUtilsClass::FunctionName(...)`）。Tests 文件中的匿名 namespace 保留不动。

**Tech Stack:** Unreal Engine 5.6 C++, UBlueprintFunctionLibrary, BlueprintHelper 插件

**编码规范参考:** CLAUDE.md 要求不使用匿名 namespace，优先使用 UBlueprintFunctionLibrary。每个 utils 类占用独立 .h + .cpp。

---

## 重构模板

每个域的转换遵循统一模板：

### 原始代码模式
```cpp
// SomeFile.cpp
#include "SomeFile.h"

namespace
{
static ReturnType HelperFunc(Params...) { ... }
static ReturnType AnotherFunc(Params...) { ... }
}

void USomeClass::DoWork()
{
    HelperFunc(...);
    AnotherFunc(...);
}
```

### 重构后代码模式
```cpp
// === 新建: BlueprintHelper<Domain>Utils.h ===
#pragma once
#include "CoreMinimal.h"
#include "BlueprintHelper<Domain>Utils.generated.h"

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelper<Domain>Utils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    static ReturnType HelperFunc(Params...);
    static ReturnType AnotherFunc(Params...);
};

// === 新建: BlueprintHelper<Domain>Utils.cpp ===
#include "BlueprintHelper<Domain>Utils.h"
// 原始需要的 includes...
ReturnType UBlueprintHelper<Domain>Utils::HelperFunc(Params...) { ... }
ReturnType UBlueprintHelper<Domain>Utils::AnotherFunc(Params...) { ... }

// === 修改: SomeFile.cpp ===
#include "SomeFile.h"
#include "BlueprintHelper<Domain>Utils.h"  // 新增 include
// 删除整个 namespace { ... } 块

void USomeClass::DoWork()
{
    UBlueprintHelper<Domain>Utils::HelperFunc(...);    // 更新调用点
    UBlueprintHelper<Domain>Utils::AnotherFunc(...);   // 更新调用点
}
```

### 函数内互相调用处理
如果匿名 namespace 内函数 A 调用函数 B，移动后改为 `UUtilsClass::B(...)`。

### include 处理
- 新增 utils 类的 .cpp 文件需要包含原始函数所用到的所有头文件
- 原始 .cpp 文件增加 `#include "Path/To/BlueprintHelper<Domain>Utils.h"`

---

## 域分组与批次划分

### 批次 1 (Pilot): Entry/Bridge (2 files)
- **Utils 类:** `UBlueprintHelperBridgeUtils`
- **路径:** `BlueprintHelper/Private/Entry/Bridge/Utils/`
- BlueprintHelperBridgeServer.cpp (1 function: `BlueprintHelperBridgeConfigWithPort`)
- BlueprintHelperBridgeRouter.cpp (8 functions: `MakeToolResultBridgeResponse`, `ParseBridgeTargetType`, etc.)

### 批次 2: Systems/Config (3 files)
- **Utils 类:** `UBlueprintHelperConfigUtils`
- BlueprintHelperRuntimeSettingResolver.cpp (~10 functions)
- BlueprintHelperSafetyProfileResolver.cpp
- BlueprintHelperSettingStore.cpp

### 批次 3: Systems/Debug (2 files)
- **Utils 类:** `UBlueprintHelperDebugUtils`
- BlueprintHelperAssetBrowseService.cpp
- BlueprintHelperDebugEntryService.cpp

### 批次 4: Systems/Authorization (1 file)
- **Utils 类:** `UBlueprintHelperAuthorizationUtils`
- BlueprintHelperWriteAuthorizationService.cpp

### 批次 5: Systems/Review (6 files, Review + Review/Utils)
- **Utils 类:** `UBlueprintHelperReviewUtils`
- BlueprintHelperReviewConfigResolver.cpp
- BlueprintHelperReviewStoreService.cpp
- BlueprintHelperReviewBaselineSnapshotService.cpp
- BlueprintHelperReviewStoreMergeUtils.cpp
- BlueprintHelperReviewRejectService.cpp
- BlueprintHelperReviewBaselineSnapshotServiceUtils.cpp

### 批次 6: UI Domain (7 files)
- **Utils 类:** `UBlueprintHelperReviewUIUtils` (Review UI + Review/Utils)
- **Utils 类:** `UBlueprintHelperSettingsUIUtils` (Settings)
- BlueprintHelperReviewSurfaceFrameGeometryUtils.cpp
- BlueprintHelperReviewSurfaceFrameWidgetUtils.cpp
- BlueprintHelperReviewSurfaceFrameBuilder.cpp
- BlueprintHelperReviewPanelSettingsResolver.cpp
- BlueprintHelperReviewDiffBlockNode.cpp
- BlueprintHelperReviewGraphBoundsUtils.cpp (Utils)
- BlueprintHelperSettingsPresenter.cpp

### 批次 7: Misc 分散小域 (4 files)
- **Utils 类:** `UBlueprintHelperBlueprintStructureUtils` → SharedServices
- **Utils 类:** `UBlueprintHelperClassSettingsUtils` → BlueprintClassSettings
- **Utils 类:** `UBlueprintHelperTaskRuntimeUtils` → TaskRuntime
- BlueprintHelperBlueprintStructureService.cpp
- BlueprintHelperClassSettingsService.cpp
- BlueprintHelperTaskRuntimeSettingsResolver.cpp
- BlueprintHelperTaskSpecWorkbenchServices.cpp

### 批次 8: GraphWrite/Root (4 files)
- **Utils 类:** `UGraphWriteCoreUtils`
- BlueprintHelperReplaceEntryResolver.cpp
- BlueprintHelperMergeBlueprintGraphService.cpp
- BlueprintHelperMergeCallableFragmentService.cpp
- BlueprintHelperGraphWriteMutationCoordinator.cpp

### 批次 9: GraphWrite/GraphStatement (14 files, 含 Utils 子目录)
- **Utils 类:** `UGraphWriteGraphStatementUtils`
- BlueprintHelperActionFragmentSpawnCoordinator.cpp
- BlueprintHelperGraphSemanticIR.cpp
- BlueprintHelperEventDelegateFragmentBuilder.cpp
- BlueprintHelperFieldFragmentBuilder.cpp
- BlueprintHelperSelectFragmentBuilder.cpp
- BlueprintHelperGraphFragmentBuilderRegistry.cpp
- BlueprintHelperGraphFragmentDag.cpp
- BlueprintHelperDelegateLinkFragmentUtils.cpp
- BlueprintHelperEventDelegateBindingObjectResolver.cpp
- BlueprintHelperControlFragmentBuilder.cpp
- BlueprintHelperActionFragmentBuildUtils.cpp
- BlueprintHelperGraphComposer.cpp
- BlueprintHelperGraphStatementPinTypeParser.cpp (Utils)
- BlueprintHelperGraphEventReferenceUtils.cpp (Utils)

### 批次 10: GraphWrite/ActionResolution Group A - Evidence & Policy (8 files)
- **Utils 类:** `UGraphWriteActionEvidenceUtils`
- BlueprintHelperGenericOpsEvidence.cpp
- BlueprintHelperOpCallableEvidence.cpp
- BlueprintHelperProjectedSpawnerEvidence.cpp
- BlueprintHelperEventDelegatePolicy.cpp
- BlueprintHelperEventDelegateUseSiteEvidence.cpp
- BlueprintHelperArrayTypedPinEvidenceGuard.cpp
- BlueprintHelperFieldCapabilityTypes.cpp
- BlueprintHelperContainerActionVocabulary.cpp

### 批次 11: GraphWrite/ActionResolution Group B - Context & Pipeline (5 files)
- **Utils 类:** `UGraphWriteActionContextUtils`
- BlueprintHelperActionContextDemandCollector.cpp
- BlueprintHelperActionContextBundleProjector.cpp
- BlueprintHelperActionContextInferenceService.cpp
- BlueprintHelperActionContextSnapshotBuilder.cpp
- BlueprintHelperGraphWriteProjectedEvidenceQueryService.cpp

### 批次 12: GraphWrite/ActionResolution Group C - Resolvers Part 1 (10 files)
- **Utils 类:** `UGraphWriteActionResolverUtils`
- BlueprintHelperGenericActionProviderBoundary.cpp ← 刚修复的编译错误在此
- BlueprintHelperFieldVariableActionResolver.cpp
- BlueprintHelperFieldPathResolution.cpp
- BlueprintHelperGenericAssetStructControlActionResolver.cpp
- BlueprintHelperFunctionSemanticActionResolver.cpp
- BlueprintHelperGenericCreateActionResolver.cpp
- BlueprintHelperGenericTransformScheduleActionResolver.cpp
- BlueprintHelperSingletonControlFlowEvidenceProvider.cpp
- BlueprintHelperStructTypeStructureActionResolver.cpp
- BlueprintHelperTypePromotionSpawnerEvidenceResolver.cpp

### 批次 13: GraphWrite/ActionResolution Group D - Resolvers Part 2 (8 files)
- **Utils 类:** `UGraphWriteActionClusterUtils`
- BlueprintHelperGenericAssetStructControlActionCluster.cpp
- BlueprintHelperEventDelegateActionCluster.cpp
- BlueprintHelperContainerActionResolver.cpp
- BlueprintHelperGenericAssetActionResolver.cpp
- BlueprintHelperOperatorActionResolver.cpp
- BlueprintHelperFieldActionReadback.cpp
- BlueprintHelperActionResolutionCore.cpp
- BlueprintHelperOpCallableCatalog.cpp

### 批次 14: GraphWrite/ActionResolution Group E - Adapters & Services (4 files)
- **Utils 类:** `UGraphWriteActionAdapterUtils`
- BlueprintHelperGenericTransformSpawnerFactory.cpp
- BlueprintHelperAssetActionProjectionService.cpp
- BlueprintHelperActionDatabaseProjectionService.cpp
- BlueprintHelperActionNodeSpawnerAdapter.cpp

### 批次 15: GraphWrite/Remaining (6 files)
- **Utils 类:** `UGraphWritePipelineUtils` (Pipeline: 2)
- **Utils 类:** `UGraphWriteTestingUtils` (Testing: 3)
- **Utils 类:** `UGraphWriteReadbackUtils` (Readback: 1)
- **Utils 类:** `UGraphWriteFunctionResolutionUtils` (FunctionResolution/Utils: 1)
- **Utils 类:** `UGraphWriteCapabilityMetricsUtils` (Testing/Capability: 1, 如有)

---

## 任务执行

### Task 1 (Pilot): 重构 Entry/Bridge 域 → UBlueprintHelperBridgeUtils

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeUtils.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeServer.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`

- [ ] **Step 1: 创建 UBlueprintHelperBridgeUtils.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintHelperBridgeUtils.generated.h"

struct FBlueprintHelperBridgeRequest;
struct FBlueprintHelperBridgeResponse;
struct FBlueprintHelperToolResultBase;
enum class EBlueprintHelperTargetType : uint8;
struct FBlueprintHelperTargetRef;
struct FBlueprintHelperBridgeRuntimeConfig;
struct FBlueprintHelperReviewActionResult;

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperBridgeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FBlueprintHelperBridgeRuntimeConfig BridgeConfigWithPort(int32 InPort);

	static FBlueprintHelperBridgeResponse MakeToolResultBridgeResponse(
		const FBlueprintHelperBridgeRequest& Req,
		const FBlueprintHelperToolResultBase& Result);

	static EBlueprintHelperTargetType ParseBridgeTargetType(const FString& Type);
	static EBlueprintHelperTargetType ParseLogicScopeTargetType(const FString& Scope);
	static EBlueprintHelperTargetType InferTargetTypeFromReadFields(const FBlueprintHelperTargetRef& Target);
	static void ApplyTargetNameToTypedField(FBlueprintHelperTargetRef& Target, const FString& TargetName);
	static FBlueprintHelperTargetRef ReadTargetRefFromPayload(const TSharedPtr<FJsonObject>& Payload);
	static TArray<FString> ReadStringArrayField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName);
	static TSharedRef<FJsonObject> ReviewActionResultToJson(const FBlueprintHelperReviewActionResult& Result);
};
```

- [ ] **Step 2: 创建 UBlueprintHelperBridgeUtils.cpp**

将 BlueprintHelperBridgeServer.cpp 中 `BlueprintHelperBridgeConfigWithPort` 和 BlueprintHelperBridgeRouter.cpp 中 8 个函数移入此文件，加上必要的 includes。

- [ ] **Step 3: 修改 BlueprintHelperBridgeServer.cpp**

删除 `namespace { ... }` 块（lines 17-25），新增 `#include "Entry/Bridge/Utils/BlueprintHelperBridgeUtils.h"`，将 `BlueprintHelperBridgeConfigWithPort(InPort)` 改为 `UBlueprintHelperBridgeUtils::BridgeConfigWithPort(InPort)`。

- [ ] **Step 4: 修改 BlueprintHelperBridgeRouter.cpp**

删除 `namespace { ... }` 块（lines 939-1099），新增 include，更新 8 个函数的调用点。

- [ ] **Step 5: 编译验证**

```bash
F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -FromMsBuild
```

- [ ] **Step 6: 提交**

```bash
git add ...files...
git commit -m "refactor: 将 Entry/Bridge 匿名 namespace 提取为 UBlueprintHelperBridgeUtils"
```

---

### Task 2-15: 剩余批次

每个批次遵循与 Task 1 相同的模式：
1. 创建 Utils .h / .cpp 文件
2. 逐个修改域内 .cpp 文件（删除 namespace 块，新增 include，更新调用点）
3. 编译验证
4. 提交

每个批次独立，可并行执行（但建议串行以避免冲突）。

---

## 自检 (Self-Review)

1. **Spec coverage:** 81 个非测试文件按域分为 15 个批次，覆盖全部匿名 namespace。Tests 文件保留不动。
2. **Placeholder scan:** 无 TBD/TODO，Batch 1 有完整代码模板。
3. **Type consistency:** Utils 类方法签名与原始 static 函数签名完全一致，仅增加类前缀。
