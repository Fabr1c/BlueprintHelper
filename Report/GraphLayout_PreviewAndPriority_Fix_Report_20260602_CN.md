# GraphLayout Preview 与优先级修复报告 - 2026-06-02

## 改动原因

Layout Preview 与真实 GraphWrite 生成图暴露出多层问题：

1. Preview 内置 sample 的部分纯数据节点被实例化为带 exec pin 的 `Print String`。
2. Preview 的非线性数据语义、Occupancy 语义存在未连接 exec pin、节点重叠、避让行错误。
3. 真实 solver 缺少跨 Block 与 Block 内数据输入簇的优先级传播，后创建/低优先级节点可能反向挤开高优先级节点。
4. Preview 和真实生成图都存在 exec 主干没有按 UE 原生 pin baseline 拉直的问题。

## 改动过程

采用 RED/GREEN 方式逐项收束：

1. 为 Preview pure data、exec context straighten、Occupancy link、Occupancy blocker、Block order、Data priority、Exec pin baseline 分别增加 focused 自动化测试。
2. 修正 `FGraphLayoutPreviewSampleFactory` 中纯数据 sample 的 node kind 与 Occupancy sample links。
3. 扩展 `FGraphLayoutPreviewSemanticProjector`，按 5 个语义分别处理 Preview sample 的真实布局语义，避免把多种语义混入一个 pattern。
4. 扩展 `FNodeSnapshot` 与 `FBlueprintHelperGraphLayoutCoordinator`，把 layout block/order 信息传入 solver。
5. 在 `FSolver` 中统一 root、detached root、input pin order、pure data cluster 的优先级排序。
6. 新增 `FGraphLayoutExecPinAnchor`，让 Preview projector 与真实 solver 共享 exec pin anchor。
7. 用真实 GraphWrite E2E 截图做像素抽样，最终将 `EventEntry` Y anchor 校准到 `62.5f`，把 `Event -> Branch` 白线偏差从约 7px 收敛到约 1px。

## 改动范围

代码文件：

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutExecPinAnchor.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutExecPinAnchor.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

文档文件：

- `Debug/GraphLayout_Preview_IssueSummary_20260602_CN.md`
- `Report/GraphLayout_PreviewAndPriority_Fix_Report_20260602_CN.md`

## 验证结果

自动化：

- Build: `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`，成功。
- GraphLayout 全组: `D:\UEProjects\Template\Saved\Automation\GraphLayout_All_AfterExecPinAnchor625_20260602_175827\index.json`，`succeeded=56`，`failed=0`，`notRun=0`。

真实 E2E：

- `D:\UEProjects\Template\Saved\Automation\GraphWriteConnectivityValidationE2E_AfterExecPinAnchor625_20260602_175936\summary.json`
- `ok=true`
- `positive_execute_passed=true`
- `positive_readback_exec_links=3`
- `positive_readback_data_links=1`
- `negative_residue_absent=true`

截图：

- `D:\UEProjects\Template\Saved\BlueprintHelper\Debug\Screenshots\graph_layout_exec_pin_anchor625_e2e_20260602_175936_001_20260602_100012_hr7k3khcAF0ChNuxkznwKA.png`
- 截图像素抽样：`Event -> Branch` 白线 `DELTA_Y_FIRST_LAST=-1`，已从修复前约 `-7` 收敛到抗锯齿级别。

## 结论

本轮修复符合 AGENTS 架构要求：没有把业务状态或布局语义塞进 UI widget；新增能力落在通用 GraphLayout model/solver/projector 边界；Preview 与真实 solver 共享 `FGraphLayoutExecPinAnchor`，避免局部硬编码分叉。真实 E2E 与截图验证均通过。
