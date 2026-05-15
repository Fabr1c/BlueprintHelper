# Smoke Bug Index 2026-05-10

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

来源文档：

- `BlueprintHelper_NewProject_Full_SmokeRun_20260510.md`
- `G:/UnrealPractise/MemorySystemDemo/BlueprintHelper_NewProject_Full_SmokeRun_20260510.md`

## 分类文档

| 分类 | 文档 | 范围 |
|---|---|---|
| AssetFactory | `SmokeBug_AssetFactory_20260510_CN.md` | BPI parent、PrimaryDataAsset、create_asset 合同漂移 |
| MCP Contract | `SmokeBug_MCPContract_20260510_CN.md` | AgentGuide 资源定位、read_context 能力边界、缺失资产诊断 |
| TaskSpec Data / UMG | `SmokeBug_TaskSpecDataUMG_20260510_CN.md` | UMG 跨步骤 dry-run、DataTable 未来行和数字字段 |
| Verification Gaps | `SmokeBug_VerificationGaps_20260510_CN.md` | UE Automation、ReviewPanel、Debug、ObjectProperty 正向验证缺口 |
| ReviewPanel | `ReviewPanelBug_20260510_CN.md` | 用户后续手动 ReviewPanel smoke 暴露的 UI / Reject / Graph diff 问题 |

## 总览

| ID | 分类 | 优先级 | 摘要 | 状态 |
|---|---|---|---|---|
| SMOKE-AF-20260510-01 | AssetFactory | P0 | Blueprint Interface 被创建成 Actor parent，阻断 class settings interface 验证 | 已修复；Automation 通过 |
| SMOKE-AF-20260510-02 | AssetFactory | P1 | DataAsset 创建必须指定具体 UDataAsset 子类；新项目 smoke 需先创建 PrimaryDataAsset 蓝图类，再用该类路径创建 DA 实例 | 已修复；Automation 通过 |
| SMOKE-AF-20260510-03 | AssetFactory | P2 | create_asset legacy schema / 注释与 TaskSpec 能力漂移 | 已记录 |
| SMOKE-MCP-20260510-01 | MCP Contract | P1 | AgentGuide 在新项目插件副本中定位失败，导致 3 个 Node regression 失败 | 已记录 |
| SMOKE-MCP-20260510-02 | MCP Contract | P1 | read_context 暴露多 context 需求，但实现只支持 blueprint_logic | 已记录 |
| SMOKE-MCP-20260510-03 | MCP Contract | P2 | 缺失资产 read_context 返回 partial / empty，不是结构化 blocked issue | 已记录 |
| SMOKE-TS-20260510-01 | TaskSpec Data / UMG | P1 | UMG dry-run 无法解析同一 TaskSpec 内跨步骤 Widget 依赖 | 已记录 |
| SMOKE-TS-20260510-02 | TaskSpec Data / UMG | P1 | DataTable dry-run 不能预览未来行的 update | 已记录 |
| SMOKE-TS-20260510-03 | TaskSpec Data / UMG | P2 | DataTable fields 只接受 string，数字 JSON 容易被丢弃或拒绝 | 已记录 |
| SMOKE-VER-20260510-01 | Verification | P1 | UE Automation 未执行 | 已记录 |
| SMOKE-VER-20260510-02 | Verification | P1 | ReviewPanel 手动环未执行 | 已记录 |
| SMOKE-VER-20260510-03 | Verification | P1 | Debug / DebugBundle 手动环未执行 | 已记录 |
| SMOKE-VER-20260510-04 | Verification | P2 | Object property 正向写入未验证 | 已记录 |
| SMOKE-VER-20260510-05 | Verification | P2 | Negative case 设计混入正常 create_asset 行为 | 已记录 |

## 建议修复顺序

1. `SMOKE-AF-20260510-01`：已修复 BPI parent；继续 smoke 时可解除 class settings / interface 前置阻塞。
2. `SMOKE-AF-20260510-02`：已补 DataAsset class-aware 复用兼容、缺少具体 `data_asset_class` 的 MCP/Python 合同阻断、抽象基类干净拒绝、以及 Blueprint DataAsset class 路径到 generated class 的解析；完整新项目路径应先建 `parent_class=PrimaryDataAsset` 的蓝图类，再用该蓝图资产路径创建具体 DA。
3. `SMOKE-MCP-20260510-01`：修 AgentGuide 资源定位，恢复 Node regression 全绿。
4. `SMOKE-TS-20260510-01` 和 `SMOKE-TS-20260510-02`：补 dry-run 虚拟状态，减少 Agent 被迫拆步骤。
5. 补跑 `SMOKE-VER-*`，再把 ReviewPanel 和 Debug 的状态从 NOT RUN 更新为 PASS / FAIL。
