# 04 Enhanced Input / Input Reference Boundary 设计文档（已同步确认 Diff）

日期：2026-05-03  
工具簇：Enhanced Input / Input Reference Boundary  
状态：同步确认 Diff 后的修正版  
同步范围：与 Asset Factory / Graph Write / runtime profile / stop_and_report 规则对齐。

---

## 0. 本次同步结论

本文件维持旧版核心边界，但同步以下规则：

```text
1. 当前阶段不将 Enhanced Input 编辑能力作为 BlueprintHelper 默认核心能力。
2. Agent 不应默认自动创建或修改 InputAction / InputMappingContext。
3. 如果 Asset Factory 当前版本支持创建 InputAction，也必须以用户明确目标和 runtime profile 能力为前提。
4. IA 事件入口属于 Graph Write / 事件入口能力，不修改 IA / IMC 资产。
5. 接入已有执行流必须使用 MergeBlueprintGraph，并 dry_run。
6. Enhanced Input 能力缺失时，Agent 应按 runtime_profile unavailable_only 和 missing_capability_policy 判断 stop_and_report。
```

---

## 1. 定位

当前阶段不将 Enhanced Input 编辑能力作为 BlueprintHelper 默认核心能力。

IA / IMC 属于项目输入架构层，通常需要用户与 Agent 协作确认。

BlueprintHelper 不应默认让 Agent 自动创建或修改 InputAction / InputMappingContext。

---

## 2. 不提供的默认能力

当前阶段不提供或不优先提供 Enhanced Input 专用写工具：

```text
input_create_action
input_create_mapping_context
input_add_mapping
input_remove_mapping
input_update_mapping
```

也不提供 Enhanced Input 专用解析工具：

```text
input_list_actions
input_get_action
input_list_mapping_contexts
input_get_mapping_context
input_find_key_binding
```

如果未来 Asset Factory 支持创建 InputAction，也不等于 Enhanced Input 编辑能力已经完整可用。创建 IA 资产、编辑 IMC 映射、让运行时接收输入是三件不同的事。

---

## 3. 输入系统边界

插件不解析 IMC 内部按键映射。

插件不判断：

```text
F 键是否已绑定到 IA_Interact
某个 IMC 是否已加入 Player Controller / Character
某个 IA 是否在运行时可触发
```

输入绑定是否正确，属于用户协作配置范围。

如果找不到指定 IA 资产，Agent 应停止并报告需要用户创建 / 指定 InputAction，而不是自动创建。

例外：

```text
用户明确要求创建 IA
+
runtime profile 未报告 Asset Factory 对应能力不可用
+
当前 Safety Profile 允许创建资产
```

这种情况下可走 Asset Factory 创建 IA 资产，但仍不代表 IMC 映射已完成。

---

## 4. 允许的能力：引用已有 IA

Agent 可通过通用资产搜索 / 资产信息工具查找已有 InputAction / InputMappingContext 资产。

如果用户明确提供 IA `asset_path`，可直接引用该资产，但仍应校验：

```text
资产存在
资产类型正确
```

如果 IA 路径不是用户明确给出，Agent 必须通过资产搜索确认唯一匹配。

多个同名或相似 IA 资产存在时，Agent 必须停止并要求用户指定明确资产路径。

---

## 5. IA 事件入口

蓝图图表写入时，可以引用用户已有 IA 资产创建或连接对应 InputAction 事件节点。

这属于：

```text
Graph Write / 事件入口能力
```

不是 Enhanced Input 资产编辑能力。

创建 IA 事件入口不修改 IA / IMC 资产。

IA 事件入口应返回：

```text
entry_ref
entry_node_guid
```

IA 事件入口不使用 `block_id`。

IA 事件入口 ownership 与其他事件入口一致：

```text
entry metadata + Journal 双写
不写 BlueprintHelperBlockId
事件后方业务逻辑由 Graph Write 写入时才生成 block_id
```

---

## 6. 接入已有执行流

如果 IA 事件入口后已有执行流，接入新逻辑必须使用：

```text
MergeBlueprintGraph
```

并且必须 dry_run。

Agent 不得用 Append 替代 Merge 接入已有执行流。

如果 IA 事件入口不存在，可由 Graph Write / 事件入口能力创建；具体能力是否可用以 runtime profile 和 MCP schema 为准。

---

## 7. runtime_profile 与 stop_and_report

Enhanced Input 相关能力是否可用，不由文档静态假设决定。

Agent 应按：

```text
runtime_profile.tool_capabilities.mode = unavailable_only
```

理解能力缺失。

如果当前任务明确需要：

```text
edit_mapping_context
input_add_mapping
input_find_key_binding
```

且该能力在 runtime_profile unavailable 列表中，且无安全替代路径，Agent 应 stop_and_report。

如果用户目标可以改为“引用用户已配置好的 IA 并写蓝图事件逻辑”，则 Agent 可以只完成 Graph Write 部分，并明确说明输入资产配置不属于本次 Agent 独立完成范围。

---

## 8. 测试验收口径修正

旧验收：

```text
Agent 必须独立创建 IA_Interact 并编辑 IMC_Default。
```

新验收：

```text
Agent 应能识别需要输入绑定。
Agent 应报告需要用户配置 IA / IMC，或请求用户指定 IA。
用户完成输入资产配置后，Agent 再继续完成蓝图事件接入与交互逻辑。
```

如果用户明确要求并且能力可用，Agent 可以创建 IA 资产，但不能把“IA 已创建”报告成“按键映射已完成”。

---

## 9. Agent 禁止行为

Agent 不得：

```text
1. 默认自动创建 InputAction / InputMappingContext。
2. 默认自动编辑 IMC 按键映射。
3. 把 IA 事件入口创建误认为 IMC 已配置。
4. 在多个 IA 匹配时猜测使用其中一个。
5. 用 Append 接入已有 IA 事件执行流。
6. 绕过 runtime_profile 的 Enhanced Input 能力缺失。
7. 把用户手动配置 IMC 计入 Agent 独立完成能力。
```

---

## 10. 验收标准

```text
1. Agent 能区分 IA 资产、IMC 映射、蓝图 IA 事件入口。
2. Agent 不默认自动创建或修改输入资产。
3. Agent 引用 IA 前会确认资产存在且类型正确。
4. 多个匹配 IA 时停止并要求明确路径。
5. IA 事件入口不使用 block_id。
6. IA 事件后方业务逻辑由 Graph Write 创建 block_id。
7. 接入已有执行流必须用 MergeBlueprintGraph 并 dry_run。
8. Enhanced Input 能力不可用且任务必须依赖时，Agent stop_and_report。
```
---

# 2026-05-04 混合架构同步：工具簇暴露层级

## 同步结论

本文档中的工具簇边界不推翻，但 Agent-facing 暴露方式调整。

底层能力簇继续作为：

```text
1. UE Task Runtime step operation。
2. Python / MCP Task Compiler 的 capability 模型。
3. Debug / Expert / 测试入口。
```

普通 Agent 不应默认直接手动拼装本工具簇调用链。普通流程改为：

```text
read_task_context → preview_task → execute_task
```

## 边界仍然有效

本工具簇原有职责边界仍必须被 Task Compiler / Task Runtime 遵守。

例如：

```text
Asset Factory 只创建资产，不添加接口、不写接口函数 body。
Component add_component 只创建组件和 attachment，不设置属性。
Class Settings add_implemented_interface 只修改 Implemented Interfaces。
Enhanced Input 当前不默认自动编辑 IA / IMC。
```

也就是说，混合架构只改变“谁来调用工具”，不改变“工具能做什么”。

## Agent-facing 返回调整

普通 execute_task 成功结果默认不展开本工具簇的底层返回。

底层 transaction / review / safety 仍进入 UE Journal / Review，但普通任务成功摘要只报告：

```text
任务是否完成
修改了哪些资产
执行了多少步骤
是否 compile/save
异常或未完成项
```
