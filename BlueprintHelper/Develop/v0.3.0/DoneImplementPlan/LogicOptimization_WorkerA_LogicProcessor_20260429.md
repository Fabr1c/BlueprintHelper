# Worker A LogicProcessor Execution Plan

## 目标

新增 `FBlueprintHelperLogicProcessor`，把 raw `BlueprintHelper.JsonToBlueprint` JSON 转成 `logic_json` 或 `logic_md`。本执行线不接 Bridge，不修改导出器，不修改 MCPServer。

## 文件边界

新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperLogicProcessor.h
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicProcessor.cpp
```

修改：

```text
无
```

移除：

```text
无
```

越界规则：

- 不修改 `BlueprintHelperBridgeRouter.*`。
- 不修改 `BlueprintTextConverter.cpp`。
- 不修改 `TextToBlueprintGenerator.cpp`。
- 需要新增自动化测试 cpp 时，先提交请求变更文档。

## 公开接口

在头文件中新增：

```cpp
enum class EBlueprintHelperLogicOutputFormat : uint8
{
	LogicJson,
	Markdown
};

enum class EBlueprintHelperLogicDetailLevel : uint8
{
	Brief,
	Normal,
	Debug
};

struct FBlueprintHelperLogicOptions
{
	EBlueprintHelperLogicOutputFormat Format = EBlueprintHelperLogicOutputFormat::LogicJson;
	EBlueprintHelperLogicDetailLevel DetailLevel = EBlueprintHelperLogicDetailLevel::Normal;
	bool bIncludeDataDependencies = true;
	bool bIncludeOrphanNodes = true;
	bool bIncludeNodeIds = false;
	bool bIncludePositions = false;
	bool bIncludeRawNodeTypes = false;
};

struct FBlueprintHelperLogicResult
{
	bool bSuccess = false;
	FString OutputText;
	FString ErrorMessage;
	int32 NodeCount = 0;
	int32 ExecLinkCount = 0;
	int32 DataLinkCount = 0;
	int32 EntryPointCount = 0;
	int32 OrphanNodeCount = 0;
};

class BLUEPRINTHELPER_API FBlueprintHelperLogicProcessor
{
public:
	static FBlueprintHelperLogicResult ProcessRawJson(
		const FString& RawJsonText,
		const FBlueprintHelperLogicOptions& Options);
};
```

## 实现步骤

- [ ] 新增头文件，保证只暴露必要接口。
- [ ] 新增 cpp，解析空字符串和非法 JSON，返回 `bSuccess=false`。
- [ ] 支持单图结构：顶层 `nodes`、`links`。
- [ ] 支持完整蓝图结构：顶层 `graphs[]`。
- [ ] 兼容图名字段：`name`、`graph`、`graph_name`。
- [ ] 兼容 link 旧格式：`from_id/from_pin/to_id/to_pin`。
- [ ] 兼容 link 嵌套格式：`source.node/source.pin` 和 `target.node/target.pin`。
- [ ] 实现节点分类：`event`、`call`、`get`、`set`、`branch`、`switch`、`sequence`、`loop`、`broadcast`、`bind_delegate`、`unbind_delegate`、`timeline`、`cast`、`reroute`、`comment`、`unknown`。
- [ ] 实现 link 分类优先级：显式 `kind`，pin type，pin 名启发式，最后 `unknown`。
- [ ] 输出 `logic_json`，包含 `version`、`schema`、`source_schema`、`stats`、`graphs`。
- [ ] 输出 `logic_md`，包含图名、入口、执行项、Data Dependencies、Orphans。
- [ ] `debug` 模式输出 node id、raw type、link confidence。

## 最小输出结构

`logic_json` 必须稳定输出：

```json
{
  "version": "1.0",
  "schema": "BlueprintHelper.LogicGraph",
  "source_schema": "BlueprintHelper.JsonToBlueprint",
  "stats": {
    "nodes": 0,
    "exec_links": 0,
    "data_links": 0,
    "entry_points": 0,
    "orphans": 0
  },
  "graphs": []
}
```

## 验收

编译插件：

```powershell
& 'G:/UE_5.3/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

通过标准：

- 空 JSON 和非法 JSON 返回错误文本。
- 单图 raw JSON 可生成合法 `logic_json`。
- `logic_md` 不包含完整 raw JSON。
- 不确定 link 不被丢弃，输出 `confidence=inferred` 或 `unknown`。

