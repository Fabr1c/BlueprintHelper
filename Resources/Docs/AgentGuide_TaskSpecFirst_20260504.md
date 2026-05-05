# BlueprintHelper Agent Guide — TaskSpec-first / Setup-aware Edition

日期：2026-05-04  
适用范围：BlueprintHelper v0.4 / v0.5 之后的 Agent→MCP→Python/MCP Task Compiler→UE Task Runtime 架构  
目标读者：Claude Code / Codex / ChatGPT Agent / 其他 MCP Agent  
文档性质：Agent-facing 操作规约；不是用户安装手册、MCP API Reference 或 UE 插件实现文档。

---

## 0. 本版收敛结论

BlueprintHelper 的 Agent 引导文档继续保留，但主线必须从旧的“Agent 直接选择大量底层 MCP 工具”收敛为：

```text
Agent
→ MCP Agent-facing Task Tools
→ Python / MCP Task Compiler
→ UE Plugin Task Runtime
→ Existing UE Capability Clusters
→ Unreal Editor
```

新的 Agent Guide 只回答一个问题：

```text
Agent 如何在 TaskSpec-first 架构下安全、低歧义、可审计地使用 BlueprintHelper 完成 UE 资产任务。
```

不再让普通 Agent 直接把一个复杂任务拆成几十个底层工具调用。底层 Asset Factory、Component、Class Settings、Graph Write、Validation、Cleanup 等工具簇继续存在，但默认定位为：

```text
1. UE Task Runtime 内部 capability。
2. Python / MCP Task Compiler 生成 TaskPlan step 时的能力边界。
3. debug / expert 工具。
4. 自动化测试入口。
5. 失败定位入口。
```

普通 Agent 的默认工作流固定为：

```text
get_runtime_profile
→ read_context / read_reference_context as needed
→ build TaskSpec
→ preview_task
→ repair TaskSpec / stop_and_report
→ execute_task
→ get_task_result if needed
→ report task summary
```

---

## 1. 文档边界

### 1.1 Agent Guide 负责

```text
1. 识别当前是否应使用 BlueprintHelper MCP。
2. 规定 Agent 默认任务循环。
3. 规定 TaskContextPack / TaskSpec / preview_task / execute_task 的使用方式。
4. 规定 runtime_profile、Safety Profile、missing capability 的消费规则。
5. 规定 diagnostics 与 runtime_profile 的区别。
6. 规定底层 capability 的边界，避免 Agent 混用工具职责。
7. 规定 LogicMD / LogicJson / RawJson / resource_ref 的读取策略。
8. 规定 Graph Write、Asset、Component、Class Settings、Enhanced Input 的 stop 条件。
9. 规定最终报告格式。
```

### 1.2 Agent Guide 不负责

```text
1. 用户安装步骤。
2. MCP Server 安装与启动命令的完整说明。
3. UE 插件 C++ 内部实现。
4. 完整 MCP tool schema。
5. Transaction Journal / Review Store 的完整存储格式。
6. Setup Wizard 的用户问卷正文。
7. C++ / TypeScript / Python 源码编辑规则。
```

这些内容分别属于用户 Setup 文档、开发者设计文档、MCP API Reference 或普通代码工程文档。

---

## 2. BlueprintHelper 四层职责与新增混合任务链路

BlueprintHelper 仍按四部分理解：

```text
BlueprintHelper = UE 插件侧 + MCP 服务侧 + Agent Skill 侧 + 用户引导侧
```

| 层 | 主要职责 | Agent Guide 关系 |
|---|---|---|
| UE Plugin Layer | 在 Unreal Editor 内读取、修改、保存资产；执行 TaskPlan；生成 transaction / Review / rollback | Agent 不直接控制内部实现 |
| MCP Server Layer | 暴露 Agent-facing Task Tools；管理 Bridge、schema、资源、错误归一化 | Agent 通过 MCP 调用任务级工具 |
| Agent Skill Layer | 规定 Agent 如何生成 TaskSpec、如何处理 preview 错误、何时 stop | 本文属于这一层 |
| User Guidance & Setup Layer | 安装、配置、Setup Profile、用户偏好、项目 Marker、Troubleshooting | 本文消费其结果，不替代它 |

新增混合链路中的两个核心模块：

```text
Python / MCP Task Compiler：
- TaskSpec schema validation
- semantic validation
- TaskContextPack 打包
- resource disambiguation
- suggested_patch 生成
- TaskSpec → TaskPlan
- Bridge / UE error normalization

UE Plugin Task Runtime：
- 接收 TaskPlan
- 重新读取 UE 当前状态
- TOCTOU / preflight 检查
- 执行 TaskPlan step
- 生成 task_run_id
- 每个真实写操作生成 transaction_id
- 写 TaskRunJournal / Transaction Journal / Review
- rollback / compile / diagnostics / save
```

Agent 不应把 UE Task Runtime 理解成 Agent 大脑。UE 侧负责执行，不负责自然语言意图修正或 Agent-facing suggested_patch。

---

## 3. Agent-facing 默认工具

普通 Agent 默认只使用少量任务级工具：

```text
blueprinthelper_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_open_editor
blueprinthelper_close_editor
```

### 3.1 工具职责

| 工具 | 是否写资产 | 作用 |
|---|---:|---|
| `blueprinthelper_get_runtime_profile` | 否 | 获取 Bridge、config、write_permission、Safety Profile、unavailable capability |
| `blueprinthelper_diagnostics` | 否 | 静态或运行时诊断，定位安装、配置、Bridge、runtime 问题 |
| `blueprinthelper_read_agent_guide` | 否 | 返回 AgentGuide 入口索引，供 Agent 查询当前 TaskSpec / ReadSpec 文档 |
| `blueprinthelper_read_context` | 否 | 按 ReadSpec 返回 ReadContextPack / LogicMD / LogicJson 等只读上下文 |
| `blueprinthelper_read_reference_context` | 否 | 返回引用上下文，用于用户问引用、preview blocked 解释或高风险写入前影响面分析 |
| `blueprinthelper_preview_task` | 否 | 校验 TaskSpec、生成 TaskPlan 摘要、执行 policy / dry_run / preflight |
| `blueprinthelper_execute_task` | 是 | 执行通过 preview 的 TaskPlan |
| `blueprinthelper_get_task_result` | 否 | 查询 task_run_id 的任务摘要、验证状态、必要错误摘要 |
| `blueprinthelper_open_editor` | 否 | 启动或打开 UE Editor |
| `blueprinthelper_close_editor` | 否 | 关闭 UE Editor |

底层工具名以 MCP `tools/list` 为准，但普通 Agent 不应优先直调底层工具。

---

## 4. 任务分类与是否使用 MCP

Agent 收到用户请求后先分类：

```text
A. UE 编辑器资产任务：使用 BlueprintHelper MCP。
B. 源码 / 配置 / 文档任务：使用普通文件或代码工具。
C. 混合任务：拆分；源码部分不用 BlueprintHelper MCP，UE 资产部分使用 BlueprintHelper MCP。
```

### 4.1 适合 BlueprintHelper MCP

```text
1. 查找、打开、读取、保存 UE 资产。
2. 读取 Blueprint LogicMD / LogicJson / RawJson。
3. 创建或修改 Blueprint 资产、组件、变量、类设置、图表逻辑。
4. 修改 UMG WidgetTree。
5. 修改 DataAsset / UObject 属性。
6. 修改 DataTable 行。
7. 编译蓝图、运行只读诊断、PIE 验证。
8. 执行 TaskSpec 表达的 UE 编辑器任务。
```

### 4.2 不适合 BlueprintHelper MCP

```text
1. C++ / TypeScript / Python / Shell 文件编辑。
2. .uproject / .uplugin / Build.cs / Target.cs / Config 直接编辑。
3. 全仓库代码搜索。
4. 生成 README、AGENTS.md、CLAUDE.md 等普通文本文件。
5. 不需要 Unreal Editor 参与的任务。
```

如果用户要求“改 C++ 后再改蓝图”，Agent 必须拆成两个阶段，且 C++ 修改不用 BlueprintHelper MCP。

---

## 5. runtime_profile 是写入前事实来源

每个可能进入写阶段的任务，在正式写入前必须调用一次：

```text
blueprinthelper_get_runtime_profile
```

Agent 需要读取：

```text
bridge_status
config_status
write_permission
risk_command
active_profile.safety_profile
active_profile.missing_capability_policy
tool_capabilities
```

### 5.1 不允许从单次工具结果推断安全档位

规则：

```text
1. safety_profile 只来自 runtime_profile.active_profile。
2. 普通工具成功结果不默认返回 safety。
3. Agent 不得期待普通工具结果带 transaction / review / safety。
4. Agent 不得通过工具参数临时覆盖 SetupProfile。
```

禁止参数或字段：

```text
safety_profile
profile
temporary_profile
per_call_profile
one-shot Expert
permission_override
force_write
no_review
no_journal
force_user_nodes
override_ownership_policy
```

若工具参数与 SetupProfile 冲突，应视为 `ProfilePolicyViolation`，停止当前写入。

### 5.2 tool_capabilities 是 unavailable_only

runtime_profile 中的能力信息采用负向稀疏列表：

```json
{
  "tool_capabilities": {
    "mode": "unavailable_only",
    "unavailable": [
      {
        "cluster": "graph_write",
        "capability": "merge",
        "status": "unavailable",
        "reason": "not_implemented"
      }
    ]
  }
}
```

Agent 必须理解：

```text
未出现在 unavailable 中，不等于 runtime_profile 已完整确认 schema。
runtime_profile 不是工具索引。
runtime_profile 不是 MCP API Reference。
具体参数仍以当前 MCP tool schema / TaskSpec schema 为准。
```

若当前任务必须依赖某 unavailable capability，且无安全替代路径，按 `missing_capability_policy` 执行，默认 `stop_and_report`。

---

## 6. diagnostics 只用于定位问题

Diagnostics 与 runtime_profile 不可互相替代：

```text
runtime_profile：任务前事实来源。
diagnostics：只读诊断报告，用于定位安装、配置、Bridge、runtime 链路问题。
```

Diagnostics 返回 ToolResultBase 外壳，实际报告位于：

```text
data.markdown
```

Markdown 必须包含：

```md
## Blocking
...

## Warning
...
```

`## Info` 可选。

### 6.1 ok/status 解释

```text
ok=true, status=completed：diagnostics 工具执行成功，即使 data.markdown 里有 Blocking。
ok=false, status=failed：diagnostics 工具自身失败。
```

Markdown 中的 Blocking 是诊断发现的环境阻断项，不是 MCP 工具调用失败。

Agent 不得这样做：

```text
runtime_profile.write_permission.disabled
→ diagnostics 没有工具失败
→ 继续写入
```

正确做法：runtime_profile 阻断写入时，diagnostics 只能用于解释原因。

---

## 7. ReadContextPack / 任务上下文

`blueprinthelper_read_context` 是 Agent 生成 TaskSpec 前的默认只读上下文入口，用于减少猜参数和反复 preview 错误。旧 `blueprinthelper_read_task_context` 定位暂不清晰，标记为 deprecated，不作为新主线默认入口。

调用：

```text
blueprinthelper_read_context
```

Agent 应请求与任务相关的上下文，例如：

```text
1. 目标资产是否存在、资产类型、父类。
2. Blueprint 组件摘要、图表列表、变量摘要、接口摘要。
3. LogicMD / LogicJson 片段或目标图表摘要。
4. 资源候选，例如 mesh、interface、InputAction、DataTable。
5. 当前 runtime 摘要和 unavailable capability 摘要。
6. 必要的 context_id 或 large_payload_ref。
```

如果 preview 返回 `context_required` 或 `context_stale`，Agent 应重新调用 `read_context` 或相关只读工具，不能继续沿用旧上下文。

---

## 8. TaskSpec 生成规则

TaskSpec 是 Agent-facing 的语义任务规格，不是自然语言，也不是 K2 节点图。

TaskSpec 应描述：

```text
目标资产
任务类型
feature_name
允许修改范围
资产/资源策略
组件需求
变量需求
类设置需求
行为逻辑
输入/接口/已有执行流集成方式
验证策略
失败策略
```

### 8.1 TaskSpec 最小骨架

固定字段以 `Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md` 为准。当前 compiler 合同覆盖 GraphWrite Append/Replace/Patch/Merge、Blueprint Variables、P1 capability clusters，以及 `create_blueprint_feature` composite + `integration.interface` 首片。复合任务由 compiler 分解为现有 capability steps，Agent 不填写 TaskPlan。

当前 smoke-verified execute 闭环更窄：GraphWrite 只确认 `append_new_owned_graph + 新图名` 可执行，Variables 只确认 `edit_blueprint_variables` 可执行。普通 Agent 默认只应使用 `append_new_owned_graph` 写新 BlueprintHelper-owned 图；Replace/Patch/Merge、Component、Composite 等路径必须 preview-first，preview blocked 时停止并报告，不回退到底层原子写工具。

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_20260504_0001",
  "task_type": "edit_blueprint_graph",
  "feature_name": "PhysicsDoor",
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EG_PhysicsDoor",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "InitializePhysicsDoor",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "call_function",
              "name": "PrintString",
              "args": {
                "InString": {
                  "kind": "literal",
                  "value_type": "string",
                  "value": "Physics door initialized"
                }
              }
            }
          ]
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report"
  },
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

### 8.2 TaskSpec 禁止写法

```text
1. 只写自然语言 body。
2. 直接传 RawJson / K2 nodes+links 作为默认协议。
3. 混入 per-call safety override。
4. 未说明 allow_modify_user_nodes 却要求改已有节点。
5. 未说明 allow_merge_existing_execution_flow 却要求接入 BeginPlay / Tick / InputAction。
6. 把 InputAction 创建当作 IMC 映射完成。
7. 把 add_implemented_interface 当作接口函数 body 已实现；需要接口实现时使用 `create_blueprint_feature.integration.interface`。
8. 在 add_component 里表达 mesh / collision / physics 等属性设置。
```

---

## 9. preview_task 规则

`blueprinthelper_preview_task` 不写 UE 资产。

preview 应完成：

```text
1. TaskSpec schema validation。
2. TaskSpec semantic validation。
3. context_id / context_stale 检查。
4. runtime_profile / write_permission / safety_profile 检查。
5. missing capability 检查。
6. TaskSpec → TaskPlan。
7. TaskPlan preflight / dry_run。
```

### 9.1 preview 返回语义

| 返回 | 语义 | Agent 行为 |
|---|---|---|
| `status=preview_passed` | 可执行 | 可以 execute_task |
| `status=context_required` | 上下文不足或过期 | 重新 read_context / read_reference_context |
| `status=preview_blocked` | TaskSpec 合法但当前不可执行 | stop_and_report 或修改 TaskSpec |
| `ok=false,status=failed` | TaskSpec schema/semantic 或 preview 工具自身失败 | 按 error.issues / suggested_patch 修正 |

preview blocked 不是写入失败，因为它不写资产。

### 9.2 suggested_patch

TaskSpec schema / semantic 错误应由 Python / MCP Task Compiler 返回 Agent-facing `suggested_patches`。Agent 可自动采用安全补丁，若补丁改变用户意图或安全范围，必须向用户报告或 stop。

---

## 10. execute_task 规则

`blueprinthelper_execute_task` 只执行通过 preview 的 TaskSpec / TaskPlan，或内部先执行 preview。

execute 由 UE Task Runtime 完成：

```text
1. 重新读取当前 UE 状态。
2. 执行 TOCTOU / preflight。
3. 执行 TaskPlan step。
4. 生成 task_run_id。
5. 每个真实写操作生成 transaction_id。
6. 写 TaskRunJournal / Transaction Journal / Review。
7. 失败时按策略 rollback。
8. 执行 compile / diagnostics / save。
```

### 10.1 成功返回默认只读任务摘要

Agent-facing 成功返回应聚焦：

```text
task_run_id
feature_name
target_assets
applied_steps
created_assets
modified_assets
validation summary with compiled / saved status when runtime reports it
```

普通用户报告不展开：

```text
child transaction_id 全量列表
完整 TaskPlan steps
完整 Journal 路径
完整 diff
底层 Bridge 原始错误 JSON
```

调试、rollback、Review 定位场景除外。

---

## 11. ID 层级

```text
task_run_id = 一次 TaskSpec / TaskPlan 执行。
transaction_id = 一次真实 UE 写操作。
block_id = Graph Write 创建或接管的 BlueprintHelper-owned 蓝图逻辑块。
```

关系：

```text
一个 task_run_id 可以包含多个 transaction_id。
一个 transaction_id 可以包含多个 block_id。
block_id 只用于 Graph Write owned 逻辑块。
```

Agent 不生成 `transaction_id` 或 `block_id`。这些 ID 由 UE 侧生成。

---

## 12. Logic 读取策略

### 12.1 默认顺序

```text
Need understand?          → logic_md
Need structured reasoning? → logic_json
Need exact pins / import?  → raw_json_structured / resource_ref
Need large payload?        → resource_ref
```

### 12.2 LogicMD

适合：

```text
用户问“这个蓝图做什么”
Agent 生成 TaskSpec 前理解蓝图意图
快速审阅入口、事件、owned/user 区域
```

多入口 scope，如 target_graph / blueprint / multi_target，应按 group 阅读。`grouped=true` 表示 Markdown 已分组，不代表不同 group 有执行连接。

### 12.3 LogicJson

适合：

```text
Patch / Merge / Replace / Cleanup 前结构化分析
定位执行流、数据流、node_ref、link_ref
判断用户区域与 owned block
```

规则：

```text
1. target_graph / blueprint / multi_target 使用 logic.groups[]。
2. node_ref / link_ref 只在当前 group 内有效。
3. links 存在 source node 的 node.links 内，表示 outgoing links。
4. LogicJson importable=false，不能作为导入格式。
```

当前已知问题：`target_type=custom_event` 的 LogicJson 查找只搜索 EventGraph，忽略自定义图。读取自定义图里的 Custom Event 时，先按 graph target 读取整张自定义图，或用 LogicMD 做人工核对，直到该 bug 修复。

### 12.4 RawJson / resource_ref

仅用于：

```text
导入 / 回放
完整保真备份
Pin/GUID 级调试
Schema 回归测试
大 payload 延迟读取
```

用户只要求理解逻辑时，不默认导出 RawJson。

---

## 13. Capability 边界速查

### 13.1 Asset Factory

只负责创建 `.uasset` 资产。

不得把 Asset Factory 用于：

```text
1. 添加 BPI 到某 Blueprint 的 Implemented Interfaces。
2. 创建接口函数实现图。
3. 写接口函数 body。
4. 修改 IMC 映射。
5. 修改 C++ 源码或项目配置。
```

创建 BPI 后不代表任何 Blueprint 已实现接口。

### 13.2 Blueprint Component

`add_component` 只负责：

```text
1. 创建组件。
2. 建立 attachment。
```

组件属性必须通过 `set_component_property / set_component_properties` 对应的 TaskPlan step 表达。

以下不得塞进 add_component：

```text
Transform / Mobility / Collision / Physics / StaticMesh / Material / PhysicsConstraint 参数
```

### 13.3 Blueprint Class Settings

只负责类设置与声明层：

```text
read_class_settings
add/remove implemented interface
set class default property
```

不负责：

```text
创建 BPI
创建接口函数实现图
写接口函数 body
接入 EventGraph
修改 Parent Class / Reparent
```

如果用户任务要求修改 Parent Class，Agent 应 `stop_and_report`，说明当前第一版 Class Settings 不支持。

### 13.4 Enhanced Input Boundary

默认不自动创建或修改 InputAction / InputMappingContext。

允许：

```text
1. 引用用户明确提供的 IA asset_path。
2. 搜索并确认唯一 IA 候选。
3. 在蓝图中创建 / 连接 IA 事件入口，前提是 capability 可用。
```

不得：

```text
1. 把 IA 创建当作 IMC 映射完成。
2. 多个 IA 候选时猜一个。
3. 用 Append 接入已有 IA 事件执行流。
```

接入已有执行流必须走 Merge，并且 dry_run。

---

## 14. Graph Write capability 边界

Graph Write 仍保留四种语义，但普通 Agent 不直接把它们作为主入口。Agent 通过 TaskSpec 表达意图，由 Task Compiler 生成 TaskPlan step。

### 14.1 AppendBlueprintGraph

用途：

```text
向新图表或空图表追加独立 Custom Event 逻辑块。
```

不负责：

```text
写非空图表
接入已有执行流
替换已有实现
Patch 精确修改
创建 Function
创建 engine event / input action event / BeginPlay / Tick / Overlap
创建变量
自动创建缺失 callee
自动改名
自动 cleanup
```

输入侧不传 block_id / block_ref。block_id 由 UE 生成。

成功结果可包含压缩 handle：

```text
data.append_result.graph.graph_id
data.append_result.block_refs[] 或 blocks[].block_ref
data.write_ref.transaction_id
validation
```

### 14.2 ReplaceBlueprintGraph

用途：

```text
替换一个明确目标的完整实现。
```

目标可以是：

```text
block / function / custom_event / event / graph
```

Replace 不允许模糊匹配。目标不明确时必须停止。

替换 owned block 时保留原 block_id。替换用户手写目标默认不接管 ownership。

### 14.3 PatchBlueprintGraph

用途：

```text
精确修改已有 node / pin / link / pin default / node comment / metadata。
```

Patch 第一版要求完整 LogicJson path：

```text
node_path / pin_path / link_path
```

不得只靠显示名、Pin 名、node_ref、link_ref 或自然语言描述直接 patch。

### 14.4 MergeBlueprintGraph

用途：

```text
把新逻辑接入已有执行流。
```

必须明确：

```text
target_graph
target node/path
target pin/path
insert_strategy
```

`append_after` 如果目标 Exec Pin 已有后继连接，应直接 error；不能自动改成 `insert_between` 或 `branch_fork`。

---

## 15. dry_run / preflight / Review

```text
dry_run = 写入前非写入预演。
preflight = 正式写入前的内部检查。
Review = 写入后的用户审查和 rollback 入口。
```

规则：

```text
1. dry_run 不写资产。
2. preview_task 可触发 dry_run / preflight。
3. execute_task 正式写入前仍必须 preflight。
4. dry_run passed 不允许跳过正式 preflight。
5. Review 不能替代 dry_run。
6. dry_run 不能替代 Review。
```

dry_run 数据只在：

```text
status=dry_run
data.dry_run
```

普通成功工具结果不应默认返回 dry_run 细节。

---

## 16. Safety Profile 行为

Agent 只能从 runtime_profile 读取当前档位。

| 档位 | Agent 行为 |
|---|---|
| ReadOnly | 只读、搜索、导出、diagnostics、dry_run；不得 execute_task 写资产 |
| Conservative | 默认安全写入档；高风险必须 dry_run；不自动 save；不自动改用户节点 |
| Standard | 可对 owned 内容更主动；仍不默认自动 save |
| AutoRepair | 只自动修复 BlueprintHelper-owned 内容 |
| Expert | 允许低层高风险操作，但仍必须 Journal / Review；不是免审模式 |

Conservative 下 `warning / info` 不阻断正式写入；`error / conflict / blocker` 阻断。

---

## 17. 错误处理

### 17.1 Task Error Layer

Agent 默认消费 Task Error，而不是底层 Bridge Error。

Task Error 应回答：

```text
TaskSpec 哪个字段错？
为什么错？
允许什么值？
是否可自动 patch？
Agent 应修 TaskSpec、问用户，还是 stop_and_report？
```

典型 code：

```text
taskspec_schema_error
taskspec_semantic_error
task_policy_error
task_capability_error
task_preview_blocked
task_execution_failed
context_required
context_stale
```

### 17.2 Bridge / UE Operation Error Layer

底层错误是事实来源，但由 Python / MCP Error Normalizer 转译为 Task Error。Agent 只有在 debug / expert / failure report 场景才需要查看底层 operation error 摘要。

### 17.3 rollback blocked / failed

如果 execute 中某步失败且 rollback blocked / failed：

```text
1. Agent 必须 stop_and_report。
2. 不得继续 compile / save / patch。
3. 报告目标资产、失败阶段、是否 modified=true、需要用户检查 Review / rollback 状态。
```

---

## 18. 最终报告规则

普通用户报告应包含：

```text
任务状态：completed / preview_blocked / failed / partially_applied
目标资产
主要变更摘要
验证结果：compile / diagnostics / save
未完成项或阻断原因
需要用户动作：例如配置 IA / IMC、通过 Setup 提升 Profile、打开 UE Editor
```

默认不报告：

```text
完整 TaskPlan
完整 child transaction_id 列表
Journal 路径
Review Store 路径
完整 diff
schema 细节
底层 Bridge 原始 JSON
```

当用户明确要求审计、rollback 或 debug 时，可报告必要的 `task_run_id`、相关 `transaction_id` 或 block handle。

---

## 19. 迁移规则：旧 Agent Guide → 新 Agent Guide

旧规则：

```text
Agent 选择 Asset / Component / Class Settings / Graph Write 工具并直接组合调用。
```

新规则：

```text
Agent 生成 TaskSpec，preview_task 产出 TaskPlan 摘要，execute_task 由 UE Task Runtime 执行。
```

迁移时保留的知识：

```text
1. 各工具簇职责边界。
2. LogicMD / LogicJson / RawJson 读取策略。
3. runtime_profile / diagnostics / safety 规则。
4. transaction_id / block_id / Review / rollback 规则。
```

迁移时删除或降级的知识：

```text
1. 普通 Agent 直接调用底层工具序列。
2. 在 Agent 报告中展开大量 ToolResultBase 字段。
3. 让 Agent 手动管理 block_id / transaction_id。
4. 让 Agent 用 per-call 参数覆盖安全档位。
```

---

## 20. Agent 侧最小 SKILL 摘要

可同步到 `Resources/Skills/BlueprintHelper/SKILL.md`：

```text
BlueprintHelper 是 UE5.3+ 的 Agent 编辑辅助系统。
普通 Agent 默认使用 TaskSpec-first 工作流：get_runtime_profile → read_context / read_reference_context as needed → build TaskSpec → preview_task → execute_task → report summary。
不要直接把复杂任务拆成大量底层 MCP 调用。
底层工具簇是 TaskPlan capability / debug / expert / 测试入口。
runtime_profile.active_profile 是 safety_profile 唯一来源。
diagnostics 只定位问题，不替代 runtime_profile。
LogicMD 用于理解，LogicJson 用于结构化分析，RawJson/resource_ref 用于保真、导入、Pin/GUID 级调试。
Asset Factory 只创建资产；Component add 只创建和 attachment；Class Settings 不写图表逻辑；Enhanced Input 默认不编辑 IA/IMC；Graph Write 合同分 Append/Replace/Patch/Merge，当前已验证执行只默认使用 Append 新图。
失败、preview_blocked、missing capability、rollback blocked 时 stop_and_report。
```
