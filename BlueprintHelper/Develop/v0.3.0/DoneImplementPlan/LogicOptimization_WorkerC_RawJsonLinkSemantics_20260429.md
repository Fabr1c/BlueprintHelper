# Worker C Raw JSON Link Semantics Execution Plan

## 目标

为 raw JSON 导出的 links 增加可选语义字段，让 LogicProcessor 优先读取显式信息，同时保持旧导入兼容。

## 文件边界

修改：

```text
Source/BlueprintHelper/Private/BlueprintTextConverter.cpp
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

- 不修改 `TextToBlueprintGenerator.cpp`。
- 不修改 `BlueprintTextConverter.h`，除非必须新增 private helper 声明。若必须新增，先提交请求变更文档。
- 不修改 JSON schema 校验器。

## 新增字段

每条 link 保留旧字段：

```text
from_id
from_pin
to_id
to_pin
```

新增可选字段：

```text
kind
from_pin_type
to_pin_type
from_direction
to_direction
```

`kind` 第一版只输出：

```text
exec
data
unknown
```

## 实现位置

优先修改直接从 `UEdGraphPin` 导出的路径：

```text
Source/BlueprintHelper/Private/BlueprintTextConverter.cpp
ExportGraphNodesAndLinks
```

该位置当前设置：

```cpp
LinkObj->SetStringField(TEXT("from_id"), SourceId);
LinkObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
LinkObj->SetStringField(TEXT("to_id"), *TargetId);
LinkObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
```

T3D 文本转换路径没有真实 `UEdGraphPin` 对象，第一版不补字段，避免猜测污染 raw JSON。

## 实现步骤

- [ ] 在 `ExportGraphNodesAndLinks` 中判断 `Pin` 和 `LinkedPin` 的 `PinType.PinCategory`。
- [ ] 当两端至少一端是 `UEdGraphSchema_K2::PC_Exec` 时输出 `kind=exec`。
- [ ] 当两端都不是 exec 且类型可读时输出 `kind=data`。
- [ ] 其它情况输出 `kind=unknown`。
- [ ] 输出 `from_pin_type` 和 `to_pin_type`。
- [ ] 输出 `from_direction` 和 `to_direction`。
- [ ] 确认旧字段保持不变。

## 最小修改约束

- 不提升 schema version，除非用户另行接受。
- 不添加 `pins` 数组，第一版只做 link 可选字段。
- 不改导入逻辑，导入器继续忽略新增字段。

## 验收

编译插件：

```powershell
& 'G:/UE_5.3/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

通过标准：

- 新导出的 graph links 包含新增字段。
- 原有 `from_id/from_pin/to_id/to_pin` 字段未改名。
- 删除新增字段后的 JSON 仍符合旧导入需求。
- T3D 转换路径不因本次修改改变输出语义。

