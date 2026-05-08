# BlueprintHelper Logic Optimization Execution Index

## 目标

根据 `Resources/v0.3.0/Module_BlueprintHelper_JsonLogicOptimization_Index_20260428.md` 及其引用文档，把 JSON / Agent 逻辑视图优化拆成可并行执行的最小改动文档。

本轮执行目标是新增派生逻辑视图，不替换 raw JSON，不破坏 `export_to_json`、`import_json`、`validate_json`。

## 全局边界

允许新增或修改的文件必须由对应执行文档列出。

如果实现者发现必须改动未列出的文件，必须先新增请求变更文档：

```text
Resources/Plan/ChangeRequests/CR_<YYYYMMDD>_<short_name>.md
```

请求变更文档被用户接受前，不得修改越界文件。

## 并行分发

| 执行线 | 文档 | 并行关系 | 主要边界 |
|---|---|---|---|
| A | `LogicOptimization_WorkerA_LogicProcessor_20260429.md` | 第一批，可独立开始 | 新增 LogicProcessor，不接 Bridge |
| B | `LogicOptimization_WorkerB_BridgeExportLogic_20260429.md` | 依赖 A 的公开接口 | 只注册 `export_logic` Bridge 命令 |
| C | `LogicOptimization_WorkerC_RawJsonLinkSemantics_20260429.md` | 第一批，可独立开始 | 只增强导出 link 可选字段 |
| D | `LogicOptimization_WorkerD_MCPTools_20260429.md` | 依赖 B 的命令契约 | 只新增 MCP 逻辑读取工具 |
| E | `LogicOptimization_WorkerE_FixturesValidation_20260429.md` | 可与 A 并行准备，最终依赖 A 输出 | 新增 fixture 与验证说明 |

推荐顺序：

```text
第一批：A + C + E fixture 草稿
第二批：B
第三批：D + E 对齐输出
最后：整体编译、MCP build、fixture 验证
```

## 文件总表

新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperLogicProcessor.h
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicProcessor.cpp
Resources/TestFixtures/LogicProcessor/simple_beginplay_call.raw.json
Resources/TestFixtures/LogicProcessor/simple_beginplay_call.logic.json
Resources/TestFixtures/LogicProcessor/simple_beginplay_call.logic.md
Resources/TestFixtures/LogicProcessor/branch_flow.raw.json
Resources/TestFixtures/LogicProcessor/branch_flow.logic.json
Resources/TestFixtures/LogicProcessor/branch_flow.logic.md
Resources/TestFixtures/LogicProcessor/compat_old_links.raw.json
Resources/TestFixtures/LogicProcessor/compat_old_links.logic.json
Resources/TestFixtures/LogicProcessor/compat_old_links.logic.md
Resources/Plan/ChangeRequest_Template_20260429.md
```

修改：

```text
Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
Source/BlueprintHelper/Private/BlueprintTextConverter.cpp
MCPServer/src/tools.ts
Resources/AGENT.md
Resources/JsonToBlueprintRules.md
```

移除：

```text
无
```

## 最小修改原则

- 不拆分 `BlueprintHelperLogicJsonWriter.cpp` 或 `BlueprintHelperLogicMarkdownWriter.cpp`，第一版保持单 cpp。
- 不修改 `TextToBlueprintGenerator.cpp`。
- 不改变 `FBlueprintHelperExportService` 的现有接口。
- 不扩展 `export_to_json` 的返回结构。
- 不把 `logic_json` 标记为可导入。
- 不默认依赖当前编辑器焦点执行写操作。

## 验收命令

UE 编译：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

MCP Server 编译：

```powershell
npm run build
```

执行目录：

```text
G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
```

## 整体验收标准

- 旧 `export_to_json` 仍返回 `result.json`。
- 新 `export_logic format=logic_json` 返回结构化 `logic` 对象和 `stats`。
- 新 `export_logic format=logic_md` 返回 `markdown` 字符串和 `stats`。
- 新 MCP 工具不替代 raw JSON 写回链路。
- 新导出的 links 包含可选 `kind`、pin type、direction 字段。
- 删除新增 link 字段后，旧导入仍可工作。
- fixture 能覆盖旧 link、显式 kind、基础执行线、分支。

