# BlueprintHelper v0.5.0 性能优化阶段计划索引

日期：2026-05-19

本目录保存 v0.5.0 TaskSpec 执行链路和读链路优化的独立阶段计划。上层文档 `../../BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN.md` 保留背景、测速记录、优先级和总体风险；具体实施拆到本目录，避免主文档继续堆叠执行细节。

## 阶段文档

| 阶段 | 文档 | 覆盖范围 | 状态 |
| --- | --- | --- | --- |
| P0 | `P0_TaskSpecExecuteFastPath_ImplementationPlan_CN.md` | 端到端 timing、preview token 复用、`dry_run_mode`、CallFunction resolution cache | 计划已写 |
| P1 | `P1_TaskSpecCompilerFastPath_ImplementationPlan_CN.md` | TaskSpec compiler fast path / Python worker、compile 输出裁剪、parity gate | 计划已写 |
| P2 | `P2_TaskRuntimeReviewIO_ImplementationPlan_CN.md` | Review IO 批处理、TaskRuntime `PurePrepare -> MainThreadCommit -> PostIO` 三层拆分 | 计划已写 |
| P3 | `P3_ReadPipelineSnapshotCache_ImplementationPlan_CN.md` | 读链路 GameThread 快照、后台格式化、request-local snapshot 复用、纯数据缓存、Bridge gap 细分 | 计划已写 |

## 执行顺序

1. 先执行 P0，保证 fast path 的行为边界、计时证据和 preview 复用正确。
2. P1 只在 P0 timing 能证明 compiler 仍有收益时推进；如果 P0 后主要耗时仍在 Bridge / UE，则 P1 可只做输出裁剪和 parity gate。
3. P2 在 P0 后推进，用 TaskRuntime 三层边界承接 Review IO 优化，不把异步和 IO 分支堆回 `RunTaskPlan`。
4. P3 以读链路 timing 证据触发，优先处理 `bridge - UE route` gap 和 GameThread 快照/后台格式化，不做后台线程直接读 UObject。

## 通用约束

- 所有新增架构能力必须保持通用性、低耦合、高内聚。
- C++ 不新增 namespace；新增类默认独立 `.h/.cpp`。
- 不新增超长 `if` / `switch`；策略应放到 policy / resolver / adapter / cache / coordinator 边界。
- UI 不参与本性能链路的状态协调。
- Review v2 是唯一 Review 架构基线；不新增 legacy fallback。
- 普通 CLI/tool 调用不启动 timing、不返回 `data.timing`；只有 `--develop` 或 `develop: true` 返回诊断。

