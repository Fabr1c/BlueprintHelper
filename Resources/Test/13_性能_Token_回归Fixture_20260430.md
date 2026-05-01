---
项目：BlueprintHelper
版本目标：v0.3.0 当前源码包
生成日期：2026-04-30
适用范围：UE5.3+ BlueprintHelper 插件、MCP Server、UE Bridge、Agent 工作流
---

# 性能、Token 与回归 Fixture 测试

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

- LogicProcessor fixtures。
- AgentImport fixtures。
- MCPRegression fixtures。
- MCP response modes：`summary_text`、`structured_json`、`resource_ref`、`legacy_text_json`。
- RawJson / LogicJson / LogicMD 的 Token 与响应大小对比。

## 测试数据集

| 目录 | 用途 |
|---|---|
| `Resources/TestFixtures/LogicProcessor/*.raw.json` | RawJson 输入基线 |
| `Resources/TestFixtures/LogicProcessor/*.logic.json` | LogicJson 期望输出 |
| `Resources/TestFixtures/LogicProcessor/*.logic.md` | LogicMD 期望输出 |
| `Resources/TestFixtures/AgentImport/*.agent_import.json` | AgentImportGraph 成功/失败样例 |
| `Resources/TestFixtures/MCPRegression/*.mcp.json` | MCP 返回结构和兼容性回归 |
| `Resources/v0.2.0/Test/*.json` | 旧版本 RawJson 全覆盖回归 |

## 测试用例

| ID | 功能 | 步骤 | 指标/期望 | 优先级 |
|---|---|---|---|---|
| PERF-001 | LogicMD 大小 | 对同一蓝图导出 RawJson 与 LogicMD | 记录 bytes/chars/token_estimate；LogicMD 明显小于 RawJson | P1 |
| PERF-002 | LogicJson 大小 | 对同一蓝图导出 RawJson 与 LogicJson | LogicJson 明显小于 RawJson；结构字段足够用于编辑计划 | P1 |
| PERF-003 | 10KB 蓝图读取 | 构造约 10KB RawJson 蓝图 | `blueprint_get_logic` 响应时间和大小满足 Agent 对话使用 | P1 |
| PERF-004 | 50KB 蓝图读取 | 构造约 50KB RawJson 蓝图 | Logic 输出不退化为完整 RawJson；无超时 | P2 |
| PERF-005 | Orphan 节点开关 | 含大量孤立节点图表，分别 include_orphans true/false | false 时显著减少输出；true 时保留诊断完整性 | P2 |
| PERF-006 | Data dependencies 开关 | 含变量/资产/DataTable 依赖图表 | include_data_dependencies 只增加必要依赖文本 | P2 |
| PERF-007 | resource_ref 模式 | 对大 RawJson 读取使用 resource_ref | MCP content 只返回 uri/摘要；大 payload 不直接刷入上下文 | P1 |
| PERF-008 | legacy_text_json 兼容 | 调用旧模式返回 | 老客户端仍可解析；新字段不破坏兼容 | P2 |
| REG-001 | LogicProcessor simple fixture | 用 `simple_beginplay_call.raw.json` 生成 LogicJson/MD | 与 fixture 语义一致；允许非语义字段差异 | P0 |
| REG-002 | LogicProcessor branch fixture | 用 `branch_flow.raw.json` 生成 LogicJson/MD | Branch true/false 关系一致 | P0 |
| REG-003 | 旧 links 兼容 fixture | 用 `compat_old_links.raw.json` | link 语义可读；Pin 类型缺失有兼容策略 | P1 |
| REG-004 | AgentImport simple fixture | 导入 `simple_beginplay_print.agent_import.json` | 成功创建并可编译 | P0 |
| REG-005 | AgentImport branch fixture | 导入 `branch_flow.agent_import.json` | 成功创建分支图 | P0 |
| REG-006 | AgentImport set variable fixture | 导入 `set_variable.agent_import.json` | 自动创建/设置变量 | P0 |
| REG-007 | AgentImport invalid pin fixture | 导入 `invalid_pin.agent_import.json` | 失败回滚 | P0 |
| REG-008 | AgentImport forbidden pos fixture | 导入 `forbidden_pos.strict.agent_import.json` | 合约失败；无写入 | P0 |
| REG-009 | MCP strict import failure | 运行 `strict_import_default_failure.mcp.json` | 错误结构稳定；包含 rollback 信息 | P1 |
| REG-010 | MCP full blueprint scope | 运行 `legacy_full_blueprint_scope.mcp.json` | 兼容旧 full blueprint scope 返回 | P2 |
| REG-011 | v0.2.0 FullCoverage | 导入/校验 `v2.3_FullCoverage.json` | 旧覆盖集不回归；失败项有明确兼容说明 | P1 |
| REG-012 | 输出 schema 版本 | 检查 LogicJson/LogicMD/RawJsonRef schema 字段 | schema 值稳定，便于客户端分支处理 | P0 |

## 建议记录格式

```json
{
  "case_id": "PERF-001",
  "asset": "/Game/BlueprintHelperTest/BP_BH_FunctionalActor.BP_BH_FunctionalActor",
  "raw_json_bytes": 10240,
  "logic_json_bytes": 3200,
  "logic_md_bytes": 1800,
  "raw_token_estimate": 2560,
  "logic_json_token_estimate": 800,
  "logic_md_token_estimate": 450,
  "status": "pass"
}
```

## 验收标准

- 性能测试不只看成功失败，还要记录响应大小和耗时趋势。
- Logic 输出的核心指标是：足够理解、足够短、不可误导导入。
- Fixture 回归应进入 CI 或至少作为发布前脚本执行。
