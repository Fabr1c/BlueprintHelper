# BlueprintHelper ReadContext Capability Discovery 架构记录 2026-05-19

## 目标

`read_context.view.format=schema` 原本被实现成 `read_context` 下的特殊模式，但它并不读取资产，也不需要 UE Bridge。该语义与普通 `ReadSpec` 资产读取不同，容易让 Agent 误以为它是某个资产的 9 类读取结果。

本轮调整将它拆成独立 Agent-facing CLI 工具：

```text
blueprinthelper_read_context_capabilities
```

该工具只返回 ReadContext 能力矩阵，类似 `blueprinthelper_read_function_chain_context` 作为独立读侧工具存在；它不作为 `read_context` 的 `format` 选项。

## 返回结构

返回 payload schema：

```text
ReadContextCapabilities.v1
```

payload 使用全量集合 + 差集：

- `asset_types`: 当前 ReadContext 目标资产/对象类型全集。
- `formats`: 当前 ReadContext 读格式全集。
- `read_type_ids`: 当前 ReadContext 读类型全集。
- `read_types[]`: 按 read type 给出非能力差集，只列 `unsupported_asset_types` 和 `unsupported_formats`。

这样避免每个 read type 重复输出完整 `target_types` / `formats`，也避免把 Bridge command、payload schema 等实现细节暴露为普通 Agent 必读字段。

## 调度语义

`blueprinthelper_read_context_capabilities` 是 task-core 本地 discovery 工具：

- 不读取 `.uasset`。
- 不调用 UE Bridge。
- 不进入 UE/GameThread。
- 可以由多个普通 Agent 并行调用，不给编辑器线程增加压力。

真正读取资产内容的 `blueprinthelper_read_context` 仍通过 ReadSpec 路由，并在需要 UObject / Blueprint / Graph / AssetRegistry 状态时进入 UE Bridge 调度。

## 兼容处理

按照当前架构规则，不为旧 Agent 保留 `view.format=schema` 兼容路径。`BlueprintHelper.ReadSpec.v1.view.format` 现在只接受：

```text
logic_md, logic_json, summary
```

需要能力矩阵时调用 `blueprinthelper_read_context_capabilities`。

## 当前验证范围

- task-core registry 包含新工具。
- 新工具执行时不调用 Bridge。
- 新 payload 使用 `ReadContextCapabilities.v1`，并返回 `asset_types` / `formats` / `read_type_ids` / negative diff `read_types[]`。
- `read_context.view.format=schema` 被 schema 拒绝。
- CLI / AgentGuide / templates 文档已更新为新工具入口。

