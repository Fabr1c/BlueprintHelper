# Worker E Fixtures And Validation Execution Plan

## 目标

新增最小 fixture 与人工验证说明，用于验证 LogicProcessor 输出稳定，不扩大到完整自动化测试框架。

## 文件边界

新增：

```text
Resources/TestFixtures/LogicProcessor/simple_beginplay_call.raw.json
Resources/TestFixtures/LogicProcessor/simple_beginplay_call.logic.json
Resources/TestFixtures/LogicProcessor/simple_beginplay_call.logic.md
Resources/TestFixtures/LogicProcessor/branch_flow.raw.json
Resources/TestFixtures/LogicProcessor/branch_flow.logic.json
Resources/TestFixtures/LogicProcessor/branch_flow.logic.md
Resources/TestFixtures/LogicProcessor/compat_old_links.raw.json
Resources/TestFixtures/LogicProcessor/compat_old_links.logic.json
Resources/TestFixtures/LogicProcessor/compat_old_links.logic.md
```

修改：

```text
Resources/v0.3.0/Module_BlueprintHelper_LogicOptimization_TestPlan_20260428.md
```

移除：

```text
无
```

越界规则：

- 不新增 C++ Automation Test。
- 不新增 Node 测试 runner。
- 不修改源码。
- 如果需要自动化测试文件，先提交请求变更文档。

## Fixture 范围

第一版只覆盖三类：

```text
simple_beginplay_call
branch_flow
compat_old_links
```

暂不创建 10 个完整 fixture。索引文档中的 10 个用例保留为后续扩展目标。

## 实现步骤

- [ ] 新增 `Resources/TestFixtures/LogicProcessor/`。
- [ ] 新增 `simple_beginplay_call.raw.json`，包含 `ReceiveBeginPlay -> PrintString`。
- [ ] 新增对应 `logic.json`，检查 event、call、exec link、stats。
- [ ] 新增对应 `logic.md`，不包含坐标。
- [ ] 新增 `branch_flow.raw.json`，包含 `ReceiveTick -> Branch -> True/False`。
- [ ] 新增对应 `logic.json` 和 `logic.md`。
- [ ] 新增 `compat_old_links.raw.json`，links 不含 `kind`。
- [ ] 新增对应输出，要求 `confidence=inferred`。
- [ ] 更新测试计划，说明第一批 fixture 是最小落地集，10 用例是扩展验收集。

## 最小 raw JSON 约束

raw fixture 使用现有 schema：

```json
{
  "version": "2.2",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "nodes": [],
  "links": []
}
```

节点字段只保留 LogicProcessor 必需字段：

```text
id
type
name 或 title
inputs
```

不引入导入器不支持的新必需字段。

## 验收

通过标准：

- fixture 文件均为合法 JSON 或 Markdown。
- `logic.json` 的 `schema` 为 `BlueprintHelper.LogicGraph`。
- `logic.md` 不复述完整 raw JSON。
- `compat_old_links` 不含 `kind`，期望输出仍能识别基础 exec link。

