# Worker B Bridge Export Logic Execution Plan

## 目标

在 Bridge 层新增 `export_logic` 命令，复用现有 `FBlueprintHelperExportService` 导出 raw JSON，再调用 Worker A 的 `FBlueprintHelperLogicProcessor` 生成逻辑视图。

## 依赖

依赖 Worker A 完成：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperLogicProcessor.h
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicProcessor.cpp
```

## 文件边界

修改：

```text
Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
```

新增：

```text
无
```

移除：

```text
无
```

越界规则：

- 不修改 `FBlueprintHelperExportService` 接口。
- 不修改 `export_to_json` 的现有 `result.json` 返回。
- 不修改 MCPServer。
- 需要新增错误码时先提交请求变更文档。当前错误码已有 `InvalidRequest`、`JsonParseFailed`、`ExecutionFailed`。

## 请求参数

`payload` 支持：

```text
target_blueprint
target_graph
scope
format
detail
include_data_dependencies
include_orphans
include_node_ids
include_positions
include_raw_node_types
```

取值约束：

- `scope`: `single_graph` 或 `full_blueprint`，默认 `single_graph`。
- `format`: `logic_json` 或 `logic_md`，默认 `logic_md`。
- `detail`: `brief`、`normal`、`debug`，默认 `normal`。

## 响应结构

`logic_json`：

```json
{
  "format": "logic_json",
  "schema": "BlueprintHelper.LogicGraph",
  "importable": false,
  "logic": {},
  "stats": {}
}
```

`logic_md`：

```json
{
  "format": "logic_md",
  "schema": "BlueprintHelper.LogicMarkdown",
  "importable": false,
  "markdown": "# EventGraph\n",
  "stats": {}
}
```

## 实现步骤

- [ ] 在 `BlueprintHelperBridgeRouter.h` 声明 `HandleExportLogic`。
- [ ] 在 `HandleRequest` 中注册 `export_logic`，位置紧邻 `export_to_json`。
- [ ] 在 cpp include `Services/BlueprintHelperLogicProcessor.h`。
- [ ] 解析 payload 并构造 `FBlueprintHelperExportRequest`。
- [ ] 调用 `ExportService.Export` 获取 raw JSON。
- [ ] 把 `format`、`detail` 和 include 参数转换为 `FBlueprintHelperLogicOptions`。
- [ ] 调用 `FBlueprintHelperLogicProcessor::ProcessRawJson`。
- [ ] 失败时返回 `JsonParseFailed` 或 `ExecutionFailed`。
- [ ] 成功时设置 `importable=false`、`format`、`schema`、`stats`。

## 最小修改约束

- 不把 `logic_json` 塞进 `result.json`。
- 不改旧 `export_to_json`。
- 不新增 `blueprint_get_logic` MCP 工具，本执行线只负责 Bridge。
- 不引入新的 Service 类型。

## 验收

编译插件：

```powershell
& 'G:/UE_5.3/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

通过标准：

- `export_to_json` 老请求仍只返回 `json`。
- `export_logic format=logic_md` 返回 `markdown`。
- `export_logic format=logic_json` 返回 `logic`。
- 缺失或非法 `format` 返回 `InvalidRequest`。

