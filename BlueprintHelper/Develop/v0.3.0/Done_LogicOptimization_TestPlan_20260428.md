# BlueprintHelper 逻辑视图优化测试与 Fixture 计划（2026-04-28）

## 一、修改原因

LogicProcessor 会把 raw JSON 转换为更短的逻辑视图。该转换不会直接修改蓝图，但它会影响 Agent 对蓝图的理解。如果摘要错误，Agent 可能基于错误逻辑继续修改蓝图。

因此该模块必须有独立 fixture 和回归标准，不能只依赖人工观察。

---

## 二、测试目标

1. 确认 raw JSON 到 `logic_json` 的转换稳定。
2. 确认 `logic_md` 可读且不包含冗余 raw JSON。
3. 确认执行线、数据线、入口事件、变量读写能被正确识别。
4. 确认新增 raw JSON 字段不会破坏旧导入。
5. 确认在缺少 `kind` 的旧 JSON 上仍可生成基本摘要。

---

## 三、Fixture 目录规划

新增目录：

```text
Resources/TestFixtures/LogicProcessor/
```

建议文件命名：

```text
<case>.raw.json
<case>.logic.json
<case>.logic.md
```

例如：

```text
simple_beginplay_call.raw.json
simple_beginplay_call.logic.json
simple_beginplay_call.logic.md
```

首批最小落地集：

```text
simple_beginplay_call
branch_flow
compat_old_links
```

该集合用于先验证 LogicProcessor 的基础稳定性：入口事件、调用节点、分支执行线、旧 links 无 `kind` 时的启发式识别。下方 10 个用例保留为后续扩展验收集，不要求第一批一次性全部落地。

---

## 四、基础用例

### CASE-001：BeginPlay 调用函数

输入逻辑：

```text
ReceiveBeginPlay -> PrintString
```

验证点：

- `ReceiveBeginPlay` 被识别为 entry point。
- `PrintString` 被识别为 call。
- `then -> execute` 被识别为 exec link。
- Markdown 输出不包含坐标。

---

### CASE-002：变量读取并传入函数

输入逻辑：

```text
Get Health -> PrintString.InString
```

验证点：

- `K2Node_VariableGet` 被识别为 get。
- `Health -> PrintString.InString` 被识别为 data dependency。
- VariableGet 不出现在执行流主链中。

---

### CASE-003：变量写入

输入逻辑：

```text
ReceiveBeginPlay -> Set bIsDead = false
```

验证点：

- `K2Node_VariableSet` 被识别为 set。
- set 节点出现在执行流中。
- 默认值或输入依赖可被输出。

---

### CASE-004：Branch 分支

输入逻辑：

```text
ReceiveTick -> Branch(IsDead)
  True -> Return
  False -> Move
```

验证点：

- Branch 被识别为 `branch`。
- `Condition` 被输出。
- True / False 两条执行线分开显示。

---

### CASE-005：ForLoop

输入逻辑：

```text
CustomEvent -> ForLoop
  LoopBody -> Call
  Completed -> Call
```

验证点：

- MacroInstance + ForLoop 被识别为 `loop`。
- LoopBody 和 Completed 被区分。
- 循环索引作为数据输出可进入 data dependencies。

---

### CASE-006：Switch

输入逻辑：

```text
InputAction -> SwitchEnum
  CaseA -> Call A
  CaseB -> Call B
  Default -> Call Default
```

验证点：

- Switch 节点识别为 `switch`。
- Case 输出 pin 被保留。
- Markdown 中不丢失 Default 分支。

---

### CASE-007：委托绑定和广播

输入逻辑：

```text
BeginPlay -> Bind Event
CustomEvent -> Broadcast OnHealthChanged
```

验证点：

- AddDelegate / AssignDelegate 识别为 bind_delegate。
- CallDelegate 识别为 broadcast。
- 事件和委托名称可读。

---

### CASE-008：Enhanced Input Action

输入逻辑：

```text
IA_Interact Triggered -> Call Interact
```

验证点：

- `K2Node_EnhancedInputAction` 被识别为 event。
- Triggered / Started / Completed 等输出能作为入口或执行分支。

---

### CASE-009：多图蓝图

输入结构：

```text
EventGraph
FunctionGraph: CalculateDamage
MacroGraph: ClampValue
```

验证点：

- `graphs[]` 分组输出。
- 每个 graph 有独立 stats。
- FunctionEntry / FunctionResult 不作为普通孤立节点误报。

---

### CASE-010：孤立节点

输入逻辑：

```text
EventGraph 中存在未连接 PrintString
```

验证点：

- 未连接节点进入 `orphans`。
- brief 模式可隐藏 orphans。
- normal/debug 模式显示 orphans。

---

## 五、兼容用例

### COMPAT-001：旧 links 无 `kind`

验证点：

- LogicProcessor 使用 Pin 名启发式。
- 输出中标记 `confidence=inferred`。

### COMPAT-002：新 links 有 `kind`

验证点：

- LogicProcessor 直接使用 `kind`。
- 输出中标记 `confidence=explicit`。

### COMPAT-003：删除新增字段后导入仍成功

验证点：

- `import_json` 只依赖 `from_id/from_pin/to_id/to_pin`。
- 新增字段不成为导入必需项。

### COMPAT-004：嵌套 link 格式

验证点：

- LogicProcessor 能解析 `source/target`。
- raw 导出仍默认使用旧格式。

---

## 六、回归检查方法

### 6.1 离线转换测试

如果后续有自动测试模块，可增加命令行或 Automation Test：

```text
Load *.raw.json
  -> FBlueprintHelperLogicProcessor::ProcessRawJson(format=LogicJson)
  -> compare with *.logic.json
```

### 6.2 Markdown 快照测试

Markdown 可以做弱匹配，不建议逐字符比较。推荐检查关键片段：

```text
# EventGraph
## ReceiveBeginPlay
PrintString
Data Dependencies
```

### 6.3 语义统计测试

每个 fixture 至少检查：

```text
nodes
exec_links
data_links
entry_points
orphans
```

---

## 七、Agent 侧验证

Agent 读取蓝图时应遵守：

1. 先调用 logic 摘要工具。
2. 根据 logic 摘要说明当前逻辑。
3. 只有当需要修改时，再拉 raw JSON。
4. 写入前必须明确目标蓝图路径和目标图表名。

验证场景：

```text
让 Agent 解释 EventGraph 做了什么。
要求 Agent 不引用坐标、不复述 raw JSON。
要求 Agent 指出入口事件、执行顺序、变量依赖。
```

---

## 八、验收标准

### 8.1 第一批最小落地验收

1. `simple_beginplay_call` 覆盖 `ReceiveBeginPlay -> PrintString`。
2. `branch_flow` 覆盖 `ReceiveTick -> Branch -> True/False`。
3. `compat_old_links` 的 raw links 不包含 `kind`，logic 输出标记 `confidence=inferred`。
4. 所有首批 fixture 均包含合法 raw JSON、`logic_json` 和 `logic_md`。
5. `logic_md` 不复述完整 raw JSON，不包含坐标。

### 8.2 后续扩展验收

1. 至少 10 个 fixture 覆盖常见节点类型。
2. 所有 fixture 可生成合法 `logic_json`。
3. 所有 fixture 可生成可读 `logic_md`。
4. 旧 raw JSON 无 `kind` 时仍能输出基本逻辑。
5. 新 raw JSON 有 `kind` 时执行线 / 数据线分类不依赖 Pin 名。
6. 修改节点坐标不影响 logic 输出。
7. logic 输出不可被误用于 `import_json`。
