# P1 TaskSpec Compiler Fast Path 实施计划

日期：2026-05-19

## 目标

P1 处理 TaskSpec 编译固定成本和编译输出体积。当前抽样显示 Python 子进程编译约 43-60ms，TS in-process 编译约 0.4-3ms；这不是 2-5s 主瓶颈，但在 P0 降低 preview/execute 成本后会变得更明显。

P1 的目标不是替换 canonical compiler，而是建立可控的 fast path / worker path，并用 parity gate 防止 TaskSpec 到 TaskPlan 语义漂移。

## 范围

- P1-4：TaskSpec 编译优先走 in-process fast path 或 long-lived Python worker。
- P1-5：减少 Python compile 无用输出。
- 只改 AgentFace compiler / runner / schema / tests，不改 UE TaskRuntime 执行语义。

## 前置决策

必须在实现前二选一：

| 策略 | 适用条件 | 风险 | 默认建议 |
| --- | --- | --- | --- |
| TS in-process fast path + Python canonical fallback | TS compiler 已覆盖目标 TaskSpec 类型，并能通过 parity tests | 双 compiler 可能漂移 | P1 首选 |
| Long-lived Python worker | 当前 Python compiler 仍是唯一可靠实现 | worker 生命周期、超时、错误隔离复杂 | 作为 fallback |

如果 P0 后 `taskspec_compile` 仍低于总耗时 3%，P1 先只做 P1-5 输出裁剪和 parity harness，不急于启用生产 fast path。

## 架构边界

- compiler selection 属于 `task-core` service，不放在 CLI command handler。
- compiler policy 用 registry / strategy 表达，不写长 `if` / `switch`。
- Python compiler 继续作为 canonical fallback。
- TS fast path 只能覆盖已通过 parity gate 的 TaskSpec 类型。
- 编译输出裁剪不能改变 runner 所需 TaskPlan 语义。

## 文件结构

- 修改 `AgentFaceService/task-core/src/task/compiler/task-spec-compiler.ts`
  - 抽出 compiler strategy 接口。
  - 返回统一 `CompiledTaskPlan` DTO。
- 新增 `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`
  - 按 TaskSpec kind / feature 注册 compiler strategy。
  - 支持 `canonical_python`、`ts_fast_path`、`python_worker`。
- 新增 `AgentFaceService/task-core/src/task/compiler/task-compiler-policy.ts`
  - 根据 feature coverage、parity gate、env/config 决定使用哪个 strategy。
- 新增 `AgentFaceService/task-core/src/task/compiler/task-plan-parity.ts`
  - 对 TS/Python 输出做规范化比较。
  - 忽略允许不同的 diagnostic 字段。
- 可选新增 `AgentFaceService/task-core/src/task/compiler/python-compiler-worker.ts`
  - 只有选择 Python worker 策略时实现。
  - worker 必须有请求 id、超时、stderr capture、崩溃重启。
- 修改 `AgentFaceService/task-core/src/task/service/task-spec-runner.ts`
  - 只消费 compiler service，不直接知道 Python/TS 细节。
  - develop timing 记录 `taskspec_compile.strategy`。
- 修改 Python compile 入口
  - 默认只输出 `task_plan`。
  - `bridge_payload`、`task_plan_summary`、debug artifacts 只在 diagnostic mode 输出。
- 新增/修改测试：
  - `AgentFaceService/task-core/src/tests/task/task-compiler-policy.test.ts`
  - `AgentFaceService/task-core/src/tests/task/task-plan-parity.test.ts`
  - `AgentFaceService/task-core/src/tests/task/task-spec-runner.regression.test.ts`

## Compiler Strategy 接口

```ts
export interface TaskCompilerStrategy {
  readonly id: 'canonical_python' | 'ts_fast_path' | 'python_worker';
  canCompile(taskSpec: TaskSpec): boolean;
  compile(taskSpec: TaskSpec, options: TaskCompileOptions): Promise<CompiledTaskPlan>;
}

export interface CompiledTaskPlan {
  taskPlan: TaskPlan;
  diagnostics?: TaskCompileDiagnostics;
  strategyId: string;
}
```

## TDD Checklist

### P1-4 Fast Path / Worker

- [ ] 测试：默认 policy 在未覆盖 TaskSpec 类型时选择 `canonical_python`。
- [ ] 测试：已覆盖且 parity 通过的 TaskSpec 类型选择 `ts_fast_path`。
- [ ] 测试：fast path 编译失败时返回结构化错误，不静默执行 fallback 生成的不同 TaskPlan。
- [ ] 测试：`develop` timing 记录 `taskspec_compile.strategy`。
- [ ] 测试：TS/Python 输出 parity 失败时禁用该 TaskSpec 类型 fast path。
- [ ] 实现 compiler registry。
- [ ] 实现 compiler policy。
- [ ] 将 runner 改为依赖 compiler service。
- [ ] 若选择 Python worker，加入 worker health check、request timeout、stderr diagnostic、崩溃重启。

### P1-5 输出裁剪

- [ ] 测试：普通 compile 输出只包含 runner 必需的 `task_plan`。
- [ ] 测试：diagnostic mode 能返回 `bridge_payload`、`task_plan_summary`、compiler debug info。
- [ ] 测试：runner 不依赖被裁剪字段。
- [ ] 修改 Python compiler 输出 DTO。
- [ ] summary 统一由 runner 或共享 helper 生成，避免 Python/TS 双源。

## 验收命令

```powershell
npm.cmd --prefix .\AgentFaceService\task-core run build
npm.cmd --prefix .\AgentFaceService\task-core run test:node -- task-compiler-policy
npm.cmd --prefix .\AgentFaceService\task-core run test:node -- task-plan-parity
npm.cmd --prefix .\AgentFaceService\task-core run test:node -- task-spec-runner
git diff --check
```

## 性能验证

对同一批 TaskSpec 记录：

| 样本 | strategy | taskspec_compile_ms | output_bytes | parity_status | fallback_reason |
| --- | --- | ---: | ---: | --- | --- |
| 04b_write_function_body | canonical_python |  |  |  |  |
| 04b_write_function_body | ts_fast_path |  |  |  |  |
| 04b_write_function_body | python_worker |  |  |  |  |

完成标准：
- fast path 支持类型编译固定成本降到毫秒级，或 Python worker 去掉每次 spawn 成本。
- 所有启用 fast path 的 TaskSpec 类型必须有 parity gate。
- 编译输出体积下降，不影响普通 execute。
- 生产入口策略与测试保持一致。

## 风险控制

- 不因为追求速度降低 TaskPlan 契约严谨性。
- 不让 CLI handler 持有 compiler 策略分支。
- 不用临时字段兼容旧 compiler 输出；发现旧字段依赖时改 runner 契约或移除旧依赖。
- 不把 TS compiler 扩展成第二套无测试的业务解释器。

