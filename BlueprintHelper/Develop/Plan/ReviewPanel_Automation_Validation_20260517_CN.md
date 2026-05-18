# ReviewPanel Automation Validation 2026-05-17

## Scope

本记录覆盖本轮 ReviewPanel 自动化回归和修复结果。

边界：本轮使用 UE Automation / UBT 验证，不等同于真实 Editor UI 点击验收。真实 Slate row hover、手动 Accept / Reject 点击、GraphPanel underlay 视觉层级、DebugPanel `LoadBundle` / `CaptureFocus` 仍属于后续 live UI smoke。

## Initial Failure

首轮命令：

```powershell
Automation RunTests BlueprintHelper.Review.UI
```

报告：

- `D:\UEProjects\Template\Saved\Automation\ReviewPanel_UI_20260517_001\index.json`
- 结果：43 succeeded, 7 warnings, 3 failed.

失败项：

1. `BlueprintHelper.Review.UI.NonGraphPanelsDoNotUseAnchorOverlay`
   - Details row highlight 仍返回 overlay widget。
2. `BlueprintHelper.Review.UI.ReviewPanelConstructsWithGenericObjectVisibleChange`
   - GenericObject / DataAsset explicit target 被 Details presenter 兼容路由抢走。
3. `BlueprintHelper.Review.UI.TreeNestsChangesUnderLifecycleRoot`
   - Panel 对没有 `ParentChangeId` 的同资产 child 做了隐式 lifecycle root 挂载。

## Fixes Applied

1. Details row highlight 不再返回旧 overlay widget。
   - 修改：`BlueprintHelperReviewRowHighlightModel.cpp`
   - 结果：Details 与 Components / MyBlueprint / WidgetTree 一样只维护 row highlight state 与 debug message，`BuildOverlay` 返回 `SNullWidget`。

2. ObjectDetails presenter 不再接管独立主 workspace surface。
   - 修改：`BlueprintHelperReviewObjectDetailsPresenter.cpp`
   - 结果：`DataAsset` / `DataTable` / `UMGWidgetTree` explicit targets 不再被 Details 兼容路径接收；GenericObject 仍走 DataAsset presenter 作为主 workspace。

3. Review tree 只按显式 `ParentChangeId` 嵌套 lifecycle root child。
   - 修改：`SBlueprintHelperReviewPanel.cpp`
   - 说明：真实 pending record 的同资产 child 仍由 Store 层 `LinkPendingChildrenToLifecycleRoots` 补齐 `ParentChangeId`；Panel 层不再猜测未规范化输入，避免把无父级关系的同资产 row 错挂到 root 下。

4. ReviewRecord round-trip 测试夹具改为有效 record。
   - 修改：`BlueprintHelperReviewStoreServiceTests.cpp`
   - 说明：当前 Store 合同会删除没有 `VisibleChanges` 的空 record；测试现在添加最小 pending visible change，用真实有效 ReviewRecord 验证 `debug_case_ids` 和 source transaction created-at bounds 持久化。

## Verification

编译：

- `TemplateEditor Win64 Development` passed.

自动化：

| Suite | Report | Result |
|---|---|---|
| `BlueprintHelper.Review.UI` | `D:\UEProjects\Template\Saved\Automation\ReviewPanel_UI_20260517_002\index.json` | 45 succeeded, 8 warnings, 0 failed |
| `BlueprintHelper.Review.VisibleChange` | `D:\UEProjects\Template\Saved\Automation\ReviewPanel_VisibleChange_20260517_001\index.json` | 15 succeeded, 0 warnings, 0 failed |
| `BlueprintHelper.Review.GraphBounds` | `D:\UEProjects\Template\Saved\Automation\ReviewPanel_GraphBounds_20260517_001\index.json` | 2 succeeded, 0 warnings, 0 failed |
| `BlueprintHelper.Review.Action` | `D:\UEProjects\Template\Saved\Automation\ReviewPanel_Action_20260517_001\index.json` | 18 succeeded, 0 warnings, 0 failed |
| `BlueprintHelper.Review.Record` | `D:\UEProjects\Template\Saved\Automation\ReviewPanel_Record_20260517_002\index.json` | 16 succeeded, 0 warnings, 0 failed |

`Review.UI` 的 8 个 warning 均为测试夹具 ObjectPath deprecation warning；没有 error。

`git diff --check` 通过，仅有仓库行尾 CRLF 提示。

## GPT-5.4 High Review Notes

只读复核结论：

- `NonGraphPanelsDoNotUseAnchorOverlay`：实现问题，需移除 Details 旧 overlay 返回路径。
- `ReviewPanelConstructsWithGenericObjectVisibleChange`：路由语义需要收紧，GenericObject / DataAsset 主 workspace 不应被 Details 抢路由。
- `TreeNestsChangesUnderLifecycleRoot`：复核倾向认为测试期望漂移；本轮实现选择将 Panel 层收紧为显式 `ParentChangeId`，并保留 Store 层负责真实 pending record 的 lifecycle child link。

## Remaining Live UI Work

仍未完成：

1. 用真实 Editor UI 打开 ReviewPanel，验证 Final Changes row 选择、hover row action、单行 Accept / Reject、同资产 selection next。
2. 验证 Components / MyBlueprint / WidgetTree / DataTable / DataAsset 的实际 row highlight 视觉和按钮位置。
3. 验证 GraphPanel diff block 是 underlay，不遮挡节点交互。
4. 验证 asset lifecycle root Reject 成功后删除 created asset，并且只在 root 成功后清理 same-asset child reviews。
5. 验证 DebugPanel `LoadBundle`、`CaptureFocus`、真实 UI 操作后的 bundle 内容。

## 2026-05-18 ReviewPanel 自动化总组回归

执行命令：

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.Review;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\ReviewPanel_All_20260518_001'
```

结果：

1. `BlueprintHelper.Review` 总前缀自动化通过。
2. 报告：`D:\UEProjects\Template\Saved\Automation\ReviewPanel_All_20260518_001\index.json`。
3. 汇总：120 total，110 succeeded，10 succeeded with warnings，0 failed，0 not run。
4. warning 性质：日志显示为测试夹具 ObjectPath deprecation warning，不是 ReviewPanel 断言失败。

覆盖范围：

1. Review visible change 聚合、去重、surface routing、颜色和 action identity。
2. ReviewStore / Record / Action / Reject / RejectAll / lifecycle cascade。
3. Review UI presenter 构造、asset context、DataTable / DataAsset / Structure / WidgetTree readonly presenter。
4. Graph bounds / block metadata bounds / Graph resolver。
5. Debug copyable text、summary、semantic hash、needs_action / reject_failed DebugCase 链路。

边界：

1. 自动化通过不等同于 live Editor UI 已验收。
2. 真实 Slate hover、row 内按钮、GraphPanel underlay 视觉层级、DebugPanel `LoadBundle` / `CaptureFocus` 仍归用户手动测试。
3. 后续如果用户手动测试发现视觉或交互问题，应新增 DebugBundle 并回写到手动测试记录。