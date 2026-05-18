# BlueprintHelper Review 系统 Visual-Presenter-Data 架构拆分记录

日期：2026-05-16

## 目标

- Review UI 遵循 `Visual -> Presenter -> Data`。
- Visual 只负责 Slate 控件、用户输入、通知和局部刷新，不直接执行 Review 写命令。
- Presenter 接收 VisualEvent，读取 DataSnapshot，并调度 CommandService。
- CommandService 包装 ReviewActionService 等写命令。
- 写命令完成后仍通过 ReviewStore 的 DataChanged 路径刷新 Visual。
- Review 目录不再使用 namespace；原 namespace 内容按职责移动到 Utils 类。
- Review UI 内不保留长 switch，枚举映射改为表驱动。

## 本轮拆分

- 新增 `FBlueprintHelperReviewPanelPresenter`：
  - 包装 ReviewStore 读取和 PendingReviewChanged 订阅。
  - 接收 Accept/Reject VisualEvent。
  - 只通过 `FBlueprintHelperReviewPanelDataSnapshot` 读取 pending changes。
- 新增 `FBlueprintHelperReviewPanelCommandService`：
  - 集中包装 AcceptVisibleChange、RejectVisibleChange、RejectLifecycleRootVisibleChange。
  - 保留无 ActionService 时的 fallback 行为。
- 新增 `FBlueprintHelperReviewPanelDataSnapshot`：
  - 承载 PendingChanges、SelectedChangeId、SelectedAssetPath。
  - 用于 Presenter 读取 Data，而不是从 Visual 直接散传状态。
- `SBlueprintHelperReviewPanel` 改造：
  - Store 读取、DataChanged 订阅、Accept/Reject 写命令均改为通过 Presenter。
  - Reject 异步准备继续由 Visual 定时器驱动，但异步任务追踪移入 Utils。
  - 删除旧的不可达 Reject 分支。
- Utils 拆分：
  - `FBlueprintHelperReviewPanelAsyncUtils`
  - `FBlueprintHelperReviewPanelLocalUtils`
  - `FBlueprintHelperReviewDebugBundleUtils`
  - `FBlueprintHelperReviewAssetContextUtils`
- 表驱动替换：
  - ReviewPanel surface overlay 刷新。
  - ReviewPanel geometry overlay 解析。
  - AssetKind/Surface/ChangeKind 文本映射。
  - SurfaceRouter main workspace route 和 legacy fallback route。
- Systems/Shared Review 追加拆分：
  - `FBlueprintHelperReviewedDataCleanupServiceUtils` 接管 ReviewedDataCleanupService 原匿名 namespace 工具函数。
  - `FBlueprintHelperReviewBaselineSnapshotServiceUtils` 接管 BaselineSnapshotService 原局部 namespace 工具函数。
  - `BlueprintHelperReviewTypes.h` 的 enum -> string/color 映射改为表驱动。

## 当前边界

当前 Review UI 的主要写路径已经变为：

```text
Visual click/selection event
  -> ReviewPanel VisualEvent
  -> ReviewPanelPresenter
  -> ReviewPanelCommandService
  -> ReviewActionService / ReviewStore mutation
  -> ReviewStore DataChanged
  -> ReviewPanelPresenter reads pending visible changes
  -> Visual refresh
```

## 验证结果

- `git diff --check`：通过，仅有工作区既有 CRLF 提示。
- `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`：通过。
- `node --test AgentFaceService/task-core/build/tests/architecture/architecture-boundaries.test.js`：4/4 通过。
- `rg -n "^namespace|namespace \{|switch \(|case " BlueprintHelper/Source/BlueprintHelper/Private/UI/Review BlueprintHelper/Source/BlueprintHelper/Public/UI/Review BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review BlueprintHelper/Source/BlueprintHelper/Public/Shared/Review`：无匹配。

## 后续风险

- ReviewPanel 仍有较多 Visual 内部刷新编排函数，这是 Slate 层复杂度，不是服务写命令耦合；后续可以继续把刷新组合拆为 ViewBinder。
- Native row/presenter 组件仍通过回调把按钮事件传回 ReviewPanel；目前已保持 Visual 事件路径，后续可以进一步统一成 ReviewPanelPresenterEventSink。

## 2026-05-16 特判函数扫描问题

本节记录对 `BlueprintHelper/Source/BlueprintHelper` 的静态扫描结果，重点检查是否仍存在违背高复用性原则的硬编码分支、字符串特判、迁移残留和不符合 coding style 的局部工具类。当前结论：Review 主写路径已经按 Visual -> Presenter -> Data 拆分，但 Review target 语义、TaskRuntime evidence、GraphWrite 分类仍存在多处特判实现，需要后续继续收敛。

### P1: Review Surface 路由仍依赖字符串特判

- 证据：
  - `Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTypes.h:173` 的 `BlueprintHelperReviewNormalizeSurfaceForTarget(...)` 将 `TargetKind`、`TargetKey`、`VisualGroupKey`、`LocationKey` 拼成文本后用 `Contains(...)` 判断 Graph、Components、MyBlueprint、UMGWidgetTree、DataTable、DataAsset。
  - `Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTypes.h:434` 的 `BlueprintHelperReviewTargetKindCanRouteToDetails(...)` 用 `TargetKind.Contains(...)` 判断 Details 可路由性。
  - `Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTypes.h:484` 到 `:584` 的 `BlueprintHelperReviewShouldShowIn*` 系列函数仍保留 legacy location 文本判断。
  - `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfaceRouter.cpp:134` 的 `LegacyFallbackMatchesSurface(...)` 又维护一套 surface fallback predicate。
- 风险：新增 TargetKind 或 Surface 时需要同步修改多个函数，容易出现 UI 路由、Details 路由和 legacy fallback 行为不一致。
- 建议：新增 `FBlueprintHelperReviewTargetKindRegistry` 或 `FBlueprintHelperReviewSurfaceRouteRegistry`，由每个 TargetKind 声明 `Surface`、`bCanRouteToDetails`、`DisplayKind` 和 legacy alias；Presenter/Visual 只查 registry，不直接解析字符串。

### P1: Snapshot、Restore、Hash 对 TargetKind 的处理分散硬编码

- 证据：
  - `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.cpp:318` 到 `:522` 按 `blueprint_variable`、`component`、`signature`、`umg_widget`、`datatable_row`、`object_property`、`asset_factory` 分支构建 snapshot。
  - `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp:1296` 到 `:1340` 按同一批 TargetKind 分支选择 restore handler 和 snapshot restore 支持范围。
  - `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewHashService.cpp:401` 和 `:405` 对 `graph_node` / `graph_block` 使用 TargetKind 与 TargetKey 文本双重特判。
- 风险：TargetKind 的能力定义没有集中归属，新增或调整一种 target 需要改 snapshot、restore、hash、route、display 多处实现。
- 建议：抽象 `IBlueprintHelperReviewTargetHandler` 或等价表驱动 handler，统一提供 `BuildSnapshot`、`Restore`、`ComputeHash`、`Route`、`BuildReadableText` 能力；ReviewActionService 和 BaselineSnapshotService 只调度 handler。

### P1: TaskRuntime Review Evidence 存在重复实现和迁移残留

- 证据：
  - `Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:207` 到 `:590` 保留 `FBlueprintHelperTaskRuntimeServiceLocalUtils::TryBuildTaskRuntimeReviewEvidence(...)` 相关 evidence 构建逻辑。
  - `Source/BlueprintHelper/Private/Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.cpp:84` 到 `:895` 存在相似的 evidence 构建、Target 添加和路由逻辑。
  - 当前主执行路径 `Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:4279` 仍调用 ServiceLocalUtils 版本，ClusterHub 的 `BuildReviewEvidence(...)` 是后置路径。
- 风险：同一 TaskRuntime review evidence 规则存在两套来源，后续改 cluster 或 target 映射时可能只更新其中一处。
- 建议：将 pre-step evidence 构建迁出 TaskRuntimeService，统一由 Cluster/ClusterExecutionUtils 或专门的 `FBlueprintHelperTaskRuntimeReviewEvidenceService` 提供；TaskRuntimeService 只负责编排，不保留 per-capability 特判。

### P2: GraphWrite 节点、Link、类型分类仍是字符串启发式

- 证据：
  - `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicProcessor.cpp:206` 的 `ClassifyNode(...)` 用 `TypeKey.Contains(...)` 区分 branch、switch、sequence、loop、delegate、event、variable、call 等。
  - `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.cpp:391` 的 `IdentifyGraphLinkType(...)` 和 `:909` 的 `IdentifyNodeKind(...)` 通过 pin/type/class/member_name 文本判断节点或连线类型。
  - `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp:436` 的 `AddCompareTypeSuffixesForToken(...)` 用类型文本包含关系推导 compare suffix。
- 风险：GraphWrite 对 UE 节点、schema 类型和 TaskSpec 别名的支持会不断堆字符串判断，难以扩展和测试覆盖。
- 建议：建立 `FBlueprintHelperGraphNodeClassifierRegistry`、`FBlueprintHelperGraphLinkTypeRegistry` 和 `FBlueprintHelperGraphCompareTypeRegistry`，把匹配 token、UE class、输出类型、优先级放进规则表或 handler。

### P2: UI Details/Geometry 匹配仍依赖显示文本和模糊匹配

- 证据：
  - `Source/BlueprintHelper/Private/UI/Review/Utils/BlueprintHelperReviewPanelLocalUtils.cpp:99` 的 `ChangeLooksLikeComponentDetailsTarget(...)` 使用 `component` / `组件` 文本判断组件详情目标。
  - `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp:1642` 到 `:1820` 的 Details row geometry 通过候选文本解析、查找 property、递归匹配 Slate 文本。
  - `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewReadableTextUtils.cpp:142` 和 `:218` 通过资产名前缀、descriptor 文本、TargetKind 文本推导 readable suffix。
- 风险：显示文本变化、中文/英文别名、资产命名前缀变化都会影响路由和几何定位，Presenter 对 Data 的读取仍不够结构化。
- 建议：Data 层输出稳定 `FBlueprintHelperReviewTargetDescriptor`，包含 `Surface`、`TargetKind`、`StableObjectPath`、`PropertyPath`、`ComponentPath`、`WidgetName` 等结构化字段；Visual 几何解析优先用稳定 id，文本匹配只作为最后 fallback。

### P2: coding style 仍有局部 LocalUtils 类残留

- 证据：
  - `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp:55` 定义 `FBlueprintHelperReviewActionServiceLocalUtils`。
  - `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp:16` 定义 `FBlueprintHelperReviewStoreServiceLocalUtils`。
  - `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewGraphBounds.cpp:15` 定义 `FBlueprintHelperReviewGraphBoundsLocalUtils`。
  - `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewGraphResolver.cpp:6` 定义 `FBlueprintHelperReviewGraphResolverLocalUtils`。
- 风险：这些不是 namespace，但仍是 `.cpp` 内的局部工具类，不符合“所有类单独 `.h/.cpp`，utils 放 `/Utils/xxxxUtils`”的约束。
- 建议：按职责移动到对应 `/Utils/` 目录并补齐独立 `.h/.cpp`；若只服务单个服务类且无需共享，可以改为 service private 成员函数，避免额外 class。

### 后续整改顺序建议

1. 先做 Review TargetKind/Surface registry，替换 `BlueprintHelperReviewNormalizeSurfaceForTarget(...)`、`BlueprintHelperReviewTargetKindCanRouteToDetails(...)` 和 `LegacyFallbackMatchesSurface(...)` 的字符串特判。
2. 再做 ReviewTargetHandler，把 snapshot/restore/hash/display/route 能力归到同一注册表。
3. 合并 TaskRuntime evidence 的 ServiceLocalUtils 与 ClusterExecutionUtils 重复实现，让 TaskRuntimeService 只做编排。
4. 最后处理 GraphWrite classifier registry 和 UI geometry descriptor，降低一次性变更风险。

## 2026-05-16 架构收敛执行记录

本轮按“保持通用性、高内聚低耦合”的目标继续收敛 Review/TaskRuntime/GraphWrite 相关实现，并避开并行 Agent 正在修改的 CallFunction 代码路径。

### 已完成

1. 新增 `FBlueprintHelperReviewTargetKindRegistry`，集中管理 TargetKind -> Surface、Details 可路由性、handler kind、asset factory surface、snapshot restore 支持、graph node/block legacy 匹配等规则。
2. `BlueprintHelperReviewTypes.h` 中的 surface 路由和 `ShouldShowIn*` 逻辑改为委托 registry；`BlueprintHelperReviewSurfaceRouter` 删除本地 fallback predicate，统一调用 `BlueprintHelperReviewShouldShowOnSurface(...)`。
3. `BlueprintHelperReviewActionService`、`BlueprintHelperReviewBaselineSnapshotService`、`BlueprintHelperReviewHashService` 改为通过 registry 判断 graph node/block、asset_factory、class_default_property、snapshot restore handler，去掉 Review 核心路径上的 TargetKind 字符串特判。
4. 新增 `FBlueprintHelperReviewEnumUtils`，集中解析 Review status/change kind/storage status/surface；`BlueprintHelperReviewStoreService` 和 `BlueprintHelperReviewedDataCleanupServiceUtils` 不再维护各自的状态/surface if 链。
5. `BlueprintHelperReviewGraphBoundsLocalUtils` 与 `BlueprintHelperReviewGraphResolverLocalUtils` 已拆到 `Private/UI/Review/Utils/BlueprintHelperReviewGraphBoundsUtils.*` 和 `BlueprintHelperReviewGraphResolverUtils.*`，满足独立 `.h/.cpp` 与 `/Utils/xxxxUtils` 约束。
6. TaskRuntime pre-step review evidence 构建已从 `BlueprintHelperTaskRuntimeService` 迁到 `FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(...)`，Service 不再保留重复 evidence 构建逻辑。
7. GraphWrite logic 节点分类、link 类型识别、导出节点 kind 识别已集中到 `FBlueprintHelperGraphWriteClassificationUtils`，`LogicProcessor`/`LogicGroupBuilder` 不再各自维护重复分类表。
8. `FBlueprintHelperVersionCompat` 已从 namespace 改为同名 class，现有 `FBlueprintHelperVersionCompat::...` 调用保持不变。

### 扫描结果

- Review 范围内扫描 `TargetKind.Equals(TEXT("asset_factory"))`、`TargetKindLower`、`ChangeKind.Equals(TEXT(...))`、`Status.Equals(TEXT(...))`、`Surface.Equals(TEXT(...))`、`Target.TargetKind == TEXT(...)`、`TargetKey.Contains(TEXT(...))`：无匹配。
- Review 范围内扫描 `namespace` / `using namespace`：无匹配。
- Public Shared 范围内原 `FBlueprintHelperVersionCompat` namespace 已清理；全仓仍有匿名 namespace 残留，集中在 GraphWrite/Debug/Config 等非 Review 文件。CallFunction 相关 namespace 本轮按并行修改约束未触碰。
- Review 范围内仍存在 `FBlueprintHelperReviewActionServiceLocalUtils`、`FBlueprintHelperReviewStoreServiceLocalUtils`。这两个是大体量服务内部编排/序列化工具集合，本轮先消除其中最影响复用的 TargetKind/status/surface 特判；完整拆分需要单独按职责切成 action prepare、snapshot restore、record serialization、collapse/sort、target normalization 等多个 utils/service，避免一次性大拆影响并行改动。

### 验证结果

- `git diff --check`：通过；仅输出现有工作区 LF/CRLF 提示。
- `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`：通过。

### 剩余架构项

1. `FBlueprintHelperReviewActionServiceLocalUtils` 拆分为独立 Action/Restore utils 或 service。
2. `FBlueprintHelperReviewStoreServiceLocalUtils` 拆分为独立 record serialization、visible change collapse、target normalization utils。
3. `GraphStatementBuilder` 中 compare operator/type suffix 仍有类型特判；因 CallFunction 优化正在并行进行，本轮未修改该文件，避免覆盖新增代码。
4. `UI geometry descriptor` 仍需把文本匹配 fallback 进一步下沉为结构化 target descriptor。


## 2026-05-16 ReviewAction/ReviewStore ? GraphStatement ??????

?????????????????? ReviewActionService/ReviewStoreService ??? LocalUtils????? CallFunction/GraphStatement ? namespace ? compare type ??????????????????

### ???

1. ReviewStore ? `FBlueprintHelperReviewStoreServiceLocalUtils` / `FBlueprintHelperReviewStoreServiceUtils` ???????????????
   - `FBlueprintHelperReviewStoreTargetUtils`?target key?scope identity?graph body aggregation?asset lifecycle metadata?evidence target ??????? lifecycle link?
   - `FBlueprintHelperReviewStoreMergeUtils`?visible change collapse?latest-wins merge?record merge?net-no-change ???
   - `FBlueprintHelperReviewStoreJsonUtils`?record/archive/action/visible change/atomic target JSON ?????????
   - `FBlueprintHelperReviewStorePathUtils`?record id ????? record path ???
2. ReviewAction ? `FBlueprintHelperReviewActionServiceLocalUtils` / `FBlueprintHelperReviewActionServiceUtils` ???????????????
   - `FBlueprintHelperReviewActionTargetUtils`?pending target key ???persisted review target ???atomic target ???
   - `FBlueprintHelperReviewActionRecordUtils`?action record ???debug case ???reject option ???reject failure result ? prepared rollback journal ???
   - `FBlueprintHelperReviewSnapshotRestoreService`?snapshot restore?asset_factory reject?Blueprint/component/datatable/struct/object/widget restore?
   - `FBlueprintHelperReviewGraphRollbackService`?graph rollback?owner block conversion?rollback journal ??? graph node reconnect?
   - `FBlueprintHelperReviewRejectService`?reject dispatcher ? lifecycle child cascade reject ???
3. Review action/store ??????????? `FBlueprintHelperReviewStatusUtils`?ActionService ? StoreService ?????? record/change status ?????
4. CallFunction ??? helper namespace ???? `FBlueprintHelperCallFunctionResolverUtils`?`BlueprintHelperCallFunctionResolver.cpp` ??? resolver ???????
5. GraphStatement ??? namespace ????????? utils ??
   - `FBlueprintHelperGraphPatternRegistryUtils`
   - `FBlueprintHelperGraphFragmentDagUtils`
   - `FBlueprintHelperGraphFragmentDagBuilderUtils`
   - `FBlueprintHelperGraphFragmentEvidenceUtils`
   - `FBlueprintHelperGraphSemanticIRUtils`
6. GraphStatementBuilder ? compare operator/type suffix ?????? `FBlueprintHelperGraphStatementTypeUtils`?Builder ??? `ResolveCompareOperatorFunctionName(...)`??????? compare type ?????
7. `FBlueprintHelperGraphStatementTypeUtils` ?? compare type ??????????helper ????? utils class ??????????? file-scope namespace helper?

### ????

- `FBlueprintHelperReviewActionServiceUtils`?`FBlueprintHelperReviewStoreServiceUtils`?`ReviewActionServiceLocalUtils`?`ReviewStoreServiceLocalUtils`?????
- `FunctionResolution` ? `GraphStatement` ???? `namespace` / `using namespace`?????
- Review/UI Review ???? `TargetKind.Equals(TEXT(...))`?`TargetKindLower`?`ChangeKind.Equals(TEXT(...))`?`Status.Equals(TEXT(...))`?`Surface.Equals(TEXT(...))`?`Target.TargetKind == TEXT(...)`?`TargetKey.Contains(TEXT(...))`?????
- `GraphStatementBuilder.cpp` ? compare type/operator ??????????????? `FBlueprintHelperGraphStatementTypeUtils::ResolveCompareOperatorFunctionName(...)` ????

### ????

- `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`????

### ????

??????????? Action/Store LocalUtils ???CallFunction namespace ???GraphStatement namespace ??? compare type ????Review ?? action/store/service ????????????????????? utils/service???????????????
