# BlueprintHelper GraphWrite Op Coverage UE Source Read Task

日期：2026-05-25

## 目标

为 GraphWrite `kind=op` 的下一阶段实现提供 UE 5.6 源码证据，确认“普通蓝图可见的 op 操作”全集，并把每个操作归类到正确的 GraphWrite 生成路径。

本任务只做 UE 源码读取和证据表导出，不修改 BlueprintHelper 代码。

## 工作目录约定

所有路径均以 UE 引擎目录为根目录：

```powershell
Set-Location UE_5.6\Engine
```

文档和结果中的源码路径也必须保持为 `Source/...` 相对路径，不使用引擎目录外显的绝对源码路径。

## 范围定义

本次要覆盖的是 UE 5.6 普通蓝图 Action Menu 中可正常生成、语义上属于 operator / op-like 的节点。

必须纳入：

1. `UK2Node_PromotableOperator` 支持的 TypePromotion 顶层 operator。
2. 带 `CommutativeAssociativeBinaryOperator` metadata、在普通蓝图中作为可加输入 pin operator 节点生成的函数。
3. 带 `CompactNodeTitle` / `ScriptOperator`，在普通蓝图中表现为常规 operator-like call node 的函数。
4. 普通蓝图可见的 boolean、bitwise、modulo、min/max、string append、vector、rotator、quat、transform、color、timespan 等 op-like 操作。

不应纳入 `kind=op`：

1. `BlueprintAutocast` 转换函数，归 `convert_function`。
2. latent / async / schedule 类函数，归对应 function operation。
3. 只有函数名像 operator、但普通蓝图菜单不以 op-like 方式暴露的普通函数。
4. 被 `IgnoreTypePromotion` 排除且无法作为普通 op-like call node 合理生成的函数。

## 必读源码路径

### TypePromotion 顶层 operator

读取：

```text
Source/Editor/BlueprintGraph/Private/BlueprintTypePromotion.cpp
Source/Editor/BlueprintGraph/Classes/BlueprintTypePromotion.h
Source/Editor/BlueprintGraph/Classes/K2Node_PromotableOperator.h
Source/Editor/BlueprintGraph/Private/K2Node_PromotableOperator.cpp
```

需要确认：

1. `FTypePromotion::GetAllOpNames()` 的完整 operator 名称。
2. `FTypePromotion::GetComparisonOpNames()` 的比较类 operator。
3. `FTypePromotion::GetOperatorSpawner()` / `RegisterOperatorSpawner()` 的注册语义。
4. `FTypePromotion::FindBestMatchingFunc()` 对 typed pins 的依赖。
5. `UK2Node_PromotableOperator` 的默认 pin、比较输出、add/remove pin、compile expansion 行为。

### ActionDatabase 与函数节点生成

读取：

```text
Source/Editor/BlueprintGraph/Private/BlueprintFunctionNodeSpawner.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_CallFunction.h
Source/Editor/BlueprintGraph/Private/K2Node_CallFunction.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_CommutativeAssociativeBinaryOperator.h
Source/Editor/BlueprintGraph/Private/K2Node_CommutativeAssociativeBinaryOperator.cpp
Source/Editor/BlueprintGraph/Classes/EdGraphSchema_K2.h
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
```

需要确认：

1. `CommutativeAssociativeBinaryOperator` metadata 如何使函数生成 `UK2Node_CommutativeAssociativeBinaryOperator`。
2. `CompactNodeTitle` 如何影响普通 `UK2Node_CallFunction` 的 operator-like 表现。
3. `BlueprintAutocast` 与普通 op 的边界。
4. `IgnoreTypePromotion` 对 TypePromotion 和普通 call node 的影响。
5. ActionDatabase 中 function spawner 与 operator spawner 的注册路径差异。

### Runtime 函数库 op-like 操作

优先读取：

```text
Source/Runtime/Engine/Classes/Kismet/KismetMathLibrary.h
Source/Runtime/Engine/Private/KismetMathLibrary.cpp
Source/Runtime/Engine/Classes/Kismet/KismetStringLibrary.h
Source/Runtime/Engine/Private/KismetStringLibrary.cpp
Source/Runtime/TimeManagement/Public/TimeManagementBlueprintLibrary.h
Source/Runtime/TimeManagement/Private/TimeManagementBlueprintLibrary.cpp
```

然后用搜索补齐所有模块中的普通蓝图 op-like metadata：

```powershell
rg -n "CommutativeAssociativeBinaryOperator|ScriptOperator|CompactNodeTitle|BlueprintAutocast|IgnoreTypePromotion" Source/Runtime Source/Editor -g "*.h" -g "*.cpp"
```

需要特别关注：

1. Boolean：`AND`、`OR`、`NAND`、`NOR`、`XOR`、`NOT`、`==`、`!=`。
2. Numeric：`+`、`-`、`*`、`/`、`%`、`<`、`<=`、`>`、`>=`、`==`、`!=`、`min`、`max`、`abs`。
3. Bitwise：`&`、`|`、`^`、`~`。
4. String / Name / Text：append、equal、not equal 等普通蓝图可见 op-like 节点。
5. Struct math：Vector、Vector2D、Vector4、Rotator、Quat、Transform、Matrix、LinearColor、Color、Timespan、FrameNumber 等。
6. Object / Class equality：普通蓝图可见的 `==` / `!=` 节点。

## 建议搜索命令

在引擎根目录执行：

```powershell
rg -n "GetAllOpNames|GetComparisonOpNames|GetOperatorSpawner|RegisterOperatorSpawner|FindBestMatchingFunc|GetOpNameFromFunction" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "CommutativeAssociativeBinaryOperator" Source -g "*.h" -g "*.cpp"
rg -n "ScriptOperator" Source -g "*.h" -g "*.cpp"
rg -n "CompactNodeTitle\\s*=\\s*\"(\\+|-|\\*|/|%|==|!=|<|<=|>|>=|&|\\||\\^|~|AND|OR|XOR|NAND|NOR|NOT|MIN|MAX|ABS)\"" Source -g "*.h" -g "*.cpp"
rg -n "BlueprintAutocast|IgnoreTypePromotion" Source/Runtime Source/Editor -g "*.h" -g "*.cpp"
rg -n "UK2Node_CommutativeAssociativeBinaryOperator|UK2Node_PromotableOperator|UK2Node_CallFunction" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
```

如正则在 PowerShell 中转义不稳定，可以拆成多次简单搜索：

```powershell
rg -n "CompactNodeTitle" Source/Runtime Source/Editor -g "*.h" -g "*.cpp"
rg -n "DisplayName = \".*\\+|DisplayName = \".*-|DisplayName = \".*\\*|DisplayName = \".*/|DisplayName = \".*%" Source/Runtime Source/Editor -g "*.h" -g "*.cpp"
rg -n "Equal|Not Equal|Greater|Less|AND|OR|XOR|MIN|MAX|ABS|Bitwise" Source/Runtime/Engine/Classes/Kismet Source/Runtime/TimeManagement -g "*.h" -g "*.cpp"
```

## 交付物

请输出一个 Markdown 或 CSV 表，路径建议：

```text
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_OpCoverage_UESourceReadResult_20260525_CN.md
```

表字段必须包含：

| 字段 | 含义 |
| --- | --- |
| `public_operation` | GraphWrite 面向 agent 的 operation 名称，例如 `add`、`boolean_and`、`modulo`。 |
| `aliases` | 可接受 token，例如 `+`、`add`、`&&`、`and`。 |
| `ue_spawn_path` | `type_promotion` / `commutative_function` / `call_function_compact` / `special_k2_node` / `excluded`。 |
| `ue_operation_name` | TypePromotion 操作名，例如 `Add`、`EqualEqual`；不适用时留空。 |
| `owner_class` | 函数 owner，例如 `UKismetMathLibrary`。 |
| `function_name` | 具体 UFUNCTION 名，例如 `BooleanAND`、`Add_VectorVector`。 |
| `node_class` | 生成节点类，例如 `UK2Node_PromotableOperator`、`UK2Node_CommutativeAssociativeBinaryOperator`、`UK2Node_CallFunction`。 |
| `metadata` | 相关 metadata，例如 `CompactNodeTitle`、`ScriptOperator`、`CommutativeAssociativeBinaryOperator`、`IgnoreTypePromotion`。 |
| `required_evidence` | GraphWrite 生成时最小证据，例如 typed pins、stable function id、owner class、graph capability。 |
| `ambiguity_rule` | 多候选时的处理规则。 |
| `included_in_graphwrite_op` | `yes` / `no`。 |
| `exclude_reason` | 排除原因；纳入时留空。 |
| `source_path` | 以引擎根为根的源码路径，例如 `Source/Runtime/Engine/Classes/Kismet/KismetMathLibrary.h:234`。 |

## 分类规则

### `type_promotion`

满足以下条件时归类为 `type_promotion`：

1. UE 顶层 operation 出现在 `FTypePromotion::GetAllOpNames()`。
2. GraphWrite 可通过 `FTypePromotion::GetOperatorSpawner(OpName)` spawn。
3. 多类型匹配依赖 `FTypePromotion::FindBestMatchingFunc()` 和 pin evidence。

这类目前已知至少包含：

```text
Add
Multiply
Subtract
Divide
Greater
GreaterEqual
Less
LessEqual
NotEqual
EqualEqual
```

### `commutative_function`

满足以下条件时归类为 `commutative_function`：

1. 函数有 `CommutativeAssociativeBinaryOperator = "true"` metadata。
2. ActionDatabase 会为其生成 `UK2Node_CommutativeAssociativeBinaryOperator`。
3. 普通蓝图中用户可见为可扩展输入 pin 的 operator-like 节点。

这类必须独立于 `type_promotion` 统计，因为它未必出现在 `FTypePromotion::GetAllOpNames()`。

### `call_function_compact`

满足以下条件时归类为 `call_function_compact`：

1. 函数没有进入 TypePromotion 顶层 operator。
2. 函数通过普通 `UK2Node_CallFunction` 生成。
3. `CompactNodeTitle` / `ScriptOperator` 使其在普通蓝图中表现为 operator-like。

这类 GraphWrite 不应 direct map 到 `FTypePromotion::GetOperatorSpawner()`，而应通过 ActionDatabase / callable evidence 查询。

### `excluded`

满足以下条件时归类为 `excluded`：

1. `BlueprintAutocast`：归 `convert_function`。
2. latent、async、schedule：归 function operation。
3. 编辑器内部、非普通蓝图 Action Menu、仅编译器内部生成的节点。
4. 无法在普通蓝图中作为用户可 spawn 节点出现的函数。

## 最终判断要求

请在结果末尾给出四个结论：

1. UE 5.6 普通蓝图 op-like 操作总数。
2. 其中应由 `type_promotion` 支持的数量。
3. 其中应由 ActionDatabase callable 查询支持的数量。
4. 当前 BlueprintHelper 固定 10 个 TypePromotion op 是否足以满足“所有普通蓝图 op 操作”。

预期结论应直接回答：

```text
当前固定 10 个 TypePromotion op 只能覆盖基础子集，不能视为所有普通蓝图 op 操作全集。
```

## GraphWrite 后续实现边界

源码读取结果会用于后续 GraphWrite 设计，但本任务不实现。后续实现应遵守以下边界：

1. `kind=op` 的 public schema 保持强类型 operation，不新增宽泛 intent 字段。
2. agent token 只能作为别名入口，不能作为最终 UE 生成证据。
3. 最终生成必须解析到 UE evidence：node class、function stable id 或 TypePromotion operation、pin evidence。
4. 多候选不允许静默选第一个，必须要求 typed pins 或 stable id。
5. TypePromotion 10 个顶层 op 可保持 direct operator spawner 路径。
6. 其他普通蓝图 op-like 函数应走 ActionDatabase / callable 查询链路。
7. `BlueprintAutocast` 仍归 `convert_function`，不混入 `op`。
