---
项目：BlueprintHelper
版本目标：v0.3.0 当前源码包
生成日期：2026-04-30
适用范围：UE5.3+ BlueprintHelper 插件、MCP Server、UE Bridge、Agent 工作流
---

# LogicJson、LogicMD 与 AgentImportGraph 测试

## 通用测试前提

- 使用独立测试工程或 `/Game/BlueprintHelperTest/` 测试目录，不直接操作正式项目资产。
- 测试资产命名建议：
  - Blueprint：`/Game/BlueprintHelperTest/BP_BH_FunctionalActor.BP_BH_FunctionalActor`
  - WidgetBlueprint：`/Game/BlueprintHelperTest/WBP_BH_Test.WBP_BH_Test`
  - DataAsset：`/Game/BlueprintHelperTest/DA_BH_TestConfig.DA_BH_TestConfig`
  - DataTable：`/Game/BlueprintHelperTest/DT_BH_TestItems.DT_BH_TestItems`
- 除生命周期和构建测试外，默认 Unreal Editor 已启动，BlueprintHelper Bridge 可连接到 `BRIDGE_HOST:BRIDGE_PORT`。
- 所有破坏性测试必须在测试资产上执行；删除、导入、控制台命令、关闭编辑器、构建工程属于高风险或关键风险步骤。
- 写操作执行顺序默认是：读取当前状态 → 生成最小写入计划 → 执行写入 → 编译/校验 → 按需保存 → 读取回验。


## 覆盖对象

- `blueprint_get_logic`
- `blueprint_get_logic_json`
- `blueprint_import_agent_graph`
- `BlueprintHelperLogicProcessor`
- `BlueprintHelperAgentImportService`
- v0.3.0 通信优化：LogicMD、LogicJson、AgentImportGraph、Token 减负。

## AgentImportGraph 基础样例

```json
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "/Game/BlueprintHelperTest/BP_BH_FunctionalActor.BP_BH_FunctionalActor",
  "target_graph": "EventGraph",
  "mode": "append",
  "layout": "auto",
  "nodes": [
    { "id": "begin_play", "kind": "event", "event": "ReceiveBeginPlay" },
    { "id": "print", "kind": "call", "function": "/Script/Engine.KismetSystemLibrary:PrintString", "inputs": { "InString": "Hello from Agent" } }
  ],
  "links": [
    { "kind": "exec", "from": "begin_play.then", "to": "print.execute" }
  ],
  "options": { "compile": true, "save": false, "strict": true }
}
```

## 测试用例

| ID | 功能 | 步骤 | 期望结果 | 优先级 |
|---|---|---|---|---|
| LOG-001 | LogicMD 基础读取 | 调用 `blueprint_get_logic`，传目标蓝图和 EventGraph | 返回 Markdown 摘要；包含函数/事件/分支/调用关系；`importable=false` | P0 |
| LOG-002 | LogicMD scope 控制 | 分别测试 `scope=blueprint`、`scope=graph` | graph 只返回目标图；blueprint 返回全蓝图摘要 | P1 |
| LOG-003 | LogicMD detail 控制 | 测试不同 `detail` 参数 | 简略模式节省文本；详细模式包含更多 pin/依赖/诊断 | P1 |
| LOG-004 | LogicMD include_data_dependencies | 开启数据依赖 | 输出变量、DataAsset/DataTable 或对象引用依赖信息 | P1 |
| LOG-005 | LogicMD include_orphans | 构造孤立节点并读取 | 开启时显示 orphan；关闭时减少无关上下文 | P1 |
| LOG-006 | LogicJson 基础读取 | 调用 `blueprint_get_logic_json` | 返回结构化 JSON；`format=logic_json`、`schema=BlueprintHelper.LogicJson.v1`、`importable=false` | P0 |
| LOG-007 | LogicJson 节点关系 | 对 branch flow fixture 读取 LogicJson | 结构中能表达 exec/data links、branch true/false、call inputs | P0 |
| LOG-008 | LogicJson 不含 Raw 冗余 | 默认读取 LogicJson | 不返回完整 RawJson 大包；除非显式 raw/type flags 允许 | P0 |
| LOG-009 | LogicJson 位置字段策略 | 默认读取 LogicJson | 不依赖 PosX/PosY；布局字段不污染 Agent 编辑决策 | P1 |
| LOG-010 | LogicJson 诊断字段 | 构造未知/不支持节点 | `diagnostics` 包含 warning，不导致读取失败 | P1 |
| LOG-011 | AgentImport 简单 BeginPlay Print | 导入 `simple_beginplay_print.agent_import.json` 到测试图 | 创建 BeginPlay/PrintString 节点并连接 exec；编译成功 | P0 |
| LOG-012 | AgentImport Branch Flow | 导入 `branch_flow.agent_import.json` | 创建 Branch 与 true/false 两条执行链；LogicJson 回读结构一致 | P0 |
| LOG-013 | AgentImport Set Variable | 导入 `set_variable.agent_import.json`，开启 `create_missing_variables` | 自动声明/创建变量 Health；Set 节点连接成功 | P0 |
| LOG-014 | AgentImport 禁止 Pos 字段 | 导入 `forbidden_pos.strict.agent_import.json`，`strict=true` | 返回 contract validation failed；不得创建节点 | P0 |
| LOG-015 | AgentImport 无效 pin | 导入 `invalid_pin.agent_import.json` | 失败并回滚；错误定位 `print_hello.not_a_pin` | P0 |
| LOG-016 | AgentImport node kind 覆盖 | 分别测试 `event`、`custom_event`、`call`、`get`、`set`、`branch`、`sequence`、`comment` | 每种 kind 能正确创建或对不支持字段给出明确错误 | P0 |
| LOG-017 | AgentImport mode append | 在已有图上 append | 不删除原有节点；新节点自动布局；回读能区分新增节点 | P0 |
| LOG-018 | AgentImport mode replace/clear 风险 | 如实现支持 replace/clear，则在测试图执行 | 删除范围只限目标图；失败回滚；需要人工确认 | P1 |
| LOG-019 | options.compile | `compile=true` 与 `compile=false` 分别导入 | compile=true 自动编译并返回诊断；false 不触发编译 | P1 |
| LOG-020 | options.save | `save=false` 导入 | 不保存资产；dirty 状态可被 save_asset 捕获 | P1 |
| LOG-021 | Token 对比 | 同一 10KB 蓝图分别调用 RawJson、LogicJson、LogicMD | LogicJson/LogicMD Token 明显低于 RawJson；记录字节数、字符数、估算 token | P1 |
| LOG-022 | MCP response mode | 对支持 response mode 的逻辑读取测试 `summary_text`、`structured_json`、`resource_ref`、`legacy_text_json` | 各模式返回格式符合预期；resource_ref 可被后续读取 | P2 |

## 验收标准

- 默认 Agent 阅读路径不再依赖 RawJson。
- Logic 输出必须清晰标记不可导入。
- AgentImportGraph 是面向 Agent 的简化写入协议，不接受布局坐标类字段作为必要输入。
- 导入失败必须保留足够错误上下文和安全摘要。
