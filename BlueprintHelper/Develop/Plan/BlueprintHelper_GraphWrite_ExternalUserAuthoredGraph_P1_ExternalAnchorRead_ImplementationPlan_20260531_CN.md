# GraphWrite External User-Authored Graph P1 External Anchor Read Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 user-authored graph 建立只读稳定 anchor 契约：read context 输出 external anchor，UE resolver 可验证 GUID、pin 和 fingerprint，stale anchor 在任何 mutation 前失败。

**Architecture:** 新增独立 external anchor types、fingerprint service 和 anchor service。LogicJson 与 `LogicFlow.v1` 只负责展示和传输 anchor，不负责写入。external anchor 与 owned `block_id` resolver 完全分离。

**Tech Stack:** TypeScript、UE 5.6 C++、BlueprintHelper read-context、LogicJson、Automation Tests。
---

## Scope

P1 只提供 read-side capability：

- 输出可被后续 TaskSpec 引用的 `BlueprintHelper.ExternalGraphAnchor.v1`；
- 对 node、pin、exec boundary 生成稳定 fingerprint；
- resolver 可验证 stale anchor；
- 不新增执行 mutation 的 service；
- 不新增 graph write adapter operation。

## Task 1: Define External Anchor Contract

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorFingerprintService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorFingerprintService.cpp`

- [ ] **Step 1: Add pure data types**

```cpp
enum class EBlueprintHelperExternalGraphAnchorRole : uint8
{
	Node,
	ExecBoundary,
	BodyEntry
};

struct FBlueprintHelperExternalGraphAnchor
{
	FString Schema = TEXT("BlueprintHelper.ExternalGraphAnchor.v1");
	FString AssetPath;
	FString GraphName;
	FString NodeGuid;
	FString NodeClass;
	FString PinName;
	FString PinDirection;
	EBlueprintHelperExternalGraphAnchorRole SemanticRole = EBlueprintHelperExternalGraphAnchorRole::Node;
	FString Fingerprint;
};
```

- [ ] **Step 2: Add deterministic fingerprint service**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperExternalGraphAnchorFingerprintService
{
public:
	FString BuildNodeFingerprint(const UEdGraphNode* Node) const;
	FString BuildPinFingerprint(const UEdGraphPin* Pin) const;
	FString BuildExecBoundaryFingerprint(const UEdGraphPin* SourcePin) const;
};
```

Fingerprint input must use deterministic ordered fields:

- node GUID;
- node class path;
- pin name, direction and category;
- sorted linked endpoint GUID and pin-name pairs for exec boundary anchors.

Do not include display title or layout position.

## Task 2: Add Anchor Build and Validate Service

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.cpp`

- [ ] **Step 1: Add service API**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperExternalGraphAnchorService
{
public:
	bool BuildNodeAnchor(const FString& AssetPath, const FString& GraphName, const UEdGraphNode* Node, FBlueprintHelperExternalGraphAnchor& OutAnchor, FString& OutError) const;
	bool BuildExecBoundaryAnchor(const FString& AssetPath, const FString& GraphName, const UEdGraphPin* SourcePin, FBlueprintHelperExternalGraphAnchor& OutAnchor, FString& OutError) const;
};

class BLUEPRINTHELPER_API FBlueprintHelperExternalGraphAnchorResolver
{
public:
	bool ResolveNode(const FBlueprintHelperExternalGraphAnchor& Anchor, UEdGraphNode*& OutNode, FString& OutError) const;
	bool ResolvePin(const FBlueprintHelperExternalGraphAnchor& Anchor, UEdGraphPin*& OutPin, FString& OutError) const;
};
```

- [ ] **Step 2: Enforce stale failure codes**

Use stable errors:

```text
external_anchor_schema_unsupported
external_anchor_graph_not_found
external_anchor_node_not_found
external_anchor_pin_not_found
external_anchor_fingerprint_mismatch
```

## Task 3: Emit Anchors Through Read Context

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintTextConverter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/BlueprintHelperLogicMdTypes.h`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-logic-flow.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.test.ts`

- [ ] **Step 1: Extend UE read JSON**

For each non-owned user-authored node, emit:

```json
{
  "node_ref": "guid-backed-ref",
  "external_anchor": {
    "schema": "BlueprintHelper.ExternalGraphAnchor.v1",
    "asset_path": "/Game/...",
    "graph_name": "EventGraph",
    "node_guid": "...",
    "node_class": "...",
    "semantic_role": "node",
    "fingerprint": "..."
  }
}
```

For each external exec output pin, emit a pin-level boundary anchor.

- [ ] **Step 2: Preserve anchors in `LogicFlow.v1`**

Extend TypeScript read types:

```ts
type LogicFlowNode = {
  ref: string;
  name: string;
  kind?: string;
  externalAnchor?: Record<string, unknown>;
};
```

Return an `anchors` collection alongside rendered flow text. Do not embed unstable display names as locators.

## Task 4: Add RED/GREEN Tests

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperExternalGraphAnchorTests.cpp`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.test.ts`

- [ ] **Step 1: Add UE tests**

Register:

- `BlueprintHelper.GraphWrite.ExternalAnchor.NodeRoundTrip`
- `BlueprintHelper.GraphWrite.ExternalAnchor.ExecBoundaryRoundTrip`
- `BlueprintHelper.GraphWrite.ExternalAnchor.RejectsStaleNode`
- `BlueprintHelper.GraphWrite.ExternalAnchor.RejectsStaleBoundaryLink`
- `BlueprintHelper.GraphWrite.ExternalAnchor.DoesNotUseDisplayName`
- `BlueprintHelper.GraphWrite.ExternalAnchor.DoesNotWriteOwnershipMetadata`

- [ ] **Step 2: Add TypeScript tests**

Assert `buildLogicFlowPayload` returns anchors with schema, GUID and fingerprint and preserves deterministic ordering.

## Task 5: Verification

- [ ] **Step 1: Run focused tests**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ExternalAnchor;Quit' -TestExit='Automation Test Queue Empty'
```

- [ ] **Step 2: Run common gate**

Use the master plan common verification gate.

## Manual Commit Checkpoint

Suggested commit message:

```text
新增内容：
1. 新增 GraphWrite user-authored graph 的只读 external anchor 和 stale validation
2. 在 read context 输出稳定 external anchor
```

