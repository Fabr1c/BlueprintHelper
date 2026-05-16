# BlueprintHelper P1-P3 Architecture Optimization Progress

日期：2026-05-16

## 范围

本文承接 `BlueprintHelper_FourLayerArchitecture_CohesionCoupling_Report_20260516_CN.md` 和 P0 TaskRuntime 拆分结果，继续推进 P1/P2/P3。

## P1 实施目标

- [ ] `BlueprintVariableTaskPlanAdapter` 从 `BlueprintHelperTaskRuntimeService.cpp` 移出，形成独立 `.h/.cpp` 实现。
- [ ] Bridge route planner 从长 if/switch 改成数据驱动表和 Utils 类，避免入口层继续堆条件判断。
- [ ] GraphStatement composer 的匿名命名空间工具移动到 `Utils` 类，为后续 pure IR / UE binding 拆分留出复用点。
- [ ] TaskSpec 生产入口保持 Python compiler 为唯一默认生产 compiler；TS compiler 仅作为 schema / parity / fixture helper 保留。
- [ ] MCP surface 保持 lifecycle-only + shared registry adapter，不恢复 frozen direct tool registrations。

## P2 实施目标

- [ ] 增加架构边界测试：
  - `Shared/**` 不允许 include `Systems/**`。
  - `Systems/**` 不允许 include `Entry/**`。
  - `Runtime/TaskRuntime/Clusters/**` 不允许 include `BlueprintHelperTaskRuntimeService.h`。
- [ ] 增加 TaskPlan adapter 合约测试：每个 `Public/Runtime/TaskRuntime/TaskPlanAdapters/**/*.h` 必须有对应 `Private/.../*.cpp`，除非有显式豁免。
- [ ] 增加 route registry 唯一性测试：Bridge route command 只能在 route planner 数据表声明一次。
- [ ] 增加生产 compiler 入口测试：CLI/MCP 非测试源码不得直接使用 TS fallback compiler。

## P3 收口目标

- [ ] 更新本文状态，明确完成项和遗留项。
- [ ] 运行 TaskCore Node/Python tests。
- [ ] 运行 UE 编译。
- [ ] P3 未在四层架构报告中给出独立实现项；本轮按工程收口和防回退测试处理。UI controller/service/dto/event-driven 改造需要单独切片，避免和 Runtime/Bridge 重构混在同一轮扩大风险。

## 当前状态

- 开始：P0 已完成 TaskRuntime cluster 拆分并通过 UE build。
- P1 已完成：
  - `BlueprintVariableTaskPlanAdapter` 已移出 `BlueprintHelperTaskRuntimeService.cpp`，新增独立 adapter `.cpp` 与 adapter utils `.h/.cpp`。
  - `BlueprintHelperBridgeRoutePlanner` 已改为 route table + `FBlueprintHelperBridgeRoutePlannerUtils`，移除长 if/switch 分发。
  - `BlueprintHelperGraphComposer` 的 endpoint/pin 解析和兼容连线逻辑已移到 `FBlueprintHelperGraphComposerUtils`。
  - LogicMd/LogicJson read service 不再从 Systems 反向依赖 Entry module singleton，改为从 Entry 注入 ExportService；默认构造保留测试可用路径。
  - RuntimeProfileService 不再 include Entry module，通过 bridge-running provider 注入状态查询。
- P2 已完成：
  - 新增 `AgentFaceService/task-core/src/tests/architecture/architecture-boundaries.test.ts`。
  - 已覆盖 Shared->Systems、Systems->Entry、TaskRuntime cluster->service header、TaskPlan adapter `.h/.cpp`、route command 唯一性、CLI/MCP Python production compiler 入口。
  - Shared service 实现中依赖 Systems 的 `.cpp` 已搬到 `Private/Systems/SharedServices`，Public Shared API 保持不变。
  - Bridge validation DTO 已从 Entry validator 下沉到 `Shared/Bridge/BlueprintHelperBridgeTypes.h`。
- P3 已完成：
  - 本文记录了完成项、验证结果和遗留项。
  - 本轮 P3 按工程收口处理；UI controller/service/dto/event-driven 改造未混入本轮，需单独切片。

## 验证

- PASS：`node --test AgentFaceService/task-core/build/tests/architecture/architecture-boundaries.test.js`，4/4。
- PASS：`E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`。
- WARN/PENDING：`npm.cmd --prefix AgentFaceService/task-core run test:node` 当前 93/106 通过，13 个失败来自 TaskSpec/fixture 契约不同步：
  - composite/GraphWrite 期望 `blueprint_signature` 依赖步骤，但当前 compiler 未产出。
  - 多个 fixtures/tests 期望 execution_policy 不含 `review_baseline_dirty_asset_policy`，但当前产物含该字段。
  - shared registry expectedToolNames 未包含 `blueprinthelper_query_review_records` / `blueprinthelper_apply_review_action`。
- WARN/PENDING：`npm.cmd --prefix AgentFaceService/task-core run test:python` 当前 44/48 通过，4 个失败同样集中在 composite/custom_event signature dependency 期望。

## 遗留项

1. TaskSpec TS/Python compiler 与 canonical fixtures 需要单独同步：到底以新增 `review_baseline_dirty_asset_policy` 和 signature dependency step 为准，还是回退产物，需要按 TaskSpec 契约决策后统一修改。
2. UI controller/service/dto/event-driven 改造仍未开始，建议作为下一轮独立 P0/P1，以 ReviewPanel action flow 为首个切片。
3. 代码库仍有其他既有 local util class / switch / if 链，本轮只处理 P1/P2 直接触达的 route planner、GraphComposer 和 BlueprintVariables adapter。
