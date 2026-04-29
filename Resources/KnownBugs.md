# BlueprintHelper 已知 Bug 与限制

> 本文档记录 BlueprintHelper 插件在自动化蓝图生成（vibe coding）场景下发现的已知 Bug 和架构限制。  
> 修复后的条目会标注版本号并移至"已修复"章节。  
> 规则文档参见 [JsonToBlueprintRules.md](JsonToBlueprintRules.md)。

---

## 已修复 (v2.9)

### BUG-001：导入拒绝 `__function_entry__` / `__function_result__` 虚拟节点

- **现象**：AI 生成的 JSON 在 `nodes` 数组中包含 `id: "__function_entry__"` 或 `type: "K2Node_FunctionEntry"` 的节点声明时，导入报"未识别的节点类型"并加入未解析列表。
- **根因**：导出时 FunctionEntry/FunctionResult 被跳过（只保留在 links 中），但导入的节点解析循环没有对这两个保留 ID 做容错。AI 不知道它们应当省略，手动写入后触发未识别错误。
- **修复**：在导入的节点生成循环中，静默跳过 `id` 为 `__function_entry__` / `__function_result__` 或 `type` 为 `K2Node_FunctionEntry` / `K2Node_FunctionResult` 的节点。连线仍通过图表中已有节点自动恢复。

### BUG-002：增量导入无法引用图中已有节点

- **现象**：执行增量导入时，新节点无法与图中已有节点（非 FunctionEntry/FunctionResult）建立连线，因为 `IdToSpawnedNode` 映射只包含新生成的节点和函数入口/结果。
- **修复**：新增 `existing_node_refs` JSON 字段，允许声明对已有节点的引用。支持按 `node_guid`（精确匹配）或 `node_title`（子串匹配）查找。详见规则文档。

### BUG-003：导出 JSON 缺少稳定节点标识

- **现象**：导出的 JSON 节点只有 `id`（如 `Node_0`、`Node_1`）和 `name`（节点标题），但没有可在后续增量导入中精确引用的稳定标识。
- **修复**：导出时为每个节点添加 `node_guid` 字段（UE 节点的 `NodeGuid`，32 位十六进制数字），可在 `existing_node_refs` 中通过 `node_guid` 精确引用。

### LIMIT-001：Enhanced Input 节点支持 (v2.9 已修复)

- **原现象**：JSON 中指定 `type: "K2Node_EnhancedInputAction"` 时报"未识别的节点类型"。
- **修复**：新增 `FEnhancedInputActionNodeHandler`，支持通过 `input_action_path` 指定 InputAction 资产路径来生成节点。

### LIMIT-003：ReconstructNode 破坏连线 (v2.9 已修复)

- **原现象**：导入完成后部分节点连线丢失，因为连线后才调用 ReconstructNode 导致引脚重建。
- **修复**：将 `ReconstructNode` 调用从连线之后移到连线之前，确保引脚完整后再建立连接。

### LIMIT-004：新增节点类型支持 (v2.9 已修复)

以下节点类型已获得导入/导出支持：
- `K2Node_EnhancedInputAction` — Enhanced Input 输入动作
- `K2Node_PromotableOperator` — UE5 可提升运算符（加减乘除、比较等数学运算）
- `K2Node_CommutativeAssociativeBinaryOperator` — 交换结合律二元运算符（多参数加法/乘法等）
- `K2Node_SwitchInteger` / `SwitchString` / `SwitchName` / `SwitchEnum` — Switch 分支
- `K2Node_Select` — 条件值选择

### BUG-004：DynamicCast 引脚别名未生效 (v2.10 已修复)

- **现象**：使用文档中的引脚别名（`valid`、`invalid`、`cast_result`、`success`）连接 DynamicCast 节点时，连线无法创建，返回 nullptr。
- **根因**：`FindPinByAlias()` 中没有 DynamicCast 特有的别名映射。`valid` 无法匹配到实际引脚名 `"then"` (PN_CastSucceeded)，`invalid` 无法匹配 `"CastFailed"`，`cast_result` 无法匹配动态命名的 `"AsXxx"` 引脚，`success` 无法匹配 `"bSuccess"`。
- **修复**：在 `FindPinByAlias()` 中添加 4 组 DynamicCast 别名映射：`valid`/`cast_succeeded` → PN_CastSucceeded、`invalid`/`cast_failed` → CastFailed、`cast_result` → 以 "As" 开头的输出引脚、`success`/`bsuccess`/`bool_success` → bSuccess。

---

## 未修复

### LIMIT-002：`K2Node_VariableGet` 是纯节点，无法串联执行线

- **现象**：AI 尝试将执行线（exec pin）连到 VariableGet 节点时失败，因为 UE 中 VariableGet 是**纯节点（Pure Node）**，没有执行引脚。
- **根因**：这是 UE 引擎的设计，不是插件 Bug。VariableGet 只提供数据输出，不参与执行流。
- **规避**：
  - 执行线只串联 `VariableSet`、`CallFunction`（非纯函数）、`CustomEvent`、`Sequence` 等有执行引脚的节点。
  - VariableGet 仅作为数据源连接到其他节点的数据输入引脚。

### LIMIT-005：不支持以下节点类型

以下节点类型尚未实现导入 Handler：

| 节点类型 | 说明 |
|---------|------|
| `K2Node_MathExpression` | 数学表达式 |
| `K2Node_InputAction` / `K2Node_InputKey` | 旧版输入节点 |
| 自定义宏库节点 | 来自第三方宏库的宏实例 |

对于上述类型，导入时会被加入"未解析列表"。可通过 `K2Node_CallFunction` 间接调用同名函数来规避部分场景。

### LogDetail
- 查找并定位问题所在，为什么会频繁出现断开又链接，
Log          LogBlueprintHelperBridge  Bridge 客户端已断开。
Log          LogBlueprintHelperBridge  Bridge 客户端已连接。
Warning      LogJson                   Field target_graph was not found.
Warning      LogJson                   Json Value of type 'Null' used as a 'String'.
Log          LogBlueprintHelperBridge  Bridge 客户端已断开。
Log          LogBlueprintHelperBridge  Bridge 客户端已连接。
Warning      LogJson                   Field target_graph was not found.
Warning      LogJson                   Json Value of type 'Null' used as a 'String'.
Log          LogBlueprintHelperBridge  Bridge 客户端已断开。
---

## 报告格式

如发现新问题，请按以下格式添加：

```
### BUG/LIMIT-NNN：简要标题

- **现象**：
- **根因**：
- **影响**：
- **规避**：（可选）
- **计划**：（可选）
```
