# P0 TaskSpec Execute Fast Path 实施计划

日期：2026-05-19

## 目标

落地 P0-0、P0-1、P0-2、P0-3，把“从 TaskSpec 输入到 ToolResult 返回”的性能证据链、preview 复用、dry-run 策略和 CallFunction resolution 复用做成通用链路。

默认调用保持现有安全语义；只有显式 preview token、明确 dry-run 策略或 develop 诊断开关才进入对应 fast path / timing path。

## 架构边界

- AgentFace 负责 TaskSpec 输入解析、TaskPlan 编译、preview token、TaskPlan hash、execute 复用决策和 CLI/tool DTO。
- UE TaskRuntime 负责 dry-run 策略、quick preview 执行边界、request-level CallFunction resolution cache 和执行期 evidence。
- 跨层只传 TaskSpec / TaskPlan / preview token / timing DTO。
- CLI handler 不做 cache 判断；handler 只做 schema parse 和 runner 调用。
- TaskRuntime service 只保留 orchestration，策略分支下沉到独立 policy / cache 类。

## 文件结构

### AgentFace TypeScript

- 修改 `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - 扩展 `PreviewTaskInputSchema` / `ExecuteTaskInputSchema`。
  - 新增 `TaskPreviewTokenSchema`：`preview_id`、`task_plan_hash`、`task_spec_hash`、`execution_policy_hash`、`created_at`。
- 新增 `AgentFaceService/task-core/src/task/service/task-plan-hash.ts`
  - 提供 `createTaskPlanHash`、`createTaskSpecHash`、`createExecutionPolicyHash`。
  - 使用 key 排序的稳定 JSON 序列化和 `sha256`。
- 新增 `AgentFaceService/task-core/src/task/service/task-preview-cache.ts`
  - process-local bounded cache，上限 64 条，TTL 10 分钟。
  - value 包含 preview id、TaskPlan、hash、execution policy hash、target asset ids、createdAt、resolved CallFunction facts。
  - 不落盘，不跨 CLI process 共享，不保存 UE 指针。
- 修改 `AgentFaceService/task-core/src/task/service/task-spec-runner.ts`
  - 新增 `TaskExecuteOptions`。
  - `previewTask` 写 cache 并返回 token。
  - `executeTask` 校验 token 后跳过 `bridge.preview_task_plan`。
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
  preview_id: string;
  task_plan_hash: string;
  task_spec_hash: string;
  execution_policy_hash: string;
  created_at: string;
}

export interface TaskExecuteOptions {
  previewToken?: TaskPreviewToken;
  allowPreviewReuse?: boolean;
}
```

规则：
- `preview_id` 只标识本 AgentFace process 内一次 preview。
- `task_plan_hash` 防止旧 TaskPlan 被误执行。
- `task_spec_hash` 防止用户改 TaskSpec 后复用旧 preview。
- `execution_policy_hash` 防止 dry-run、review、save 策略变化后误复用。
- `allowPreviewReuse` 默认 `true`，但只有 token 完整且 cache 命中才跳过 preview。

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

- [ ] 失败测试：`previewTask` 返回完整 token。
- [ ] 失败测试：同一个 TaskSpec preview 后 execute，Bridge 调用顺序为 `preview_task_plan`、`execute_task_plan`。
- [ ] 失败测试：修改 TaskSpec 后复用旧 token，execute 返回 `preview_token_mismatch`，不调用 `execute_task_plan`。
- [ ] 实现 stable hash。
- [ ] 实现 preview cache。
- [ ] runner 内部校验 token，handler 不做业务判断。
- [ ] develop timing 增加 `preview_token.validate` 和 `preview_token.reuse_task_plan`。

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
- P0-1 Preview Token 复用：已实现 `TaskPreviewTokenSchema`、稳定 hash、runner 显式 preview cache 依赖、execute token 校验与 TaskPlan 复用；无效 token 在 execute 前结构化失败。
- P0-2 `dry_run_mode`：已实现 UE dry-run policy；UE preview 收到 `none` 会诊断失败；AgentFace 无 token 的 `none` execute 会在 Bridge 调用前失败；quick preview 只做 lowering 和 CallFunction resolution 验证后返回合成 dry-run 成功。
- P0-3 CallFunction Resolution Cache：已实现 request-level 纯 DTO cache、hits/misses/entries develop 数据、runtime facts、`resolved_stable_id` 写回 lowered payload 并由 GraphStatementBuilder 优先消费；cache key 已保留 category priority 顺序并纳入 argument/target/return pin type 上下文。
- 审查修复：已修复直接 TaskSpec 顶层 `preview_token` 被吞掉、`dry_run_mode=none` 无 token 仍触发 UE preview、quick preview Blueprint 缺失仍成功、runtime pre-resolution 语义上下文不足、cache key 折叠 order-sensitive 字段、resolved fact 写回晚于 lowering 等问题。
- 验证：`task-core build`、`task-core test:node`、`cli build`、`cli test:node`、UE 5.6 `TemplateEditor Win64 Development` 构建均已通过；最终 `git diff --check` 已通过。
