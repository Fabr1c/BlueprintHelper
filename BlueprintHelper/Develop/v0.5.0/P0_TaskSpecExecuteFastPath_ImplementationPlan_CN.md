# P0 TaskSpec Execute Fast Path 实施计划

日期：2026-05-19

## 目标

落地 P0-0、P0-1、P0-2、P0-3，把“从 TaskSpec 输入到 ToolResult 返回”的性能证据链、preview 复用、dry-run 策略和 CallFunction resolution 复用做成通用链路。

默认调用保持现有安全语义；只有显式 32 位 hex preview token、明确 dry-run 策略或 develop 诊断开关才进入对应 fast path / timing path。

## 架构边界

- AgentFace 负责 TaskSpec 输入解析、TaskPlan 编译、preview token 生成/校验、TaskPlan hash、execute 复用决策和 CLI/tool DTO。
- UE TaskRuntime 负责 dry-run 策略、quick preview 执行边界、request-level CallFunction resolution cache 和执行期 evidence。
- 跨层只传 TaskSpec / TaskPlan / 32 位 hex preview token / timing DTO。
- CLI handler 不做 cache 判断；handler 只做 schema parse 和 runner 调用。
- TaskRuntime service 只保留 orchestration，策略分支下沉到独立 policy / cache 类。

## 文件结构

### AgentFace TypeScript

- 修改 `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - 扩展 `PreviewTaskInputSchema` / `ExecuteTaskInputSchema`。
  - `TaskPreviewTokenSchema` 收敛为 32 位 hex 字符串；完整 hash 只保存在 preview store 内部，不作为 CLI/Agent 入参。
- 新增 `AgentFaceService/task-core/src/task/service/task-plan-hash.ts`
  - 提供 `createTaskPlanHash`、`createTaskSpecHash`、`createExecutionPolicyHash`。
  - 使用 key 排序的稳定 JSON 序列化和 `sha256`。
- 新增 `AgentFaceService/task-core/src/task/service/task-preview-cache.ts`
  - Editor-session preview store，上限 64 条，TTL 10 分钟，Editor / Bridge session 结束时整体失效。
  - key 是 32 位 hex token；token 由 128-bit 随机数生成，插入时检测碰撞，碰撞则重新生成。
  - value 包含 TaskPlan、preview result、TaskSpec hash、TaskPlan hash、execution policy hash、target asset ids、asset revision / dirty generation、createdAt、resolved CallFunction facts。
  - 不保存 UE 指针；只保存可序列化 DTO 和 TaskPlan。
- 修改 `AgentFaceService/task-core/src/task/service/task-spec-runner.ts`
  - 新增 `TaskExecuteOptions`。
  - `previewTask` 写 cache 并返回 token。
  - `executeTask` 校验 token 后直接复用 store 内 TaskPlan，跳过 `TaskSpec -> TaskPlan` compile 和 `bridge.preview_task_plan`。
  - token mismatch 返回结构化失败，不静默降级执行。
- 修改 `AgentFaceService/task-core/src/tool-surface/task/task-execution-handlers.ts`
  - 支持 `{ task_spec, develop, preview_token }`。
  - handler 只转发 token 给 runner。
- 修改测试：
  - `AgentFaceService/task-core/src/tests/task/task-spec-runner.regression.test.ts`
  - `AgentFaceService/task-core/src/tests/task/task-execution-handlers.test.ts`

### UE C++

- 修改 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
  - 保留执行编排。
  - 调用 dry-run policy 和 CallFunction cache。
  - `ExecuteTaskPlan` 始终真实执行，不因 `dry_run_mode=none` 跳过 mutation。
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeDryRunPolicy.h`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeDryRunPolicy.cpp`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCallFunctionResolutionCache.h`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCallFunctionResolutionCache.cpp`
- 修改 CallFunction runtime resolution 所在文件
  - `TryResolveTaskRuntimeCallFunctions` 显式接收 request-level cache。
  - 返回 `ResolvedCallFunctionFacts`。
- 修改 GraphStatement 相关 builder 文件
  - 优先消费 runtime facts。
  - facts 缺失时走当前架构内 resolver fallback。

## DTO 设计

```ts
export interface TaskPreviewToken {
  token: string; // 32 hex chars, 128-bit random handle
}

export interface TaskExecuteOptions {
  previewToken?: TaskPreviewToken;
  allowPreviewReuse?: boolean;
}
```

规则：
- token 只标识当前 Editor session preview store 内一次 preview，不承载安全校验本体。
- `task_plan_hash` 保存在 store 内，防止旧 TaskPlan 被误执行。
- `task_spec_hash` 保存在 store 内，防止用户改 TaskSpec 后复用旧 preview。
- `execution_policy_hash` 保存在 store 内，防止 dry-run、review、save 策略变化后误复用。
- asset revision / dirty generation 保存在 store 内，防止用户编辑资产后复用旧 preview。
- `allowPreviewReuse` 默认 `true`，但只有 token 命中且所有内部校验通过才跳过 compile 和 preview。

## Dry-run Policy

```cpp
enum class EBlueprintHelperTaskRuntimeDryRunMode
{
    Full,
    Quick,
    None
};

class FBlueprintHelperTaskRuntimeDryRunPolicy
{
public:
    static FBlueprintHelperTaskRuntimeDryRunPolicy FromTaskPlan(const TSharedRef<FJsonObject>& TaskPlanObject);
    EBlueprintHelperTaskRuntimeDryRunMode GetMode() const;
    bool ShouldRunFullPreview() const;
    bool ShouldRunQuickPreview() const;
    bool ShouldAllowNoPreview(bool bHasValidatedPreviewToken) const;
    FString ToDiagnosticString() const;
};
```

策略：
- `full`：默认路径，保持当前完整 preview。
- `quick`：只做 parse、policy、lowering、SemanticIR、目标解析、CallFunction 预解析和可执行性校验；不生成预览图，不写 Review archive，不 flush graph layout。
- `none`：只能由 AgentFace 在 preview token 校验通过后跳过 `bridge.preview_task_plan`；UE `PreviewTaskPlan` 收到 `none` 必须返回诊断失败。
- UE `ExecuteTaskPlan` 不消费 `dry_run_mode` 来决定是否 mutation。

## TDD Checklist

### P0-0 Timing 收口

- [ ] TypeScript 测试：`develop: true` 返回 `taskspec_parse`、`taskspec_compile`、`bridge.preview_task_plan`、`bridge.execute_task_plan`、`result_wrap`、nested UE timing。
- [ ] TypeScript 测试：`develop` 缺省或为 `false` 时不启动 timing，不返回 `data.timing`。
- [ ] CLI 入口：`--develop` 从参数解析开始计时，覆盖所有 CLI tool 分支。
- [ ] UE runtime：只有 `include_timing=true` 创建 `BlueprintHelper.TimingTrace.v1`。
- [ ] 文档“度量要求”更新 P0 timing 字段清单和字段语义。

### P0-1 Preview Token 复用

- [x] 失败测试：`previewTask` 返回 32 位 hex token。
- [x] 失败测试：同一个 TaskSpec preview 后 execute，Bridge 调用顺序为 `preview_task_plan`、`execute_task_plan`。
- [x] 失败测试：修改 TaskSpec 后复用旧 token，execute 返回 `preview_token_mismatch`。
- [x] 失败测试：目标资产 state snapshot 变化后复用旧 token，execute 返回 `preview_token_mismatch`。
- [ ] 失败测试：token 碰撞时重新生成，不覆盖已有 preview entry。
- [x] 实现 stable hash。
- [x] 实现 Editor-session preview store。
- [x] runner 内部校验 token，handler 不做业务判断。
- [x] develop timing 增加 `preview_token.validate`；TaskPlan 复用由 UE preview store 执行。
- [x] develop timing 证明 token execute 不再出现 `taskspec_compile` 和 `bridge.preview_task_plan`。

### P0-2 `dry_run_mode`

- [ ] TypeScript 测试：`quick` 透传到 UE preview。
- [ ] TypeScript 测试：无有效 token 且 `none` 时返回 `dry_run_mode_none_requires_preview_token`。
- [ ] C++ 测试：policy 正确解析 `full`、`quick`、`none`、缺省值。
- [ ] C++ 测试：UE `PreviewTaskPlan` 收到 `none` 返回诊断失败。
- [ ] quick preview 不写 Review archive、不捕获 before/after snapshot、不触发 graph layout flush。
- [ ] execute 路径真实写入语义不变。

### P0-3 CallFunction Resolution Cache

- [ ] C++ 测试：同一 TaskPlan 两条相同 CallFunction query 只产生一次 miss。
- [ ] C++ 测试：不同 blueprint path、graph path、target object type 或参数类型不会误命中。
- [ ] request-level cache value 只保存纯 DTO。
- [ ] runtime resolution 显式接收 cache。
- [ ] TaskRuntime 创建 cache，并把 hits / misses 写入 develop 诊断。
- [ ] GraphStatementBuilder 优先使用 runtime facts。
- [ ] 删除或收敛重复 resolution 分支。

## 验收命令

```powershell
npm.cmd --prefix .\AgentFaceService\task-core run build
npm.cmd --prefix .\AgentFaceService\task-core run test:node
npm.cmd --prefix .\AgentFaceService\cli run build
npm.cmd --prefix .\AgentFaceService\cli run test:node
E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild
git diff --check
```

## 集成测速

写链路代表样本：

```powershell
node .\AgentFaceService\cli\build\cli\index.js task execute --file "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.3\ArchivedReference\RetiredReviewDebugDocs_20260518\PlanArtifacts\ReviewPanel_UI_Test_TaskSpecs_20260518\04b_write_function_body.json" --develop --format full
```

记录字段：

| 样本 | dry_run_mode | preview token | total_ms | taskspec_compile | bridge.preview_task_plan | bridge.execute_task_plan | nested ue.preview_task_plan total | nested ue.execute_task_plan total | call_function_cache_hits | call_function_cache_misses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 04b_write_function_body | full | no |  |  |  |  |  |  |  |  |
| 04b_write_function_body | quick | no |  |  |  |  |  |  |  |  |
| 04b_write_function_body | none | yes |  |  |  |  |  |  |  |  |

### P0-2 / P0-3 小样本补测（2026-05-20）

测试目录：`D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\PerfProbe\P0P23_20260520011234`

前置链路：
- MCP `blueprint_open_editor` 启动 Editor。
- CLI 执行 `01_create_blueprint_actor.json`：成功，`1599.951ms`。
- CLI 执行 `04_edit_blueprint_signatures.json`：成功，`1082.636ms`。
- MCP `blueprint_close_editor(save_all=false)` 关闭 Editor。

P0-2 `dry_run_mode=quick` 行为验证：

| 样本 | mode | cli_total_ms | bridge.preview_task_plan | nested ue.preview_task_plan total | call_function_resolution | cluster_execute | quick_preview_validate |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `04b_write_function_body` | full | 637.881 | 585.860 | 445.429 | 434.029 | 11.325 | 0 |
| `04b_write_function_body` | quick | 1797.013 | 1744.535 | 289.841 | 289.777 | 0 | 0.009 |
| `04b_write_function_body` | quick round2 | 447.435 | 395.960 | 289.306 | 289.241 | 0 | 0.009 |
| `04b_write_function_body` | full round2 | 1791.927 | 1740.923 | 300.885 | 289.184 | 11.646 | 0 |
| `04b_write_function_body` | quick round3 | 1886.394 | 1836.185 | 288.970 | 288.901 | 0 | 0.009 |
| `04b_write_function_body` | full round3 | 1908.624 | 1857.598 | 298.324 | 287.653 | 10.615 | 0 |

结论：
- `quick` preview 已稳定跳过 `cluster_execute`，只保留 lowering、CallFunction resolution 和 `quick_preview_validate`。
- 本样本与旧 `04b` preview nested 基线 `456.449ms` 相比，quick preview nested 降到约 `289ms`。
- warmed full/quick 同轮差距只有约 `10-12ms`，因为当前 `04b` 的主耗时已经变为 `call_function_resolution`。
- CLI 总耗时存在 Bridge round-trip 噪声，判断 P0-2 收益时优先看 UE nested timing。

P0-3 CallFunction resolution cache 验证：

| 样本 | dry_run_strategy | preview_kind | ue_total_ms | call_function_resolution | cache_hits | cache_misses | cache_entries | resolved_facts |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 单个 `PrintString` | quick | synthetic | 294.300 | 294.235 | 0 | 1 | 1 | 1 |
| 重复 `PrintString` | quick | synthetic | 289.791 | 289.723 | 1 | 1 | 1 | 2 |

结论：
- 同一 TaskPlan 内重复的 `PrintString` 查询第二次命中 request-level cache，`entries=1`。
- resolved fact 返回 stable id：`/Script/Engine.KismetSystemLibrary:PrintString`。
- AgentFace `task preview` wrapper 已补 `--develop` 诊断透传：CLI 结果直接包含 `ue_preview_result`、`dry_run`、`call_function_resolution_cache`、`runtime_facts`；普通非 develop 调用仍不返回这些字段。

`--develop` UE 原始返回透传补测：

| 场景 | 结果 |
| --- | --- |
| `task preview --develop --format json` | `has_ue_preview_result=true`，`ue_preview_operation=preview_task_plan`，`dry_run_strategy=quick`，`dry_run_preview_kind=synthetic`，`cache_hits=1`，`cache_misses=1`，`cache_entries=1`，`resolved_facts=2` |
| `task preview --format json` | `has_timing=false`，`has_ue_preview_result=false`，`has_dry_run=false`，`has_cache=false`，`has_runtime_facts=false` |

P0-2 `none` 防绕过验证：

| 场景 | 结果 |
| --- | --- |
| 无 preview token 执行 `dry_run_mode=none` | CLI preflight 失败，`error_code=dry_run_mode_none_requires_preview_token`，未进入 UE preview/execute 写入 |

## 完成标准

- 普通调用不返回 `data.timing`。
- `--develop` 返回完整 AgentFace + UE nested timing。
- token 命中路径不重复调用完整 preview。
- `quick` preview 保持校验强度，只减少重型预览生成。
- `none` 不能成为外部直接绕过 preview 的公共能力。
- CallFunction 重复查询可被 request-level cache 去重。
- 不改变 Review evidence、DebugBundle、Accept/Reject 状态模型。
## 执行状态（2026-05-19）

- P0-0 Timing 收口：已保持 CLI 接收处到返回处的通用 timing 链路；本轮补齐 preview token 分配、hash/cache store、validate、reuse 的 develop timing 阶段。普通调用仍不返回 `data.timing`。
- P0-1 Preview Token 复用（旧实现）：已实现 `TaskPreviewTokenSchema`、稳定 hash、runner 显式 preview cache 依赖、execute token 校验与 TaskPlan 复用；无效 token 在 execute 前结构化失败。
- P0-1 后续修订（2026-05-20）：preview token 方案收敛为 32 位 hex 短句柄 + Editor-session preview store；execute 复用 store 内 TaskPlan，需要移除 token execute 阶段的二次 compile，并支持跨 CLI 进程复用。
- P0-2 `dry_run_mode`：已实现 UE dry-run policy；UE preview 收到 `none` 会诊断失败；AgentFace 无 token 的 `none` execute 会在 Bridge 调用前失败；quick preview 只做 lowering 和 CallFunction resolution 验证后返回合成 dry-run 成功。
- P0-3 CallFunction Resolution Cache：已实现 request-level 纯 DTO cache、hits/misses/entries develop 数据、runtime facts、`resolved_stable_id` 写回 lowered payload 并由 GraphStatementBuilder 优先消费；cache key 已保留 category priority 顺序并纳入 argument/target/return pin type 上下文。
- 审查修复：已修复直接 TaskSpec 顶层 `preview_token` 被吞掉、`dry_run_mode=none` 无 token 仍触发 UE preview、quick preview Blueprint 缺失仍成功、runtime pre-resolution 语义上下文不足、cache key 折叠 order-sensitive 字段、resolved fact 写回晚于 lowering 等问题。
- 验证：`task-core build`、`task-core test:node`、`cli build`、`cli test:node`、UE 5.6 `TemplateEditor Win64 Development` 构建均已通过；最终 `git diff --check` 已通过。

## 执行状态（2026-05-20）

- P0-1 32 hex token 已落地：`TaskPreviewTokenSchema` 改为 32 位 hex 字符串，preview response 只向 Agent 暴露短句柄。
- 旧 AgentFace 进程内 `TaskPreviewCache` 已移除，TaskPlan 缓存迁移到 UE Editor 生命周期内的 `FBlueprintHelperTaskPreviewStore`。
- UE `preview_task_plan` 在收到 `preview_token_request` 后把 TaskPlan、TaskSpec hash、TaskPlan hash、执行策略 hash 和 preview passed 状态写入 preview store。
- UE `execute_task_plan` 支持 `{ preview_token, task_spec_hash }` 输入；token 命中后由 UE preview store 取出 TaskPlan 并执行，不要求 AgentFace 再发送 `task_plan`。
- UE token execute 会重新计算目标资产 state snapshot；如果资产存在性、磁盘时间戳、包加载状态或 dirty 状态和 preview 时不一致，则返回 `preview_token_mismatch`。
- AgentFace token execute 路径只读取 TaskSpec 文件并计算 TaskSpec hash，然后直接调用 `bridge.execute_task_plan`；该路径不再执行 `taskspec_compile`，也不再调用 `bridge.preview_task_plan`。
- 跨 CLI 实测通过：`01_create_blueprint_actor.json` preview 返回 `2fc5391d4aaea4f274322bb67c008af1`，另一个 CLI execute 带 token 成功；execute timing 中无 `taskspec_compile` 和 `bridge.preview_task_plan`。
- 失效实测通过：同 token 改用另一个 TaskSpec 返回 `preview_token_mismatch` / `task_spec_hash`；同 token 在成功 execute 后再次执行同一 Spec 返回 `preview_token_mismatch` / `preview_token.asset_state`。
- 验证通过：`npm.cmd run build` / `npm.cmd run test:node` for `AgentFaceService/task-core`，`npm.cmd run build` / `npm.cmd run test:node` for `AgentFaceService/cli`，UE 5.6 `TemplateEditor Win64 Development` 构建。
- 未完成项：token 碰撞重试已在实现中存在，但尚未补充专门测试；UE 当前记录的是通用 target asset state snapshot，不提供细粒度内容 generation 计数。
