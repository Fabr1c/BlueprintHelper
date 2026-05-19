# FunctionChainContext 自定义逻辑追读工具计划

日期：2026-05-17

## 0. 当前落地状态（2026-05-17）

状态：已完成（第一阶段 K2 Blueprint 读侧工具）。

已落地范围：

- Agent-facing 工具 `blueprinthelper_read_function_chain_context` 已注册到 task-core / CLI 默认读侧工具面，内部 Bridge 命令为 `read_function_chain_context`。
- UE 侧新增 `Shared/FunctionChain` 服务、DTO 和 traversal utils；Bridge Router 只做 payload 转换和 ToolResult 包装，FunctionChain service 负责 orchestration，遍历/过滤逻辑在独立 utils。
- 返回结果保持紧凑：根字段只有 `schema`、`custom_logic_refs`、`summary`、`unresolved`、`ambiguous`；不返回 `entry`、`target`、`query`、owner 字段或 raw GUID。
- 遍历覆盖同资产自定义函数、纯函数数据依赖、跨资产 Blueprint 函数、递归 cycle、max_depth 截断、interface ambiguous、Engine/native utility 过滤和项目 C++ native terminal 计数。
- 未接入废弃 MCP 普通工具；普通入口保持 CLI / task-core / Bridge，MCP 只保留编辑器生命周期/开发执行边界。

本轮发现并修复的问题：

- 初版 native 分类会把项目 C++ `BlueprintCallable` 调用因为 `OwnerBlueprint == null` 误计入 Engine/trusted 过滤数量；已改为按 `/Script/<ProjectName>` 判断项目 native，并计入 `project_native_terminal_calls`。
- UE automation 补充 `CountsProjectNativeTerminalCall`，防止该计数回退。
- UE 命令行一次拼接多个 `Automation RunTests` 时后续命令会被识别为 Unknown automation command；相关 contract 测试已拆成独立运行记录。
- 2026-05-17 二次收口：按最终返回字段约定，移除 FunctionChain agent-facing 输出中的 `node_ref` / `node_path`；调用点或 block 级定位不属于本工具 v1 输出。
- 2026-05-17 三次收口：按返回 schema 短名规则，将 FunctionChain payload schema 收口为 `FunctionChainContext.v1`；task-core 对旧 Bridge payload 做兼容归一化，但新 UE 输出和文档均使用短名。

验证记录：

| 验证项 | 结果 | 证据 |
| --- | --- | --- |
| task-core build | 通过 | `npm.cmd run build` |
| task-core node tests | 111/111 通过 | `npm.cmd run test:node` |
| CLI build | 通过 | `node ..\scripts\clean-build.mjs` + `node ..\scripts\run-tsc.mjs` |
| CLI node tests | 32/32 通过 | `npm.cmd run test:node` |
| UE build | 通过 | `D:\UEProjects\Template\Saved\BuildLogs\UBT-FunctionChain-20260517-r7.log` |
| FunctionChain automation | 6/6 通过 | `D:\UEProjects\Template\Saved\Automation\FunctionChain_20260517_005\index.json` |
| payload contract automation | 1/1 通过 | `D:\UEProjects\Template\Saved\Automation\FunctionChain_20260517_contract_001\index.json` |
| pack shape contract automation | 1/1 通过 | `D:\UEProjects\Template\Saved\Automation\FunctionChain_20260517_contract_005\index.json` |
| route planner automation | 1/1 通过 | `D:\UEProjects\Template\Saved\Automation\FunctionChain_20260517_contract_003\index.json` |

验收结论：

- 计划中的第一阶段完整期望已满足。
- CLI smoke 在本轮采用分层自动化覆盖：CLI direct dispatch 测试确认 Agent-facing 命令、默认字段和 Bridge command；UE automation transient fixture 确认 Controller/Pawn 跨资产、自定义纯函数、Engine 过滤等运行时行为；Bridge/contract automation 确认 payload、返回 shape 和路由。
- 没有剩余 blocker。后续 Material Graph、AnimGraph、dispatcher 静态接收方展开、可信插件配置化过滤可作为单独扩展阶段处理。

## 1. 目标

新增一个读侧聚合工具，用于从指定 Blueprint `function` / `event` / `custom_event` 入口开始，追踪该入口实际调用到的项目自定义逻辑，并返回 Agent 可继续精确读取的最小定位数组。

该工具解决的问题是：Agent 在 A 资产中看到调用了 B 资产的自定义函数时，不应再反复调用依赖工具猜函数所在资产、图和入口，而应一次拿到功能链路中的自定义逻辑索引，再按索引调用现有 `read_context` 精读函数体。

## 2. 非目标

- 不做写入，不创建或修改 Blueprint。
- 不替代 `read_reference_context`；本工具追读调用链，`read_reference_context` 仍用于依赖、引用和高风险操作预检。
- 不返回请求回显字段，例如 `entry`、`target`、`query`。
- 不返回 `owner`、`owner_asset_path`、`owner_kind` 等字段；被调用自定义逻辑的定位资产就是 `asset_path`。
- 不返回 UE Engine 或可信插件封装函数列表；这些只进入过滤数量摘要。
- 不返回调用点定位字段，例如 `node_ref`、`node_path`、raw node guid。该工具只返回可继续精读的自定义逻辑入口索引。
- 不声明 Material Graph / AnimGraph 支持；第一阶段只覆盖 K2 Blueprint 图。

## 3. 工具命名

建议 Agent-facing 工具名：

```text
blueprinthelper_read_function_chain_context
```

Bridge 内部命令名：

```text
read_function_chain_context
```

返回 schema：

```text
FunctionChainContext.v1
```

## 4. 请求字段

```json
{
  "asset_path": "/Game/BP_PlayerController",
  "target_type": "event",
  "target_name": "Input_Fire",
  "graph_name": "EventGraph",
  "max_depth": 3,
  "include_data_dependencies": true,
  "expand_cross_asset": true
}
```

字段规则：

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| `asset_path` | 是 | 起点 Blueprint 资产路径。 |
| `target_type` | 是 | `function`、`event`、`custom_event`。后续可扩展 `interface_event`。 |
| `target_name` | 是 | 起点函数或事件名。 |
| `graph_name` | 否 | 用于事件、CustomEvent 消歧。函数可从函数图解析。 |
| `max_depth` | 否 | 默认 3。跨自定义函数递归展开深度。 |
| `include_data_dependencies` | 否 | 默认 true。Branch 条件、参数来源、Return 值来源里的自定义纯函数必须纳入。 |
| `expand_cross_asset` | 否 | 默认 true。允许从 Controller 追到 Pawn、Weapon、Projectile 等项目 Blueprint。 |

不加入请求字段：

- `target_guid`：普通 Agent 不知道 GUID。
- `owner_asset_path`：可由起点和节点解析得到，不让 Agent 维护。
- `include_engine_calls`：Engine / 可信插件函数不返回明细，避免污染主数组。

## 5. 返回字段

```json
{
  "schema": "FunctionChainContext.v1",
  "custom_logic_refs": [
    {
      "order": 1,
      "depth": 1,
      "parent_order": 0,
      "asset_path": "/Game/BP_Weapon",
      "target_type": "function",
      "target_name": "CanFire",
      "graph_name": "CanFire",
      "call_kind": "pure_function",
      "reason": "branch_condition"
    },
    {
      "order": 2,
      "depth": 1,
      "parent_order": 0,
      "asset_path": "/Game/BP_Weapon",
      "target_type": "function",
      "target_name": "Fire",
      "graph_name": "Fire",
      "call_kind": "impure_function",
      "reason": "exec_call"
    }
  ],
  "summary": {
    "visited_nodes": 42,
    "returned_custom_refs": 2,
    "filtered_engine_or_trusted_plugin_calls": 17,
    "filtered_native_pure_calls": 8,
    "project_native_terminal_calls": 1,
    "unresolved_calls": 1,
    "ambiguous_calls": 0,
    "cycle_count": 0,
    "truncated": false
  },
  "unresolved": [],
  "ambiguous": []
}
```

### `custom_logic_refs[]`

只返回可继续精读的项目自定义逻辑入口：

- Blueprint 自定义函数，包括纯函数。
- Blueprint Event。
- Blueprint CustomEvent。
- 可解析到唯一项目 Blueprint 实现的接口调用。

字段说明：

| 字段 | 说明 |
| --- | --- |
| `order` | 首次发现顺序。 |
| `depth` | 从起点递归展开的深度。 |
| `parent_order` | 调用来源的 `order`。起点内部发现的第一层可用 0。 |
| `asset_path` | 被调用自定义逻辑所在资产。 |
| `target_type` | `function`、`event`、`custom_event`。 |
| `target_name` | 可传给 `read_context` 的目标名。 |
| `graph_name` | 可传给 `read_context` 的图名；能解析则填。 |
| `call_kind` | `pure_function`、`impure_function`、`event`、`custom_event`、`interface_call`。 |
| `reason` | 为什么进入链路：`exec_call`、`branch_condition`、`argument_source`、`return_value_source`、`set_value_source`。 |

### `summary`

`summary` 负责记录被过滤或无法展开的数量，不用返回明细污染 Agent 主上下文。

- `filtered_engine_or_trusted_plugin_calls`：UE Engine 和可信插件封装函数数量。
- `filtered_native_pure_calls`：被折叠的 native utility / pure 函数数量。
- `project_native_terminal_calls`：项目 C++ `BlueprintCallable` 数量。第一阶段不作为 Blueprint 自定义逻辑返回，因为没有 Blueprint 函数体；后续可单独设计源码定位扩展。
- `unresolved_calls`：无法解析目标的调用数量。
- `ambiguous_calls`：多个项目自定义候选，工具不能安全选择。
- `cycle_count`：递归中发现循环调用的次数。
- `truncated`：达到 `max_depth` 或节点预算。

## 6. 过滤规则

默认折叠：

- `/Script/Engine`、`/Script/CoreUObject`、`/Script/UMG`、`/Script/GameplayTags` 等 UE Engine 模块函数。
- BlueprintHelper 内部可信插件函数。
- 用户配置为 trusted plugin 的插件函数。
- Native utility pure 函数，例如数学、字符串、容器、基础 Kismet library。

必须保留：

- 项目 Blueprint 自定义纯函数。
- 项目 Blueprint 自定义 impure 函数。
- 项目 Blueprint Event / CustomEvent。
- 影响分支条件、参数、Return、Set 值来源的项目自定义纯函数。

不安全时不猜：

- Interface 调用无法唯一确定实现者时进入 `ambiguous`。
- Dispatcher broadcast / bind 无法静态确定接收方时进入 `unresolved` 或后续专门的 dispatcher 链路。
- Dynamic Spawn、SoftClass、GetActorOfClass、运行时 Cast 只能返回可证明的候选，不能当作唯一目标。

## 7. 遍历策略

1. 解析起点资产和目标入口。
2. 读取目标函数或事件图中的 K2 节点。
3. 按执行流遍历节点。
4. 当 `include_data_dependencies=true` 时，同时追踪：
   - Branch 条件。
   - Call 参数来源。
   - Return 值来源。
   - Set 变量值来源。
5. 遇到 `UK2Node_CallFunction` 时解析 `UFunction`：
   - Blueprint 自定义函数：加入 `custom_logic_refs` 并按 `max_depth` 递归。
   - Engine / trusted plugin native：过滤计数。
   - 项目 C++ native：计入 `project_native_terminal_calls`，第一阶段不返回为自定义 Blueprint 逻辑。
6. 遇到 CustomEvent 调用或同资产函数调用时解析到对应图入口。
7. 遇到 Interface / Dispatcher / BoundEvent / Dynamic target 时按确定性分类：
   - 唯一项目 Blueprint 目标：加入数组。
   - 多候选或运行时目标：进入 `ambiguous` / `unresolved`。
8. 用 visited key 防止递归循环：
   - `asset_path + target_type + target_name + graph_name`。

## 8. 与现有读工具的关系

本工具只返回索引，不返回完整函数体。Agent 后续按 `custom_logic_refs[]` 调用现有读工具精读：

```json
{
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/BP_Weapon",
    "target_type": "function",
    "target_name": "CanFire"
  },
  "view": {
    "format": "logic_md"
  }
}
```

因此它应复用现有 `read_context` 的目标入口读取能力，而不是重复输出大段 graph JSON。

## 9. 实现边界

建议新增独立服务和类型，避免继续膨胀 Bridge Router 或现有 dependency service：

- `Shared/FunctionChain/BlueprintHelperFunctionChainContextTypes.h/.cpp`
- `Shared/FunctionChain/BlueprintHelperFunctionChainContextService.h/.cpp`
- `Shared/FunctionChain/Utils/BlueprintHelperFunctionChainTraversalUtils.h/.cpp`
- Bridge route：`read_function_chain_context`
- AgentFace schema / handler：`blueprinthelper_read_function_chain_context`

架构规则：

- Service 只做 orchestration。
- 解析、过滤、遍历、结果整形分别放到职责明确的 utility/coordinator。
- 不新增 `.cpp` 本地大型 `LocalUtils`。
- 不把该工具塞进 MCP 废弃工具集；普通入口走 CLI / task-core / Bridge。

## 10. 测试计划

### task-core Node 测试

- schema 接受最小请求。
- schema 拒绝 `target_guid`。
- 返回 schema 不包含 `entry`、`owner`、`owner_asset_path`、`query`。
- `custom_logic_refs[]` 允许 `pure_function`。

### UE Automation

1. 同资产事件调用自定义 impure 函数，返回该函数。
2. Branch 条件调用自定义纯函数，返回 `call_kind=pure_function`、`reason=branch_condition`。
3. Controller 调 Pawn 自定义函数，返回 Pawn 资产定位。
4. Engine native 调用被过滤，只增加 `filtered_engine_or_trusted_plugin_calls`。
5. 自定义函数互相递归时不死循环，设置 `cycle_count`。
6. Interface 多实现时返回 `ambiguous`，不猜目标。
7. 达到 `max_depth` 时 `summary.truncated=true`。

### CLI smoke

- 使用一个小型 Controller / Pawn fixture：
  - Controller 输入事件调用 Pawn `TryInteract`。
  - Pawn `TryInteract` 内调用纯函数 `CanInteract`。
  - `CanInteract` 内调用 Engine native 工具函数。
- 期望 `custom_logic_refs` 只包含 `TryInteract`、`CanInteract`，Engine 函数只进入 summary 计数。

## 11. 验收标准

- 普通 Agent 能从一个 Controller 或 Pawn 的入口事件开始，一次拿到后续可精读的项目自定义函数/事件索引。
- 返回结果不包含请求回显、owner 字段、GUID-first 字段、Engine/native 封装函数明细。
- 自定义纯函数不会被过滤。
- 无法唯一确定的跨资产目标不会被误选。
- 结果可直接驱动后续 `read_context target_type=function/event/custom_event` 调用。
