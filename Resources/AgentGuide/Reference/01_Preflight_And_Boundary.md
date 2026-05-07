# 01 - Preflight And Boundary

## 1. Task Type Decision

Agent 先判断任务是否需要 Unreal Editor:

```text
UE asset read or write -> BlueprintHelper MCP TaskSpec-first flow
Source/config/docs edit -> normal repository tools
Mixed task -> split first; code edits do not use BlueprintHelper MCP
```

## 2. BlueprintHelper MCP Scope

适用:

- 读取 Blueprint、UMG、DataAsset、DataTable 等 UE 资产上下文。
- 通过 TaskSpec 修改 Blueprint 图表、变量、组件、Class Settings、UMG、DataTable 行和对象属性。
- 通过 Task Runtime 执行预览、写入、验证和结果查询。

不适用:

- 全仓库代码搜索。
- C++、TypeScript、Python、配置、脚本和普通文档编辑。
- 直接修改 `.uproject`、`.uplugin`、`.Build.cs`、`.Target.cs`。
- 生成 AGENTS.md、memory 或普通项目说明。

## 3. Required Preflight

资产写入前必须确认:

1. 目标项目和 Editor/Bridge 状态可用。
2. 目标资产路径明确，例如 `/Game/Blueprints/BP_Player`。
3. 图表、函数、控件、行名或 block 锚点等目标上下文明确。
4. 修改范围明确，尤其是是否允许修改用户节点、接入已有执行流、创建资产。
5. 写入前已执行 preview，且 preview 未 blocked。

## 4. Default Loop

```text
understand request
-> check runtime profile
-> read compact context
-> build TaskSpec
-> preview
-> execute only after preview passes
-> read task result when needed
-> report concise summary
```

## 5. Frozen Tool Boundary

已注册但冻结的兼容、测试和专家工具不在 AgentGuide 中列为调用入口。普通 Agent 遇到 TaskSpec 无法表达的需求时，应停止报告缺口，而不是改用冻结入口。
