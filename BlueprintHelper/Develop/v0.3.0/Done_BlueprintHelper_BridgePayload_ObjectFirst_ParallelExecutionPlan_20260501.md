# BlueprintHelper Bridge Payload Object-First Parallel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 BlueprintHelper 的 RawJson/LogicJson/Import 链路从 string-first 改成 object-first，并把 MCP 默认 RawJson 暴露方式保持为 resource-first。

**Architecture:** C++ 侧先建立结构化 JSON contract，再让 Converter、ExportService、LogicProcessor、ImportService 各自支持 object 输入输出，最后由 BridgeRouter 统一对外协议。MCP 侧继续使用 `structuredContent` 和 `resource_link`，补齐对 `payload`、`json` object、legacy `json` string、`json_text` 的兼容。

**Tech Stack:** Unreal Engine 5.6 editor plugin C++、UE JSON DOM、Automation Tests、TypeScript MCP server、Zod、Node test runner。

---

## 1. 输入与边界

本计划来自：

- `G:\edgeDownloads\BlueprintHelper_BridgePayload_ObjectFirst_OptimizationPlan_20260501.md`
- `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\AgentGuide\00_Agent_Onboarding_Index_20260430.md`
- `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\Plan\MCP返回结构优化_TechSpec_20260430.md`
- 当前源码状态：
  - BlueprintHelper 插件：`G:\UnrealPractise\MrStone\Plugins\BlueprintHelper`
  - MCP Server：`G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server`

执行边界：

- 用普通代码工具修改 C++、TypeScript、文档和测试。
- 不用 BlueprintHelper MCP 搜索或编辑源码。
- Bridge/MCP 工具只用于后续读写 Unreal Editor 资产的人工验证。
- `BlueprintHelperBridgeRouter.cpp` 是集成瓶颈文件，同一时间只分配给一个 worker。
- `BlueprintHelperServiceTypes.h` 是 contract 瓶颈文件，先由 Contract worker 完成后再放开依赖任务。

## 2. 当前代码地图

BlueprintHelper C++：

- `Source/BlueprintHelper/Public/Services/BlueprintHelperServiceTypes.h`
  - 当前 `FBlueprintHelperExportResult` 只有 `FString JsonText`。
  - 当前 `FBlueprintHelperImportRequest` 只有 `FString JsonText`。
- `Source/BlueprintHelper/Public/BlueprintTextConverter.h`
- `Source/BlueprintHelper/Private/BlueprintTextConverter.cpp`
  - 当前 `ConvertGraphToJson()` 和 `ExportBlueprintToJson()` 直接返回字符串。
  - 内部已经先组装 `FJsonObject`，再 serialize。
- `Source/BlueprintHelper/Public/Services/BlueprintHelperExportService.h`
- `Source/BlueprintHelper/Private/Services/BlueprintHelperExportService.cpp`
  - 当前默认调用字符串导出 API。
- `Source/BlueprintHelper/Public/Services/BlueprintHelperLogicProcessor.h`
- `Source/BlueprintHelper/Private/Services/BlueprintHelperLogicProcessor.cpp`
  - 当前 `ProcessRawJson(const FString&)` 内部 parse 字符串。
- `Source/BlueprintHelper/Public/Services/BlueprintHelperImportService.h`
- `Source/BlueprintHelper/Private/Services/BlueprintHelperImportService.cpp`
  - 当前 Import 只消费字符串。
- `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`
  - 当前 `HandleExportToJson()` 写 `result.json` string。
  - 当前 `HandleExportLogic()` 使用 `ExportResult.JsonText`。
  - 当前 `HandleImportJson()` 要求 `payload.json` string。
- `Source/BlueprintHelper/Private/Bridge/BlueprintHelperRequestValidator.cpp`
  - 当前 `export_to_json` 只接受 `scope` 额外字段。
  - 当前 `import_json` 需要 `json` string。
- `Source/BlueprintHelper/Private/Tests/BlueprintHelperSafetyTests.cpp`
  - 当前已有安全回归测试，可新增独立 object-first 测试文件避免冲突。

MCP Server：

- `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\mcp-response.ts`
  - 已有 `normalizeBlueprintPayload()`、resource URI helper 和 `buildBlueprintToolResult()`。
- `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\resources.ts`
  - 已有 `blueprint://asset/...` resource handler。
  - 当前 raw-json resource 仍可能返回 Bridge 包裹对象，需要直接返回 RawJson 本体。
- `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\tools.ts`
  - `blueprint_export_to_json` 已默认返回 RawJson resource link。
  - `blueprint_import_json_to_graph` 当前 `json` schema 是 string。
- `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\tools.regression.test.ts`
  - 已覆盖 resource link、legacy text、scope、logic structured 输出。
- `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\package.json`
  - 当前 `npm test` 只运行 `build/tools.regression.test.js`。

## 3. 目标协议

Bridge `export_to_json` 默认响应：

```json
{
  "success": true,
  "request_id": "req_1",
  "result": {
    "format": "raw_json",
    "schema": "BlueprintHelper.JsonToBlueprint.v2.2",
    "assetPath": "/Game/BP/BP_Test.BP_Test",
    "graph": "EventGraph",
    "effective_scope": "graph",
    "importable": true,
    "payload": {
      "version": "2.2",
      "schema": "BlueprintHelper.JsonToBlueprint",
      "nodes": [],
      "links": []
    },
    "json": {
      "version": "2.2",
      "schema": "BlueprintHelper.JsonToBlueprint",
      "nodes": [],
      "links": []
    },
    "stats": {
      "nodes": 0,
      "links": 0
    },
    "diagnostics": []
  }
}
```

Bridge `export_to_json` legacy/debug 响应只在请求 `include_json_text: true` 时额外包含：

```json
{
  "json_text": "{\"version\":\"2.2\",\"schema\":\"BlueprintHelper.JsonToBlueprint\",\"nodes\":[],\"links\":[]}"
}
```

Bridge `export_logic` 默认响应：

```json
{
  "success": true,
  "result": {
    "format": "logic_json",
    "schema": "BlueprintHelper.LogicJson.v1",
    "importable": false,
    "payload": {
      "version": "1.0",
      "schema": "BlueprintHelper.LogicGraph",
      "graphs": [],
      "stats": {}
    },
    "stats": {},
    "diagnostics": []
  }
}
```

MCP RawJson resource 内容直接是 RawJson 本体：

```json
{
  "version": "2.2",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "nodes": [],
  "links": []
}
```

Import 接受两种等价输入：

```json
{
  "json": {
    "version": "2.2",
    "schema": "BlueprintHelper.JsonToBlueprint",
    "nodes": [],
    "links": []
  }
}
```

```json
{
  "json": "{\"version\":\"2.2\",\"schema\":\"BlueprintHelper.JsonToBlueprint\",\"nodes\":[],\"links\":[]}"
}
```

Import 必须拒绝：

- `schema` 以 `BlueprintHelper.Logic` 开头的对象。
- `importable: false` 的对象。
- 缺少 RawJson schema 且同时缺少 `nodes`/`links` 或 `graphs`/`blueprint_operations` 的对象。

## 4. 完全并行度总览

最大可用并行度按依赖波次组织。每个 worker 都有独占写入范围；跨 worker 需要的公共接口只通过前置 contract 任务发布。

```text
Wave 0:
  C0 C++ Contract
  M0 MCP Test Harness
  B0 Baseline Fixtures And Metrics
  S0 Snapshot IR Design Spike

Wave 1:
  C1 Converter And ExportService Object Path      depends on C0
  C2 LogicProcessor Object Input                  independent after source read
  C3 ImportService Object Input                   depends on C0
  M1 MCP Response And Resource Normalization      depends on M0
  M2 MCP Tool Schema Object Import                depends on M0

Wave 2:
  C4 BridgeRouter Protocol Integration            depends on C0, C1, C2, C3
  M3 MCP Protocol Regression Consolidation        depends on M1, M2

Wave 3:
  I1 End-To-End Verification                      depends on C4, M3
  D1 Documentation And Migration Notes            depends on C4, M3
```

Parallel worker table:

| Worker | Main responsibility | Writes | Depends on | Can run with |
|---|---|---|---|---|
| C0 | Shared C++ request/result contract | `BlueprintHelperServiceTypes.h`, `BlueprintHelperRequestValidator.cpp`, new contract test file | none | M0, B0, S0 |
| M0 | MCP test runner split | `package.json`, optional new MCP test helper file | none | C0, B0, S0 |
| B0 | Baseline fixtures and metrics plan data | `Resources/TestFixtures/BridgePayloadObjectFirst/*`, `Resources/Plan/*Baseline*.md` | none | C0, M0, S0 |
| S0 | Snapshot IR design spike | `Resources/Plan/*SnapshotIR*.md`, optional header-only sketch under new path | none | C0, M0, B0, Wave 1 |
| C1 | Converter and ExportService object-first output | `BlueprintTextConverter.h/.cpp`, `BlueprintHelperExportService.cpp`, new C++ test file | C0 | C2, C3, M1, M2 |
| C2 | LogicProcessor object-first input | `BlueprintHelperLogicProcessor.h/.cpp`, new C++ test file | none | C1, C3, M1, M2 |
| C3 | ImportService object input | `BlueprintHelperImportService.h/.cpp`, new C++ test file | C0 | C1, C2, M1, M2 |
| M1 | MCP normalize/resource direct RawJson body | `mcp-response.ts`, `resources.ts`, new MCP test file | M0 | C1, C2, C3, M2 |
| M2 | MCP import schema and object forwarding | `tools.ts`, new MCP test file | M0 | C1, C2, C3, M1 |
| C4 | BridgeRouter unified protocol | `BlueprintHelperBridgeRouter.cpp`, `BlueprintHelperRequestValidator.cpp`, new bridge test file | C0, C1, C2, C3 | M3 |
| M3 | MCP full regression consolidation | MCP regression test files | M1, M2 | C4 |
| I1 | Build, automation, MCP, live Bridge validation | no source writes | C4, M3 | D1 |
| D1 | Docs and migration notes | `README.md`, `Resources/AgentGuide/*`, `Resources/Plan/*` | C4, M3 | I1 |

## 5. Wave 0 Tasks

### Task C0: C++ Contract

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Public\Services\BlueprintHelperServiceTypes.h`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Bridge\BlueprintHelperRequestValidator.cpp`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Tests\BlueprintHelperObjectFirstContractTests.cpp`

**Purpose:** Publish the shared types and request validator rules that unlock C1, C3, and C4.

- [ ] Add a forward declaration above service structs:

```cpp
class FJsonObject;
```

- [ ] Extend export request/result:

```cpp
struct FBlueprintHelperExportRequest
{
	FBlueprintHelperGraphTarget Target;
	EBlueprintHelperExportScope Scope = EBlueprintHelperExportScope::SingleGraph;
	bool bIncludeJsonText = false;
};

struct FBlueprintHelperExportResult
{
	bool bSuccess = false;
	TSharedPtr<FJsonObject> JsonObject;
	FString JsonText;
	FString EffectiveScope;
	FBlueprintHelperDiagnosticSet Diagnostics;
};
```

- [ ] Extend import request:

```cpp
struct FBlueprintHelperImportRequest
{
	FBlueprintHelperGraphTarget Target;
	TSharedPtr<FJsonObject> JsonObject;
	FString JsonText;
	bool bAutoCompile = false;
	bool bStrict = true;
	bool bAllowPartial = false;
};
```

- [ ] Update request validation:
  - `export_to_json` accepts optional `include_json_text` bool.
  - `import_json` accepts `json` as string or object.
  - `validate_json` remains string-only for this plan unless a later task explicitly extends it.

- [ ] Add tests:
  - `export_to_json` accepts `include_json_text: true`.
  - `import_json` accepts object `json`.
  - `import_json` accepts string `json`.
  - `import_json` rejects array `json`.

Verification:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

Automation target:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst.Contract;Quit'
```

### Task M0: MCP Test Harness

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\package.json`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\test-harness.ts`

**Purpose:** Allow M1 and M2 to add independent test files without editing the same large regression file.

- [ ] Change test script:

```json
{
  "scripts": {
    "test": "npm run build && node --test build/*.test.js"
  }
}
```

- [ ] Move reusable fake MCP server helpers from `tools.regression.test.ts` into `src/test-harness.ts`:
  - `registerWithBridge`
  - `registerResourcesWithBridge`
  - `invokeTool`
  - `withConnectedMcpServer`

- [ ] Keep existing `tools.regression.test.ts` passing by importing the helpers.

Verification:

```powershell
Set-Location 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper_MCP_Server'
npm test
```

### Task B0: Baseline Fixtures And Metrics

**Files:**

- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\TestFixtures\BridgePayloadObjectFirst\README.md`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\TestFixtures\BridgePayloadObjectFirst\small_raw_json_legacy_response.json`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\TestFixtures\BridgePayloadObjectFirst\small_raw_json_payload_response.json`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\Plan\BridgePayload_ObjectFirst_BaselineMetrics_20260501.md`

**Purpose:** Record before/after fixture shape and metric definitions without blocking code work.

- [ ] Create a small graph fixture with `nodes`, `links`, `version`, `schema`.
- [ ] Create a legacy Bridge response fixture where `result.json` is a string.
- [ ] Create a target Bridge response fixture where `result.payload` and `result.json` are objects.
- [ ] Document the metrics:
  - Bridge response byte count.
  - Count of escaped quote sequences in `result.json`.
  - RawJson node count.
  - RawJson link count.
  - MCP default output `content` item count.

Verification:

```powershell
Get-Content 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper/Resources/TestFixtures/BridgePayloadObjectFirst/small_raw_json_payload_response.json' | ConvertFrom-Json | Out-Null
```

### Task S0: Snapshot IR Design Spike

**Files:**

- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\Plan\BridgePayload_ObjectFirst_SnapshotIR_DesignSpike_20260501.md`

**Purpose:** Keep Snapshot IR out of the short-term object-first critical path while preserving design decisions for Phase 6.

- [ ] Define `FBlueprintHelperPinSnapshot`, `FBlueprintHelperNodeSnapshot`, `FBlueprintHelperLinkSnapshot`, `FBlueprintHelperGraphSnapshot`, `FBlueprintHelperBlueprintSnapshot`.
- [ ] Define serializer boundaries:
  - RawJson serializer consumes Snapshot.
  - LogicJson serializer consumes Snapshot.
  - LogicMarkdown serializer consumes Snapshot.
- [ ] State that Snapshot IR does not block Tasks C1 through M3.
- [ ] State the first implementation target as graph-only snapshot after object-first protocol ships.

Verification:

```powershell
Select-String -Path 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper/Resources/Plan/BridgePayload_ObjectFirst_SnapshotIR_DesignSpike_20260501.md' -Pattern 'FBlueprintHelperGraphSnapshot','RawJson serializer','does not block'
```

## 6. Wave 1 Tasks

### Task C1: Converter And ExportService Object Path

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Public\BlueprintTextConverter.h`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\BlueprintTextConverter.cpp`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Services\BlueprintHelperExportService.cpp`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Tests\BlueprintHelperObjectFirstExportTests.cpp`

**Purpose:** Make export internally produce `FJsonObject` first and generate string text only on request.

- [ ] Add converter APIs:

```cpp
static TSharedPtr<class FJsonObject> ConvertGraphToJsonObject(class UEdGraph* TargetGraph);
static TSharedPtr<class FJsonObject> ExportBlueprintToJsonObject(class UBlueprint* Blueprint);
static FString SerializeJsonObject(const TSharedPtr<class FJsonObject>& JsonObject);
```

- [ ] Refactor existing string APIs into wrappers:

```cpp
FString FBlueprintToTextConverter::ConvertGraphToJson(UEdGraph* TargetGraph)
{
	return SerializeJsonObject(ConvertGraphToJsonObject(TargetGraph));
}

FString FBlueprintToTextConverter::ExportBlueprintToJson(UBlueprint* Blueprint)
{
	return SerializeJsonObject(ExportBlueprintToJsonObject(Blueprint));
}
```

- [ ] Move existing root-object creation from `ConvertGraphToJson()` into `ConvertGraphToJsonObject()`.
- [ ] Move existing root-object creation from `ExportBlueprintToJson()` into `ExportBlueprintToJsonObject()`.
- [ ] Update `FBlueprintHelperExportService::Export()`:
  - Fill `Result.JsonObject` by default.
  - Fill `Result.JsonText` only when `Request.bIncludeJsonText` is true.
  - Preserve `EffectiveScope`.
  - Preserve diagnostics for failed graph or blueprint resolution.

- [ ] Add tests:
  - `ConvertGraphToJsonObject()` returns object with `version`, `schema`, `nodes`, `links`.
  - `ConvertGraphToJson()` serializes the same object shape as before.
  - `ExportService.Export()` returns valid `JsonObject` with empty `JsonText` when `bIncludeJsonText=false`.
  - `ExportService.Export()` returns both object and text when `bIncludeJsonText=true`.

Verification:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

Automation target:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst.Export;Quit'
```

### Task C2: LogicProcessor Object Input

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Public\Services\BlueprintHelperLogicProcessor.h`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Services\BlueprintHelperLogicProcessor.cpp`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Tests\BlueprintHelperObjectFirstLogicTests.cpp`

**Purpose:** Let logic view generation consume RawJson object directly.

- [ ] Add object-first API:

```cpp
static FBlueprintHelperLogicResult ProcessRawJsonObject(
	const TSharedPtr<FJsonObject>& RawJsonObject,
	const FBlueprintHelperLogicOptions& Options);
```

- [ ] Convert current parse body into `ProcessRawJsonObject()`.
- [ ] Keep `ProcessRawJson(const FString&)` as wrapper:

```cpp
FBlueprintHelperLogicResult FBlueprintHelperLogicProcessor::ProcessRawJson(
	const FString& RawJsonText,
	const FBlueprintHelperLogicOptions& Options)
{
	FBlueprintHelperLogicResult Result;
	if (RawJsonText.TrimStartAndEnd().IsEmpty())
	{
		Result.ErrorMessage = TEXT("JSON parse failed: input is empty.");
		return Result;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		Result.ErrorMessage = TEXT("JSON parse failed: input is not a valid JSON object.");
		return Result;
	}

	return ProcessRawJsonObject(RootObject, Options);
}
```

- [ ] Add tests:
  - String wrapper and object API produce identical LogicJson stats.
  - String wrapper and object API produce identical Markdown node/link counts.
  - Invalid object returns `bSuccess=false` with a non-empty error message.

Verification:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

Automation target:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst.Logic;Quit'
```

### Task C3: ImportService Object Input

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Public\Services\BlueprintHelperImportService.h`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Services\BlueprintHelperImportService.cpp`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Tests\BlueprintHelperObjectFirstImportTests.cpp`

**Purpose:** Support object RawJson import while keeping string import intact.

- [ ] Add private helper:

```cpp
FString ResolveImportJsonText(const FBlueprintHelperImportRequest& Request, FBlueprintHelperImportResult& Result) const;
```

- [ ] Serialize `Request.JsonObject` only at the ImportService boundary because `TextToBlueprintGenerator` currently consumes string JSON.
- [ ] Keep existing `Validator.Validate(JsonText)` behavior.
- [ ] Add schema/importable guard before generator execution:
  - Reject `schema` starting with `BlueprintHelper.Logic`.
  - Reject explicit `importable=false`.
  - Accept `BlueprintHelper.JsonToBlueprint`.
  - Accept legacy objects with `nodes`/`links`.
  - Accept multi-graph objects with `graphs` or `blueprint_operations`.

- [ ] Add tests:
  - Object RawJson imports through the same validation path as string RawJson.
  - Legacy string RawJson remains accepted.
  - LogicJson object is rejected.
  - `importable=false` object is rejected.

Verification:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

Automation target:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst.Import;Quit'
```

### Task M1: MCP Response And Resource Normalization

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\mcp-response.ts`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\resources.ts`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\mcp-response.object-first.test.ts`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\resources.object-first.test.ts`

**Purpose:** Normalize all Bridge payload variants and make raw-json resources return RawJson body directly.

- [ ] Extend normalization to prefer `payload`:

```ts
export function getBlueprintPayloadBody(result: unknown): unknown {
  const normalized = normalizeBlueprintPayload(result);
  if (!isRecord(normalized)) return normalized;
  if (normalized['payload'] !== undefined) return normalized['payload'];
  if (normalized['json'] !== undefined) return normalized['json'];
  if (normalized['json_text'] !== undefined) return normalizeBridgeResult(normalized['json_text']);
  return normalized;
}
```

- [ ] Update `normalizeBlueprintPayload()`:
  - Parse string `json`.
  - Parse string `json_text`.
  - Preserve object `payload`.
  - Do not replace `payload` with `json`.

- [ ] Update raw-json resource handler:
  - Call `export_to_json`.
  - Normalize response.
  - Return `getBlueprintPayloadBody(resp.result)` as JSON text.
  - Do not wrap RawJson as `{ "json": ... }`.

- [ ] Add tests:
  - Payload object wins over `json` string.
  - Object `json` is accepted.
  - Legacy string `json` is parsed.
  - Legacy `json_text` is parsed.
  - RawJson resource returns `{"version":"2.2","schema":"BlueprintHelper.JsonToBlueprint","nodes":[],"links":[]}` directly.

Verification:

```powershell
Set-Location 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper_MCP_Server'
npm test
```

### Task M2: MCP Tool Schema Object Import

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\tools.ts`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\tools.import-object.test.ts`

**Purpose:** Let `blueprint_import_json_to_graph` pass structured RawJson objects without stringifying at the MCP layer.

- [ ] Add RawJson input schema:

```ts
const rawJsonInputSchema = z.union([
  z.string(),
  z.record(z.unknown()),
]);
```

- [ ] Change import tool input:

```ts
json: rawJsonInputSchema.describe('The BlueprintHelper RawJson object or legacy JSON string to import')
```

- [ ] Forward `json` unchanged:

```ts
const payload: Record<string, unknown> = {
  json,
  compile_after_import,
  strict: strict ?? true,
  allow_partial: allow_partial ?? false,
};
```

- [ ] Reject obvious read-only views in MCP before Bridge call:

```ts
if (isRecord(json)) {
  const schema = typeof json['schema'] === 'string' ? json['schema'] : '';
  if (schema.startsWith('BlueprintHelper.Logic') || json['importable'] === false) {
    return {
      content: [{ type: 'text' as const, text: 'LogicJson/LogicMD are read-only views and cannot be imported as RawJson.' }],
      isError: true,
    };
  }
}
```

- [ ] Add tests:
  - Object RawJson is forwarded as object.
  - String RawJson is forwarded as string.
  - LogicJson object returns MCP error without Bridge call.
  - `importable:false` object returns MCP error without Bridge call.

Verification:

```powershell
Set-Location 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper_MCP_Server'
npm test
```

## 7. Wave 2 Tasks

### Task C4: BridgeRouter Protocol Integration

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Bridge\BlueprintHelperBridgeRouter.cpp`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Bridge\BlueprintHelperRequestValidator.cpp`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Source\BlueprintHelper\Private\Tests\BlueprintHelperObjectFirstBridgeTests.cpp`

**Purpose:** Convert Bridge external protocol to object-first after C++ services are ready.

- [ ] Add helpers in anonymous namespace:
  - `MakeRawJsonStatsObject(const TSharedPtr<FJsonObject>& RawJsonObject)`
  - `DiagnosticSetToJsonArray(const FBlueprintHelperDiagnosticSet& Diagnostics)`
  - `TryReadJsonObjectOrString(...)`
  - `SerializeJsonObjectForLegacy(...)`

- [ ] Update `HandleExportToJson()`:
  - Read optional `include_json_text`.
  - Set `ExportReq.bIncludeJsonText = include_json_text`.
  - Return fields: `format`, `schema`, `assetPath`, `graph`, `effective_scope`, `importable`, `payload`, `json`, `stats`, `diagnostics`.
  - Return `json_text` only if requested.
  - Never set `json` as a string by default.

- [ ] Update `HandleExportLogic()`:
  - Use `FBlueprintHelperLogicProcessor::ProcessRawJsonObject(ExportResult.JsonObject, LogicOptions)`.
  - For Markdown, return `markdown`, `stats`, `importable=false`.
  - For LogicJson, parse `LogicResult.OutputText` into `payload` and keep `logic` as a compatibility alias for one migration cycle.
  - Set schema values to `BlueprintHelper.LogicMarkdown.v1` and `BlueprintHelper.LogicJson.v1`.

- [ ] Update `HandleImportJson()`:
  - Accept object or string `payload.json`.
  - Put object into `ImportReq.JsonObject`.
  - Put string into `ImportReq.JsonText`.
  - Let ImportService enforce RawJson importability.

- [ ] Add tests:
  - `export_to_json` returns `payload` object.
  - `export_to_json` returns `json` object.
  - `export_to_json` omits `json_text` by default.
  - `export_to_json` includes `json_text` when requested.
  - `export_logic` calls object-first logic path and returns `importable=false`.
  - `import_json` accepts object payload and string payload.

Verification:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

Automation target:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst.Bridge;Quit'
```

### Task M3: MCP Protocol Regression Consolidation

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\tools.regression.test.ts`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\resources.object-first.test.ts`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\src\tools.import-object.test.ts`

**Purpose:** Confirm M1 and M2 work together with all Bridge payload shapes.

- [ ] Add regression table covering Bridge result shapes:
  - `{ payload: rawObject, json: rawObject }`
  - `{ json: rawObject }`
  - `{ json: JSON.stringify(rawObject) }`
  - `{ json_text: JSON.stringify(rawObject) }`

- [ ] Confirm `blueprint_export_to_json` default result:
  - first content item is summary text.
  - second content item is `resource_link`.
  - `structuredContent.format` is `raw_json_ref`.
  - no full RawJson appears in text content.

- [ ] Confirm `legacy_text_json` response mode:
  - returns inline structured RawJson as JSON text.
  - does not include a resource link.

- [ ] Confirm resource read:
  - returns RawJson body directly.
  - does not return a `{ json: ... }` wrapper.

Verification:

```powershell
Set-Location 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper_MCP_Server'
npm test
```

## 8. Wave 3 Tasks

### Task I1: End-To-End Verification

**Files:** no source writes.

**Purpose:** Validate the C++ plugin and MCP server as one object-first path.

- [ ] Build plugin:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

- [ ] Run object-first automation:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst;Quit'
```

- [ ] Build and test MCP server:

```powershell
Set-Location 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper_MCP_Server'
npm test
```

- [ ] Live Bridge validation with editor running:
  - Call `blueprint_export_to_json` for a small Blueprint and inspect `result.payload`.
  - Call RawJson resource URI and inspect direct RawJson body.
  - Call `blueprint_get_logic` and confirm Markdown remains first text content.
  - Call `blueprint_get_logic_json` and confirm `importable=false`.
  - Call `blueprint_import_json_to_graph` with structured RawJson object in a scratch Blueprint.
  - Call `blueprint_import_json_to_graph` with LogicJson and confirm it is rejected.

Pass criteria:

- Plugin build exits 0.
- `BlueprintHelper.ObjectFirst` automation exits 0.
- MCP `npm test` exits 0.
- Default Bridge export contains no stringified RawJson in `result.json`.
- MCP default RawJson export returns a resource link, not inline RawJson text.

### Task D1: Documentation And Migration Notes

**Files:**

- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\README.md`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\AgentGuide\Reference\02_Capability_Index.md`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\AgentGuide\Workflows\04_Read_Blueprint_Workflow.md`
- Modify: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\AgentGuide\Workflows\05_Edit_Blueprint_Workflow.md`
- Create: `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\Resources\Plan\BridgePayload_ObjectFirst_MigrationNotes_20260501.md`

**Purpose:** Document the new protocol and prevent Agent confusion between RawJson and LogicJson.

- [ ] Document Bridge fields:
  - `payload` is the structured body.
  - `json` is a compatibility alias and is object by default.
  - `json_text` only appears when requested.
  - `importable=false` means the body cannot be passed to RawJson import.

- [ ] Document MCP behavior:
  - `blueprint_export_to_json` defaults to `raw_json_ref`.
  - RawJson resource returns RawJson body directly.
  - `legacy_text_json` is available for compatibility/debug.
  - `blueprint_import_json_to_graph` accepts object or string RawJson.

- [ ] Document migration examples:
  - Old consumer: `JSON.parse(result.json)`.
  - New consumer: `result.payload ?? result.json`.
  - Legacy consumer: request `include_json_text` or MCP `legacy_text_json`.

Verification:

```powershell
Select-String -Path 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper/README.md' -Pattern 'payload','json_text','raw_json_ref','importable=false'
```

## 9. Dependency Rules For Workers

Each worker must follow these rules:

- Do not edit files outside the assigned write set unless the coordinator explicitly reassigns ownership.
- Do not revert changes made by other workers.
- If a shared contract is missing, stop and report the missing symbol instead of adding a second version.
- Keep all old public APIs as wrappers for one migration cycle.
- Add tests in new files when possible to preserve parallelism.
- Run the narrow verification command for the worker before returning.
- Return a summary with:
  - files changed,
  - tests run,
  - contract symbols added or consumed,
  - any follow-up dependency for the next wave.

## 10. Final Acceptance Checklist

- [ ] `FBlueprintHelperExportResult` has `JsonObject`.
- [ ] `ExportService.Export()` fills `JsonObject` by default.
- [ ] `JsonText` is generated only when requested or through legacy wrapper APIs.
- [ ] `HandleExportToJson()` returns `result.payload` object.
- [ ] `HandleExportToJson()` does not return stringified `result.json` by default.
- [ ] `HandleExportLogic()` consumes `ExportResult.JsonObject`.
- [ ] LogicJson/LogicMD responses carry `importable=false`.
- [ ] RawJson responses carry `importable=true`.
- [ ] `blueprint_export_to_json` defaults to `raw_json_ref`.
- [ ] RawJson MCP resource returns RawJson body directly.
- [ ] `blueprint_import_json_to_graph` accepts structured RawJson object.
- [ ] Legacy string RawJson import still works.
- [ ] LogicJson and LogicMD cannot be imported through RawJson import.
- [ ] Plugin build passes.
- [ ] Object-first automation tests pass.
- [ ] MCP `npm test` passes.
- [ ] AgentGuide and README describe the new fields.

## 11. Recommended Execution Order

Fastest safe order:

```text
Start:
  C0 + M0 + B0 + S0

When C0 completes:
  C1 + C2 + C3

When M0 completes:
  M1 + M2

When C1/C2/C3 complete:
  C4

When M1/M2 complete:
  M3

When C4 and M3 complete:
  I1 + D1
```

The practical maximum parallelism is five workers during Wave 1:

```text
C1 + C2 + C3 + M1 + M2
```

`C4` is intentionally sequential because it integrates all C++ service contracts inside `BlueprintHelperBridgeRouter.cpp`.
