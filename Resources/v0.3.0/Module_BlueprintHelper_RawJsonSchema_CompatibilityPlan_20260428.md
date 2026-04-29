# BlueprintHelper Raw JSON Schema 兼容增强规划（2026-04-28）

## 一、修改原因

### 1.1 当前 links 缺少明确语义

当前 raw JSON 的 link 通常类似：

```json
{
  "from_id": "Node_A",
  "from_pin": "then",
  "to_id": "Node_B",
  "to_pin": "execute"
}
```

它能用于导入连线，但对逻辑分析不够：

- 不能直接判断这是执行线还是数据线。
- 不能直接知道 Pin 类型。
- 不能区分 `then` 是 exec 输出还是普通字段名。
- 对 Agent 和 LogicProcessor 都需要额外推断。

### 1.2 逻辑处理器需要更可靠的源信息

LogicProcessor 可以先用 Pin 名启发式分类，但长期应依赖导出时从 `UEdGraphPin` 获取的真实类型。

由于导出阶段能直接访问 Pin 对象，因此最准确的位置是在 raw JSON 中补充可选语义字段。

### 1.3 必须保持导入兼容

新增字段不能破坏 `TextToBlueprintGenerator` 现有导入逻辑。导入器应继续只依赖：

```text
from_id
from_pin
to_id
to_pin
```

新增字段只作为可选信息存在。

---

## 二、修改目标

### 2.1 为 links 增加可选语义字段

推荐新增：

```json
{
  "from_id": "Node_A",
  "from_pin": "then",
  "to_id": "Node_B",
  "to_pin": "execute",
  "kind": "exec",
  "from_pin_type": "exec",
  "to_pin_type": "exec",
  "from_direction": "output",
  "to_direction": "input"
}
```

### 2.2 为 node pins 增加可选结构

当前 node 的 `inputs` 更像默认值字典。建议后续补充 `pins` 数组：

```json
{
  "id": "Node_A",
  "type": "K2Node_CallFunction",
  "pins": [
    {
      "name": "execute",
      "direction": "input",
      "category": "exec"
    },
    {
      "name": "then",
      "direction": "output",
      "category": "exec"
    },
    {
      "name": "InString",
      "direction": "input",
      "category": "string",
      "default_value": "Hello"
    }
  ]
}
```

第一阶段不要求导入器读取 `pins`，只用于分析和调试。

### 2.3 兼容两种 link 写法

当前测试资源和规划中可能出现两种 link 结构：

旧格式：

```json
{
  "from_id": "A",
  "from_pin": "then",
  "to_id": "B",
  "to_pin": "execute"
}
```

嵌套格式：

```json
{
  "source": { "node": "A", "pin": "then" },
  "target": { "node": "B", "pin": "execute" }
}
```

推荐 LogicProcessor 两种都读。导入器可在后续增强中兼容嵌套格式，但 raw JSON 导出仍优先使用旧格式，保证最小变更。

---

## 三、字段定义

### 3.1 `kind`

| 值 | 说明 |
|----|------|
| `exec` | 执行线 |
| `data` | 数据线 |
| `delegate` | 委托绑定或调用相关连线 |
| `unknown` | 无法分类 |

第一阶段可只使用 `exec` / `data` / `unknown`。

### 3.2 `from_pin_type` / `to_pin_type`

来自 `FEdGraphPinType.PinCategory`：

```text
exec
bool
byte
int
int64
float
double
real
name
string
text
object
class
struct
interface
delegate
wildcard
```

### 3.3 `from_direction` / `to_direction`

| 值 | 来源 |
|----|------|
| `input` | `EGPD_Input` |
| `output` | `EGPD_Output` |
| `unknown` | 无法判断 |

---

## 四、导出侧修改建议

### 4.1 修改 link 构造位置

在 `FBlueprintToTextConverter` 生成 links 时，拿到源 Pin 和目标 Pin 后增加：

```cpp
LinkObject->SetStringField(TEXT("kind"), IsExecPin(SourcePin, TargetPin) ? TEXT("exec") : TEXT("data"));
LinkObject->SetStringField(TEXT("from_pin_type"), SourcePin->PinType.PinCategory.ToString());
LinkObject->SetStringField(TEXT("to_pin_type"), TargetPin->PinType.PinCategory.ToString());
LinkObject->SetStringField(TEXT("from_direction"), SourcePin->Direction == EGPD_Output ? TEXT("output") : TEXT("input"));
LinkObject->SetStringField(TEXT("to_direction"), TargetPin->Direction == EGPD_Output ? TEXT("output") : TEXT("input"));
```

判断执行线：

```cpp
static bool IsExecPin(const UEdGraphPin* Pin)
{
    return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}
```

### 4.2 不改变现有必要字段

继续保留：

```text
from_id
from_pin
to_id
to_pin
```

这四个字段仍是导入器的主键字段。

---

## 五、导入侧兼容策略

### 5.1 第一阶段

导入器忽略新增字段。

### 5.2 第二阶段

导入器可利用 `kind` 做预校验：

- `kind=exec` 时，两端 Pin 必须都是 exec。
- `kind=data` 时，不允许连接到 exec Pin。
- 类型不匹配时给出更明确诊断。

### 5.3 第三阶段

支持嵌套 link 格式：

```json
{
  "source": { "node": "A", "pin": "then" },
  "target": { "node": "B", "pin": "execute" }
}
```

但不建议立刻把导出格式改成嵌套结构。

---

## 六、版本策略

当前 raw JSON 可将 schema 版本提升到：

```json
{
  "version": "2.11",
  "schema": "BlueprintHelper.JsonToBlueprint"
}
```

新增字段均为 optional。导入器不应因为版本高于当前而直接拒绝，除非主 schema 不匹配。

---

## 七、修改计划

### Phase A：Link 可选字段

- 为图对象导出 links 增加 `kind`。
- 增加 pin category 和 direction 字段。
- 保持旧字段不变。

### Phase B：LogicProcessor 优先使用显式字段

- 有 `kind` 时直接使用。
- 无 `kind` 时回退 Pin 名启发式。
- 输出 `confidence`。

### Phase C：Pins 数组增强

- 为 node 增加可选 `pins` 数组。
- 不替换现有 `inputs`。
- 用于调试、类型诊断和 Agent 精确理解。

### Phase D：导入预校验增强

- 利用 link `kind` 和 pin type 提前发现错误。
- 返回结构化诊断，而不是只返回连线失败。

---

## 八、验收标准

1. 原有 JSON fixture 仍可导入。
2. 新导出的 links 包含 `kind`。
3. `kind=exec` 的线两端 Pin 类型均为 exec。
4. LogicProcessor 在有 `kind` 时不再依赖 Pin 名猜测。
5. 删除新增字段后，导入器仍能工作。

