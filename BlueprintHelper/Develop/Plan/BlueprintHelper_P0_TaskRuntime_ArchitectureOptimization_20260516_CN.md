# BlueprintHelper P0 TaskRuntime 架构优化进度 2026-05-16

## 目标

从四层架构报告中的 P0 项开始做行为保持型重构，优先降低 `BlueprintHelperTaskRuntimeService.cpp` 的中心化耦合。

本轮遵守的 coding style：

1. 不新增 namespace。
2. 需要复用的辅助逻辑放入 `/Utils/*Utils` 类。
3. 新增类均提供独立 `.h/.cpp` 文件。
4. 新增分发逻辑不使用超长 if/switch，改用小型表驱动。
5. 本轮不触碰 UI；UI 侧 controller/service/dto/event-driven 作为后续切片处理。

## 本轮 P0 范围

### Phase 1：TaskRuntime 公共类型抽离

状态：完成。

- 新增 `BlueprintHelperTaskRuntimeTypes.h`。
- `FBlueprintHelperTaskRuntimeLoweredStep`、`FBlueprintHelperTaskRuntimeStepRecord`、`FBlueprintHelperTaskRuntimePostOperationRecord` 已从 `BlueprintHelperTaskRuntimeService.h` 移出。
- Cluster headers 已改为依赖公共 types，不再直接依赖 service header。

### Phase 2：ClusterHub 独立实现文件

状态：完成。

- 新增 `BlueprintHelperTaskRuntimeClusterHub.cpp`。
- `FBlueprintHelperTaskRuntimeClusterHub` 构造、lowering 转发、cluster resolve、execute、review evidence 分发已从 `BlueprintHelperTaskRuntimeService.cpp` 移出。
- 新增 `BlueprintHelperTaskRuntimeClusterHubUtils.h/.cpp`，cluster resolve 使用表驱动。

### Phase 3：非 GraphWrite cluster 实现拆回独立文件

状态：完成。

- 新增并补齐以下 cluster `.cpp`：
  - `AssetFactory`
  - `BlueprintVariables`
  - `ClassSettings`
  - `CleanupOwnership`
  - `Component`
  - `DataTable`
  - `ObjectProperty`
  - `Signature`
  - `UMGWidget`
- `BlueprintHelperTaskRuntimeService.cpp` 中对应 cluster 方法实现已移除。
- 新增 `BlueprintHelperTaskRuntimeClusterExecutionUtils.h/.cpp`，承接 cluster 共享执行、review evidence 和失败结果构造逻辑。

## 验证

状态：通过。

编译命令：

```powershell
E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild
```

结果：

- `Result: Succeeded`
- 新增 TaskRuntime Hub、cluster、utils 源文件均参与编译。

## 当前结论

P0 TaskRuntime 切片已完成：

- Service 从“聚合 controller + cluster 实现 + hub 分发 +共享执行工具”的混合角色，收敛为运行时 orchestration/controller。
- ClusterHub 成为独立分发器。
- 各业务 cluster 具备独立 `.h/.cpp` 文件。
- 新增分发点使用表驱动，避免继续扩大长 if/switch。

## 剩余架构债

1. `BlueprintHelperTaskRuntimeService.cpp` 内仍保留大量 TaskPlan lowering、runtime data、journal、review baseline 辅助逻辑；建议下一轮继续拆到 `TaskRuntimeLoweringService`、`TaskRuntimeJournalService`、`TaskRuntimeReviewService`。
2. `BlueprintHelperTaskRuntimeClusterExecutionUtils.cpp` 当前承接了多 cluster 共享逻辑，后续可按能力继续拆成更细的 `AssetFactoryTaskRuntimeUtils`、`ReviewEvidenceTaskRuntimeUtils` 等。
3. UI 侧 controller/service/dto/event-driven 未在本轮触碰，需要单独 P0/P1 切片处理。

## 阻塞

无当前阻塞。
