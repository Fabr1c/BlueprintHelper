# BlueprintHelper 插件隐患登记文档（P 等级分级）

- 文档编号：`Module_BlueprintHelper_RiskRegister_PLevels_20260430`
- 适用版本：BlueprintHelper v0.3.0r / UE 5.3+
- 日期：2026-04-30
- 范围：本文件登记当前插件在 MCP / Bridge / Blueprint 导入导出 / UMG / DataTable / UObject / 打包与测试方面的潜在隐患。
- 不重复登记项：RawJson / LogicJson / LogicMD 返回格式优化、导入 JSON 去除 Pos 并由插件布局、Links 缺少 PinType、改动审阅面板等已经单独立项的内容。本文件仅登记除这些既有优化项以外的问题。

---

## 1. P 等级定义

| 等级 | 含义 | 处理策略 |
|---|---|---|
| P0 | 可能导致错误写入、资产损坏、安全越权、Agent 误判成功的阻断级隐患。 | 立即修复；修复前不建议扩大工具使用范围。 |
| P1 | 高概率影响 Agent 编辑可靠性、蓝图语义正确性、事务一致性或长期可维护性的高优先级隐患。 | 进入最近一个版本迭代；需要配套测试。 |
| P2 | 中等风险，主要影响复杂项目、复杂资产、大数据量、并发请求或发布质量。 | 排入版本计划；可以分批修复。 |
| P3 | 低风险或工程卫生类问题，不一定直接造成错误，但会增加维护成本、排障成本或发布不确定性。 | 在版本整理、文档完善、CI 建设时处理。 |

---

## 2. P0 隐患

### P0-01：MCP `blueprint_export_to_json` 的 scope 与 Bridge 实际识别值不一致

**问题描述**  
MCP 层的 schema 暴露 `full_graph` / `selection` 等 scope，但 Bridge 侧逻辑存在按 `full_blueprint` 分支判断的实现差异。Agent 通过 MCP 请求完整蓝图导出时，可能无法触发预期导出范围。

**影响**
- Agent 以为读取了完整蓝图，实际只读取了图表或选择集。
- 后续编辑、校验、增量修改基于错误上下文，可能误删、误连或漏改节点。
- LogicJson / RawJson / LogicMD 的上层优化会受到底层 scope 不一致影响。

**触发场景**
- Agent 调用 `blueprint_export_to_json` 并设置完整蓝图导出。
- MCP 层与 Bridge 层枚举值未完全对齐。

**建议修复**
- 统一 scope 枚举为：`blueprint` / `graph` / `selection`。
- 对历史值做兼容映射：`full_blueprint -> blueprint`，`full_graph -> graph`。
- Validate 阶段返回实际生效的 scope，例如 `effective_scope`。

**验收标准**
- MCP 请求完整蓝图导出时，Bridge 返回明确的 `effective_scope: "blueprint"`。
- 旧参数不会静默失败；若无法兼容，返回 `invalid_scope`。

---

### P0-02：显式指定 graph 名错误时可能回退到 EventGraph

**问题描述**  
写操作中，如果 Agent 显式传入 `target_graph` / `graph_name`，但插件没有找到对应图表，当前部分路径可能回退到默认 EventGraph。

**影响**
- 目标图表拼写错误会变成对 EventGraph 的误写。
- 蓝图资产可能被错误修改，且 Agent 不易立即发现。
- 对函数图、宏图、Widget Blueprint 的自动编辑尤其危险。

**触发场景**
- Agent 传入错误 graph 名。
- 蓝图存在 EventGraph，插件 fallback 到默认图。

**建议修复**
- 写操作规则：只要请求显式指定 graph，找不到就 hard fail。
- 只有请求完全未指定 graph 时，才允许默认 EventGraph。
- 错误返回中给出可用 graph 列表。

**验收标准**
- `target_graph="EventGrph"` 不会写入 EventGraph。
- 返回 `graph_not_found`，并附带 `available_graphs`。

---

### P0-03：Bridge 无认证，且暴露高危命令

**问题描述**  
Bridge 默认监听本地端口，例如 `127.0.0.1:54321`。本地其他进程理论上可以连接 Bridge 并调用资产写入、控制台命令、关闭编辑器等高危操作。

**影响**
- 本机任意进程可能误用或滥用 Bridge。
- `exec_console_command`、`close_editor`、资产写入命令存在越权风险。
- Agent 工具链越复杂，本地信任边界越不稳定。

**触发场景**
- 多个 Agent / IDE / 脚本同时运行。
- 本地恶意或误配置进程扫描 localhost 端口。
- Bridge 未设置 token / nonce / origin 校验。

**建议修复**
- Bridge 启动时生成 session token，并由 MCPServer 通过环境变量或握手获取。
- 所有写命令和高危命令必须携带 token。
- `exec_console_command`、`close_editor` 默认关闭，通过配置显式启用。

**验收标准**
- 未携带 token 的请求返回 `unauthorized`。
- 高危命令在默认配置下返回 `command_disabled`。

---

### P0-04：Bridge 请求解析缺少统一 schema 校验

**问题描述**  
部分请求解析依赖 `GetStringField`、`GetObjectField` 等强取字段方式。缺字段、null、类型不匹配时会产生日志警告或非预期行为。

**影响**
- Agent 传入 `null` 或错误类型时，插件可能继续执行。
- 错误在 UE 日志中出现，但 MCP 结果不一定清晰。
- 可能出现“请求无效但局部写入已经发生”的问题。

**触发场景**
- `target_graph: null`。
- `asset_path` 缺失或不是字符串。
- `operations` 结构类型错误。

**建议修复**
- 建立统一 `FBlueprintHelperRequestValidator`。
- 所有命令入口先执行 schema 校验，再进入业务逻辑。
- 使用 `TryGetStringField` / `TryGetBoolField` / `TryGetArrayField`，禁止直接强取。

**验收标准**
- 错误请求返回结构化错误：`invalid_request`、`field`、`expected_type`、`actual_type`。
- 错误请求不会触发任何资产写入。

---

### P0-05：Validate 与 Import 的 graph 作用域不一致

**问题描述**  
校验阶段可能将多个 graph 的 node id 放入全局集合，而导入阶段按 graph 独立生成和连接。这样会造成校验和实际导入语义不一致。

**影响**
- 校验通过但导入失败。
- 不同 graph 中合法的相同 node id 被误判重复。
- 跨 graph link 没有明确禁止或明确语义。

**触发场景**
- JSON 中包含多个 graph。
- 不同 graph 使用相同局部 node id。
- link 引用没有 graph 前缀。

**建议修复**
- node id、link from/to 均改为 graph-scoped。
- 跨 graph link 默认禁止，除非协议显式支持。
- Validate 输出每个 graph 的独立诊断。

**验收标准**
- GraphA.Node_1 与 GraphB.Node_1 不会互相冲突。
- 跨 graph link 返回 `cross_graph_link_not_supported`。

---

### P0-06：Import 成功语义不严谨，可能造成 Agent 误判

**问题描述**  
导入命令可能在 operation-only、空图、部分节点失败、仅 warning 等情况下返回看似成功的结果。Agent 可能误以为蓝图已按预期生成。

**影响**
- Agent 后续步骤基于错误状态继续执行。
- 用户看到“成功”，但蓝图实际没有完整修改。
- 部分成功不易回滚或复现。

**触发场景**
- 导入 payload 只有 operations，没有 nodes。
- 节点生成失败但部分 operation 成功。
- link 连接失败但节点已经生成。

**建议修复**
- 成功结果拆分为：`operations_applied`、`nodes_created`、`links_connected`、`warnings`、`errors`。
- 默认 `strict=true`：存在 error 时事务回滚。
- 允许 `allow_partial=true`，但必须显式请求。

**验收标准**
- link 失败时默认回滚节点创建。
- 返回结果能明确区分 full success / partial success / no-op / failed。

---

## 3. P1 隐患

### P1-01：`existing_node_refs` 通过 title 模糊匹配节点不安全

**问题描述**  
现有节点引用可通过 title 匹配。蓝图中多个节点显示标题可能完全一致，例如多个 `Branch`、`Print String`、函数调用显示名重复。

**影响**
- Agent 可能把新连线接到错误节点。
- 修改复杂图时存在隐性逻辑破坏。

**建议修复**
- 写操作只接受稳定 `node_guid`。
- title 匹配只用于只读诊断。
- 多候选时返回 `ambiguous_node_ref`。

---

### P1-02：已有节点被统一 `ReconstructNode`，可能破坏已有连线或 pin 状态

**问题描述**  
已有节点引用与新生成节点可能进入同一后处理流程，并被统一 reconstruct。

**影响**
- 已有节点 pin 重新生成。
- 原连线、默认值、隐藏 pin、展开 pin 状态可能变化。

**建议修复**
- 区分 `CreatedNodes` 与 `ReferencedExistingNodes`。
- 默认只 reconstruct 新节点。
- 对已有节点 reconstruct 需要显式参数，例如 `reconstruct_existing_nodes=true`。

---

### P1-03：函数节点解析不够稳定

**问题描述**  
`CallFunction` 节点解析主要依赖函数名或显示名，缺少 owner class、target class、module、签名约束。

**影响**
- 同名函数、重载式 K2 节点、BlueprintFunctionLibrary 函数可能解析错误。
- Agent 生成的图编译失败或逻辑错误。

**建议修复**
- 导入协议增加 `owner_class`、`target_type`、`function_name`、可选 `signature_hash`。
- 函数候选多于一个时 hard fail。

---

### P1-04：默认值应用顺序可能被 reconstruct 覆盖

**问题描述**  
节点创建后，如果先设置 pin 默认值，再执行 `ReconstructNode`，部分 UK2Node 会重建 pin，导致默认值丢失或 pin 名变化。

**影响**
- Agent 以为默认值已写入，实际节点值恢复默认。
- 编译未必失败，但运行逻辑错误。

**建议修复**
- 标准流程调整为：Spawn Node → 设置类型/函数 → AllocatePins → ReconstructNode → 解析 pin alias → ApplyDefaultValues → Link。

---

### P1-05：默认值设置失败缺少结构化诊断

**问题描述**  
当 pin 找不到、类型转换失败或默认值格式非法时，当前结果不一定明确告诉 Agent 哪些值没有写入。

**影响**
- Agent 难以自动修复导入 JSON。
- 用户难以定位生成图的语义偏差。

**建议修复**
- 默认值应用返回 `applied`、`missing_pins`、`type_conversion_errors`。
- strict 模式下，任何默认值失败都回滚事务。

---

### P1-06：函数图 / 宏图 / 事件分发器 pin type 转换失败可能静默退化

**问题描述**  
变量添加逻辑会检查类型转换结果，但函数图、宏图、事件分发器部分路径可能没有统一检查 pin type 转换是否成功。

**影响**
- 错误类型可能退化为 bool 或默认类型。
- 后续 Agent 读取到的图与用户意图不一致。

**建议修复**
- 统一 pin type 解析器。
- 所有类型转换失败均返回 `invalid_pin_type`，不能 fallback。

---

### P1-07：同名函数图存在时直接视为成功，但不校验签名

**问题描述**  
如果已有同名函数图，工具可能直接返回成功，而不比较输入、输出、本地变量或 metadata。

**影响**
- Agent 以为目标函数结构已满足要求，实际签名不匹配。
- 后续节点调用或接口适配失败。

**建议修复**
- 同名函数图必须比较 signature。
- 签名不一致返回 `graph_signature_conflict`。

---

### P1-08：DeleteNodes 使用 `Node_0` 这类不稳定 id

**问题描述**  
`Node_i` 依赖 Graph Nodes 数组顺序。蓝图修改、重载、复制粘贴后，索引不稳定。

**影响**
- 删除错误节点。
- Agent 多步操作之间的 node id 失效。

**建议修复**
- 写操作优先使用 `node_guid`。
- `Node_i` 仅作为只读展示或一次性 UI 临时索引。

---

### P1-09：删除节点部分失败时整体结果不够严格

**问题描述**  
批量删除多个节点时，部分节点不存在、受保护或删除失败，结果可能不够精确。

**影响**
- Agent 不知道哪些节点实际被删。
- 后续导入 / 连接操作基于错误状态继续执行。

**建议修复**
- 返回 `deleted`、`not_found`、`protected`、`failed`。
- strict 模式下，任何失败都回滚。

---

### P1-10：UMG Widget 操作缺少完整事务与失败回滚

**问题描述**  
UMG Add / Remove / Move / SetProperty 等操作可能只调用 `Modify()`，没有统一 `FScopedTransaction`。Move 操作尤其容易出现先移除后添加失败。

**影响**
- Widget 树结构可能半修改。
- Slot 布局数据可能丢失。
- 用户无法通过一次 Undo 恢复 Agent 写入。

**建议修复**
- 每个 UMG 写操作包裹 `FScopedTransaction`。
- MoveWidget 先保存 old parent、old slot、layout data，失败时回滚。

---

### P1-11：Widget Root 删除风险

**问题描述**  
RootWidget 删除属于结构性高危操作。如果先置空 RootWidget 再 RemoveWidget，失败时可能导致蓝图 root 丢失。

**影响**
- Widget Blueprint 结构损坏。
- 设计器无法正常显示或编译异常。

**建议修复**
- Root 删除单独命令或需要 `allow_delete_root=true`。
- 先完整验证，再事务内修改。
- 失败时恢复原 RootWidget。

---

### P1-12：SetObjectProperty 权限过宽

**问题描述**  
读取属性时可能有过滤，但写属性时若找到属性就尝试设置，缺少统一属性 flag 检查。

**影响**
- 可能写入只读、EditConst、Transient 或非编辑期属性。
- DataAsset / UObject 资产可能进入异常状态。

**建议修复**
- set 前检查 `CPF_Edit`、`CPF_BlueprintVisible`、`CPF_BlueprintReadOnly`、`EditConst`、`Transient` 等 flags。
- 默认只允许编辑器中可编辑属性。

---

### P1-13：DataTable update 非原子

**问题描述**  
更新一行多个字段时，前几个字段成功、后续字段失败，可能留下部分修改。

**影响**
- 表数据出现半更新状态。
- Agent 后续无法可靠判断数据是否完整写入。

**建议修复**
- 先复制 row buffer，在副本上导入全部字段。
- 全部字段成功后再替换原 row。
- strict 模式下任意字段失败不落盘。

---

### P1-14：写操作缺少统一 Undo / Transaction 策略

**问题描述**  
蓝图导入已有事务意识，但 UMG、DataTable、UObject、部分资产操作的事务策略不统一。

**影响**
- 用户无法以“一次 Agent 命令 = 一次 Undo”的方式恢复。
- 部分写入失败后难以回滚。

**建议修复**
- 建立 `FBlueprintHelperScopedAssetMutation` 工具层。
- 统一处理 Transaction、Modify、MarkPackageDirty、Compile、Rollback、diagnostics。

---

## 4. P2 隐患

### P2-01：Bridge 单连接串行处理，长命令阻塞后续请求

**问题描述**  
Bridge 接收连接后同步处理请求，命令执行还可能切到 GameThread 等待。大蓝图导出、复杂导入、编译等长命令会阻塞后续请求。

**影响**
- Agent 端出现 timeout。
- 用户误以为编辑器卡死。
- 多工具并发调用时不稳定。

**建议修复**
- 增加 request queue、busy 状态与超时响应。
- 长任务返回 job id，支持查询状态。

---

### P2-02：Node 侧 timeout 与 UE 侧执行没有取消联动

**问题描述**  
MCPServer 超时后，UE 侧命令可能仍在执行。Agent 认为失败，但编辑器稍后完成写入。

**影响**
- Agent 状态与编辑器实际状态分裂。
- 可能出现重复写入或二次修复误操作。

**建议修复**
- 增加 command id。
- 支持 cancel 或超时后标记命令结果为 abandoned。
- UE 侧完成后写入操作日志，供 Agent 查询。

---

### P2-03：Bridge 消息大小上限与大资产分页能力不足

**问题描述**  
复杂蓝图、大 WidgetTree、大 DataTable 可能超过当前消息上限或导致响应过重。LogicJson / LogicMD 可以缓解，但底层协议仍缺分页。

**影响**
- 大资产读取失败。
- Agent 无法稳定处理复杂工程。

**建议修复**
- Bridge 支持分页：`page_size`、`cursor`、`has_more`。
- 大对象支持分块读取，例如 graphs、nodes、datatable rows。

---

### P2-04：每个 MCP 命令新建 TCP 连接，日志噪声与性能开销偏高

**问题描述**  
频繁 connect / disconnect 会产生大量日志，对性能和排障都有影响。

**影响**
- 日志中有效错误被噪声淹没。
- 高频 Agent 调用下性能不稳定。

**建议修复**
- Node 侧连接复用。
- Bridge 日志将连接级别降为 VeryVerbose。

---

### P2-05：AssetBrowse class 过滤过窄

**问题描述**  
资产浏览的 class path 过滤偏硬编码，可能集中在 Engine / UMGEditor / CoreUObject 等路径。游戏模块、插件模块、自定义资产类型可能过滤不准。

**影响**
- Agent 搜不到项目自定义资产。
- 搜索结果不完整导致后续编辑目标错误。

**建议修复**
- 支持完整 class path。
- 支持父类递归过滤。
- 支持 Blueprint Generated Class 反查。

---

### P2-06：SearchAssets 主要依赖名称子串搜索

**问题描述**  
Agent 常需要按路径、类型、tag、package、父类、接口等条件检索资产，单纯名称子串不足。

**影响**
- 大项目中搜索结果过多或遗漏。
- Agent 可能选择错误资产。

**建议修复**
- 增加结构化 filter：`path_prefix`、`class`、`name_contains`、`recursive`、`tags`。

---

### P2-07：active blueprint / active graph 推断不可靠

**问题描述**  
多个 Blueprint Editor 打开时，active context 可能拿到第一个编辑器，而不是用户当前聚焦图表。

**影响**
- 依赖当前上下文的写操作可能写错资产或图表。

**建议修复**
- 写操作 schema 必须要求 `asset_path` + `graph_name`。
- active context 仅用于只读诊断或用户明确要求“当前激活图表”时使用。

---

## 5. P3 隐患

### P3-01：插件版本标识可能不一致

**问题描述**  
发布包命名、`.uplugin`、MCPServer `package.json`、资源文档版本号可能存在不一致。

**影响**
- 用户和 Agent 难以判断当前实际版本。
- 升级、回滚、问题定位成本增加。

**建议修复**
- 统一版本来源，例如 `VERSION` 文件或脚本同步。
- 发布前检查 `.uplugin VersionName`、MCPServer package 版本、文档版本。

---

### P3-02：`FilterPlugin.ini` 可能没有递归包含 Resources 子目录

**问题描述**  
如果打包配置只包含 `/Resources/*.md`，则 `Resources/Plan/...`、`Resources/v0.3.0/...`、测试 fixture、规则文件可能不会进入打包插件。

**影响**
- 用户安装后的插件缺少文档、规则或 fixture。
- Agent 无法读取内置规则资源。

**建议修复**
- 使用递归包含规则，例如 `/Resources/...`、`/MCPServer/...`。
- 打包后自动校验关键资源是否存在。

---

### P3-03：发布包可能包含 `.git`、`Intermediate`、`Binaries`、`node_modules`

**问题描述**  
开发包中可以包含这些目录，但源码发布包不应携带 `.git`、`Intermediate`。`node_modules` 会显著增加包体并带来平台差异。

**影响**
- 发布包过大。
- 用户环境可能受到无关文件影响。
- 许可证、供应链和缓存污染风险增加。

**建议修复**
- 拆分 source-only 包、prebuilt 包、MCPServer 包。
- 发布脚本中明确 exclude 规则。

---

### P3-04：Build.cs editor 依赖暴露偏多

**问题描述**  
部分 Editor 模块可能被放在 PublicDependency，扩大模块耦合面。

**影响**
- 编译依赖变重。
- 后续拆分 Runtime / Editor 模块更困难。

**建议修复**
- 尽量将 `UnrealEd`、`Kismet`、`GraphEditor`、`BlueprintGraph`、`UMGEditor` 等放入 PrivateDependency。
- 若未来需要 Runtime 模块，应拆分 Editor-only 代码。

---

### P3-05：自动化测试体系不足

**问题描述**  
当前更偏文档、fixture 与手动验证，缺少完整自动化回归。Agent 写蓝图属于高风险自动编辑能力，仅靠人工测试不足。

**影响**
- 修复一个导入问题时容易引入另一个回归。
- Agent 修改复杂蓝图时缺少稳定保障。

**建议修复**
- 增加三层测试：
  1. 纯 C++ schema / validation 单元测试。
  2. UE Editor Automation 测试。
  3. MCP 端集成测试。

**最低测试用例**
- 缺字段 / null 字段 / 错类型。
- 跨 graph link。
- 重复 node id。
- pin alias 失败。
- 导入简单图、函数图、宏图。
- 添加变量 / Dispatcher。
- UMG Add / Move / Remove。
- DataTable update rollback。
- open_editor / health / export / validate / import / compile / save。

---

## 6. 建议修复顺序

### 第一阶段：阻断错误写入与安全风险

1. 修复 `export_to_json` scope 不一致。
2. 禁止显式 graph 错误时回退 EventGraph。
3. Bridge 请求统一 schema 校验。
4. Validate / Import 改为 graph-scoped。
5. Bridge 增加 token；高危命令默认禁用。
6. Import 结果改为结构化 full success / partial success / failed。

### 第二阶段：提升 Agent 写入可靠性

1. 写操作改用稳定 `node_guid`。
2. 函数解析增加 owner class / target class / signature。
3. 默认值应用顺序修正。
4. pin type 转换失败 hard fail。
5. 删除、UMG、DataTable、UObject 写操作引入统一事务层。

### 第三阶段：提升复杂项目适配能力

1. Bridge 增加分页、job id、timeout/cancel 语义。
2. AssetBrowse / SearchAssets 增加结构化过滤。
3. active context 降级为只读诊断来源。
4. 建立自动化测试矩阵。

### 第四阶段：发布与维护治理

1. 统一版本号。
2. 修正 FilterPlugin.ini 递归资源包含。
3. 拆分发布包类型。
4. 收敛 Build.cs 依赖。

---

## 7. 汇总表

| ID | 等级 | 模块 | 隐患摘要 | 建议状态 |
|---|---:|---|---|---|
| P0-01 | P0 | MCP / Bridge | export scope 不一致 | 立即修复 |
| P0-02 | P0 | Blueprint 写入 | graph 找不到时回退 EventGraph | 立即修复 |
| P0-03 | P0 | Bridge 安全 | 无认证且暴露高危命令 | 立即修复 |
| P0-04 | P0 | Bridge 协议 | 请求缺少统一 schema 校验 | 立即修复 |
| P0-05 | P0 | Validate / Import | graph 作用域不一致 | 立即修复 |
| P0-06 | P0 | Import | 成功语义不严谨 | 立即修复 |
| P1-01 | P1 | Blueprint 引用 | title 模糊匹配已有节点 | 近期修复 |
| P1-02 | P1 | Blueprint 节点 | 已有节点被 reconstruct | 近期修复 |
| P1-03 | P1 | Function Node | 函数解析不稳定 | 近期修复 |
| P1-04 | P1 | Pin 默认值 | 默认值可能被 reconstruct 覆盖 | 近期修复 |
| P1-05 | P1 | Pin 默认值 | 默认值失败缺少诊断 | 近期修复 |
| P1-06 | P1 | Graph / Dispatcher | pin type 转换失败可能退化 | 近期修复 |
| P1-07 | P1 | Function Graph | 同名函数图不校验签名 | 近期修复 |
| P1-08 | P1 | DeleteNodes | 使用不稳定 Node_i | 近期修复 |
| P1-09 | P1 | DeleteNodes | 部分失败结果不严格 | 近期修复 |
| P1-10 | P1 | UMG | Widget 操作缺少完整事务 | 近期修复 |
| P1-11 | P1 | UMG | RootWidget 删除风险 | 近期修复 |
| P1-12 | P1 | UObject | SetObjectProperty 权限过宽 | 近期修复 |
| P1-13 | P1 | DataTable | update 非原子 | 近期修复 |
| P1-14 | P1 | 写操作公共层 | Undo / Transaction 策略不统一 | 近期修复 |
| P2-01 | P2 | Bridge | 单连接串行阻塞 | 计划修复 |
| P2-02 | P2 | MCP / UE | timeout 无取消联动 | 计划修复 |
| P2-03 | P2 | Bridge | 大资产缺少分页 | 计划修复 |
| P2-04 | P2 | Bridge | TCP 连接复用不足 | 计划修复 |
| P2-05 | P2 | AssetBrowse | class 过滤过窄 | 计划修复 |
| P2-06 | P2 | SearchAssets | 搜索过滤能力不足 | 计划修复 |
| P2-07 | P2 | Context | active graph 推断不可靠 | 计划修复 |
| P3-01 | P3 | 发布 | 版本标识可能不一致 | 整理修复 |
| P3-02 | P3 | 打包 | Resources 子目录可能未递归包含 | 整理修复 |
| P3-03 | P3 | 发布包 | 可能包含开发缓存目录 | 整理修复 |
| P3-04 | P3 | Build.cs | Editor 依赖暴露偏多 | 整理修复 |
| P3-05 | P3 | 测试 | 自动化测试不足 | 整理修复 |

---

## 8. 结论

当前插件的核心能力已经覆盖 Agent 访问和编辑 Unreal Editor 资产，但主要风险集中在五类：

1. **写入目标不够严格**：graph fallback、Node_i、title 匹配等会导致误写。
2. **协议校验不够硬**：缺字段、错类型、scope 不一致会造成 Agent 误判。
3. **事务与回滚不统一**：UMG、DataTable、UObject 等资产写入存在半成功风险。
4. **Bridge 安全边界不足**：localhost Bridge 缺少 token，高危命令需要默认收敛。
5. **复杂项目适配不足**：大资产分页、搜索过滤、active context、自动化测试还需要补齐。

建议先完成 P0，再处理 P1。P0/P1 修复完成前，不建议继续扩展更多高危写工具；否则工具数量增加会放大误写和误判问题。
