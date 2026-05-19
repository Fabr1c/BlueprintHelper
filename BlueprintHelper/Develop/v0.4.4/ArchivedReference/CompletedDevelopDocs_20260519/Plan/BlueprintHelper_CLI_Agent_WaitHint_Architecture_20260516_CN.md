# BlueprintHelper CLI Agent 并行与等待提示架构记录 2026-05-16

## 结论

当前 Agent-facing 主入口是 CLI。旧 MCP 生命周期路径已废弃；Editor lifecycle、TaskSpec/ReadSpec、diagnostics、result query 都应从 CLI 文档和 Claude 插件说明中按 CLI 主线描述。

多个普通 Agent 可以同时启动 CLI 进程并发出请求，但进入 UE Bridge 后仍受编辑器侧约束：会触碰 UObject、Blueprint、Graph、AssetRegistry 或 Slate 状态的请求必须串行进入 UE/GameThread。并行能力应放在 Agent 本地读文件、生成 TaskSpec、Python 编译、结果分析、缓存命中和纯静态 preview 上，不应把多路 UE 请求直接压到游戏线程。

## CLI 等待提示

CLI 必须给 Agent 明确的等待信号，否则长时间没有输出会被误判为请求超时或卡死。

- `stdout` 只保留最终 `BlueprintHelper.CliResult.v1` JSON，便于 Agent 稳定解析。
- 等待 UE Bridge 响应时，CLI 向 `stderr` 输出 keep-alive 提示。
- 提示文本必须明确包含 command、elapsed_ms 和继续等待语义。
- 默认 30 秒后首次提示，之后每 30 秒提示一次。
- CLI 创建的 BridgeClient 默认请求超时为 10 分钟，避免 UE-bound preview/execute 被 30 秒传输默认值截断。

示例：

```text
[BlueprintHelper CLI] waiting for UE Bridge response: command=preview_task_plan elapsed_ms=30000. UE-bound requests are serialized on the editor side; keep waiting unless the CLI exits.
```

可调环境变量：

```text
BPH_CLI_BRIDGE_REQUEST_TIMEOUT_MS
BPH_CLI_WAIT_HINT_INITIAL_MS
BPH_CLI_WAIT_HINT_INTERVAL_MS
BPH_CLI_WAIT_HINTS=0
```

## Preview 边界

当前 full preview 不是纯本地操作：

```text
TaskSpec -> Python compiler -> TaskPlan -> Bridge preview_task_plan -> UE Task Runtime dry-run
```

因此 full preview 需要走 UE/GameThread。只有以下部分可以并行：

- TaskSpec schema 校验。
- Python compiler 的本地 TaskPlan 生成。
- 不访问 UE 状态的静态检查。
- 缓存命中的只读上下文与历史结果分析。

如果后续需要真正并行 preview，应拆成两个显式层次：

- `static_preview`: 本地编译和静态 schema/semantic 检查，可并行。
- `ue_preview`: 需要 UE 当前资产状态、函数解析、Graph dry-run、compile/save policy 判断，进入 UE 调度器。

## UE 调度策略

建议后续在 CLI/task-core 或 Bridge 入口前增加单一 UE 调度器：

- UE-bound read/write/ue-preview/execute 都以 Task 进入同一个调度器。
- 调度器对 UE 实际执行并发度保持 `1`。
- read 和 write 可维护各自队列，但最终进入 UE 的执行闸口仍为单通道。
- write 优先级高于可重试 read；write 后必须使相关 read cache 失效。
- Agent 侧可以并行等待多个 CLI 进程，但每个 CLI 都应通过 `stderr` 心跳说明当前仍在等待。

## 读请求去重与缓存

读请求可以减轻 GameThread 压力：

- 对 read-only Bridge command 使用稳定 key：`command + normalized_payload + runtime_profile_generation/cache_epoch`。
- 同 key in-flight read 合并为一个 UE 请求，多个 Agent 等同一个结果。
- 成功读结果使用短 TTL 缓存；默认只缓存 summary/context/profile/debug-summary 等不含写入副作用的结果。
- 任何 write/session/execute/import/save/compile 成功后递增 cache epoch，使相关资产和全局缓存失效。
- preview 因为依赖当前资产状态和 TaskPlan，应默认不跨写入缓存；只允许缓存本地 static preview。

## Agent 约定

- Agent 解析 CLI 结果时只看 `stdout`。
- Agent 看到 `stderr` 的 `waiting for UE Bridge response` 后继续等待，除非 CLI 进程退出。
- Agent 不应通过并发 CLI 写请求制造 UE GameThread 压力；需要并行读能力时优先依赖去重、缓存和静态本地阶段。
