# BlueprintHelper UE 5.3/5.4/5.5/5.7 兼容修复记录 - 2026-05-20

## 目标

在不改写 UE 5.6 主实现路径的前提下，让插件源代码按现有兼容模式覆盖 UE 5.3、5.4、5.5、5.7：

1. 兼容差异必须收敛到 `#if`、能力宏、compat helper 或窄 fallback。
2. 不为旧引擎重写主逻辑，不改变 Review / TaskRuntime / UI 架构语义。
3. 只做最小源码插入或无行为变化的清理。

## 本轮源码调整

### 1. EAllowShrinking 低版本 fallback

问题：

- UE 5.3 没有 `EAllowShrinking::No`。
- `BlueprintHelperTaskRuntimePostOperationPlanner.cpp` 和 `BlueprintHelperTaskRuntimeAssetStateService.cpp` 新增了裸 `LeftInline(..., EAllowShrinking::No)` 调用，绕过了现有兼容层。

处理：

- 引入 `Shared/BlueprintHelperVersionCompat.h`。
- 改为 `FBlueprintHelperVersionCompat::LeftInlineNoShrink(...)`。
- 5.6+ 仍走 `EAllowShrinking::No`，5.3 走 bool fallback。

### 2. BuildPlugin 长路径兼容

问题：

- `BlueprintHelperBlueprintVariableTaskPlanAdapterUtils.h/.cpp` 原路径过深。
- BuildPlugin 复制到 HostProject 后路径超过传统 Windows 260 字符限制，5.3 首次验证出现头文件存在但编译器打不开的失败。

处理：

- 将该私有 utils 从：
  - `Private/Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/Utils/`
- 移到：
  - `Private/Runtime/TaskRuntime/Utils/`
- 类名、职责和实现不变，只缩短 BuildPlugin HostProject 内的源码路径。

### 3. UE 5.7 FStringOutputDevice 头文件拆分

问题：

- UE 5.7 将 `FStringOutputDevice` 拆到 `Misc/StringOutputDevice.h`。
- 旧版本没有该头文件。

处理：

- 在 `BlueprintHelperEditorCommandService.cpp` 中新增 5.7+ 条件 include：
  - `#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)`
  - `#include "Misc/StringOutputDevice.h"`
- UE 5.3/5.4/5.5/5.6 不进入该 include。

### 4. UE 5.7 C4702 不可达代码

问题：

- `FBlueprintHelperRuntimeProfileService::BuildActiveProfileState()` 在 `return State;` 后保留了一段旧配置读取代码。
- UE 5.7 将 C4702 作为错误，导致 BuildPlugin 失败。

处理：

- 删除不可达代码块。
- 保持实际运行路径不变：仍由 `FBlueprintHelperSafetyProfileResolver::ResolveSafetyProfile()` 解析安全档位。

## 验证结果

| 引擎版本 | 命令 | 结果 | 说明 |
| --- | --- | --- | --- |
| UE 5.3 | `E:\UE_5.3\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin -Package=D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\B53 -TargetPlatforms=Win64` | PASS | 最终源码状态通过。 |
| UE 5.4 | `E:\UE_5.4\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin -Package=D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\B54 -TargetPlatforms=Win64` | PASS | 最终源码状态通过。 |
| UE 5.5 | `E:\UE_5.5\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin -Package=D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\B55 -TargetPlatforms=Win64` | BLOCKED | UHT 在插件编译前写入 `E:\UE_5.5\Engine\Plugins\VirtualProduction\CaptureData\Intermediate\...\CaptureDataUtils\UHT\*.gen.cpp.tmp` 失败；当前沙箱不能写 UE_5.5 引擎 Intermediate。失败不在 BlueprintHelper 源码编译阶段。 |
| UE 5.6 | `E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin -Package=D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\B56 -TargetPlatforms=Win64` | PASS | 主基线通过；仍有 `STreeView::ItemHeight` 未来废弃 warning。 |
| UE 5.7 | `E:\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin -Package=D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\B57 -TargetPlatforms=Win64` | PASS | 5.7 通过；仍有 `STreeView::ItemHeight` 未来废弃 warning。 |

## 静态检查

- `rg -n "EAllowShrinking::No" BlueprintHelper\Source\BlueprintHelper -g "*.h" -g "*.cpp"`
  - 结果只剩 `BlueprintHelperVersionCompat.h` 内部使用。
- `rg -n "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/Utils/BlueprintHelperBlueprintVariableTaskPlanAdapterUtils" BlueprintHelper\Source\BlueprintHelper -g "*.h" -g "*.cpp"`
  - 结果为空，旧长路径 include 已清理。

## 遗留风险

1. UE 5.5 需要在允许写入 `E:\UE_5.5\Engine\Plugins\VirtualProduction\CaptureData\Intermediate` 的环境中复跑 BuildPlugin，才能得到完整 PASS。
2. UE 5.6/5.7 仍提示 `STreeView::ItemHeight` 废弃 warning，当前不阻塞 BuildPlugin，但建议后续以 5.7+ 兼容项单独处理。
