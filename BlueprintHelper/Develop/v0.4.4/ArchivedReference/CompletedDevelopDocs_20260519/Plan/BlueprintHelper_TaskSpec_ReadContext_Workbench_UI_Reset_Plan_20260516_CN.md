# BlueprintHelper TaskSpec / ReadContext Workbench UI Reset Plan

日期：2026-05-16
状态：已完成初版实现与自动化验证

## 0. 执行进度

- 2026-05-16：进入实现阶段。当前目标是替换旧 Tools 主入口，建立 TaskSpec / ReadContext Workbench 的 Visual / Presenter / Data 边界，并接入首版 `logicmd`、`logicjson` 导出、TaskSpec 预览块和 CallFunction 候选选择。
- 2026-05-16：已完成初版代码落地。Tools 主入口已切换到 `SBlueprintHelperTaskSpecWorkbench`；旧 `SHelperMainWidget` 暂保留但不再作为主入口。
- 2026-05-16：已新增 Workbench Data / Presenter / Systems / Visual 边界，解析、导出、候选、预览、layout 状态同步逻辑均不写在 Slate Widget 内。
- 2026-05-16：已完成 T3D -> ReadContext `logicmd` / `logicjson` 剪切板导出，导出动作不覆写当前输入文本。
- 2026-05-16：已完成 TaskSpec 字段推断预览模型：绿色块表示图逻辑并进入 preview layout，蓝色块表示非图逻辑并固定在左侧 lane。
- 2026-05-16：已完成 CallFunction 候选卡片和 Row 单选 Store 语义；候选构建走 `FBlueprintHelperCallFunctionResolver` 链路，不使用旧全量函数列表作为主路径。
- 2026-05-16：验证完成：`TemplateEditor Win64 Development` 编译通过；编辑器 Bridge 读状态通过；`BlueprintHelper.UI.TaskSpecWorkbench` 自动化 3/3 通过。

## 1. 目标定位

当前 Tools UI 仍残留 v0.1.0 的 T3D -> JSON -> 生成蓝图工具形态。本轮将其重置为 TaskSpec / ReadContext 工作台：

- 用户可以直接导入或编辑 `BlueprintHelper.TaskSpec.v1`。
- 用户可以粘贴某段函数 T3D 文本，并选择 ReadContext 工具内支持的导出格式，例如 `logicmd`、`logicjson`。
- T3D 导出结果只写入剪切板，不覆写当前文本框内容。
- 左侧显示 CallFunction 卡片，每张卡片内列出候选函数 Row；Row 右侧显示单选框，同一 CallFunction 只能选择一个候选函数。
- CallFunction 候选必须走一次正常 TaskSpec 的 CallFunction 解析链路，不能回退为旧 UI 的全量函数列表搜索。
- 下方预览区改为 TaskSpec 预览图。该预览只解析 TaskSpec 意图，不预览真实 UE 蓝图。
- 图逻辑使用绿色矩形块显示，并接入 layout 系统同步预览状态。
- 非图逻辑使用蓝色矩形块显示，固定显示在预览面板左侧。
- 图逻辑 / 非图逻辑分类由 TaskSpec 字段推断，不要求用户额外标注。

## 2. 非目标

- 不保留旧的“从蓝图文本/剪贴板转换为 JSON”“复制 JSON 规则”“从 JSON 生成蓝图”作为主流程。
- 不在该面板直接执行真实蓝图写入。
- 不把 TaskSpec 预览绑定到真实 UE Graph 节点读取结果。
- 不让 Slate Widget 直接调用解析、resolver、layout 或剪切板导出业务逻辑。
- 不用 `FBlueprintGraphWriteFacade::GetAllBlueprintFunctions()` 作为候选函数主来源。

## 3. UI 三层架构

本功能必须保持现有 UI 三层范式：

```text
Visual
  -> Presenter
  -> Command / Service / Coordinator
  -> DataStore mutation
  -> DataChanged
  -> Presenter reads Data snapshot
  -> PresenterEvent
  -> Visual refresh
```

### 3.1 Visual 层

建议新增或替换为：

```text
SBlueprintHelperTaskSpecWorkbench
SBlueprintHelperCallFunctionCandidateList
SBlueprintHelperTaskSpecPreviewPanel
SBlueprintHelperReadContextExportBar
```

职责：

- 只负责 Slate 控件、布局、用户输入事件、列表渲染、单选框渲染、状态提示。
- 输入框文本变化、导出按钮点击、候选 Row 单选变化都转成 VisualEvent。
- 不直接解析 JSON / T3D。
- 不直接运行 TaskSpec resolver。
- 不直接计算预览块布局。
- 不直接写 Data 对象；只接收 PresenterEvent 刷新 UI。

### 3.2 Presenter 层

建议新增：

```text
FBlueprintHelperTaskSpecWorkbenchPresenter
FBlueprintHelperTaskSpecWorkbenchVisualEvent
FBlueprintHelperTaskSpecWorkbenchPresenterEvent
```

职责：

- 接收 VisualEvent，并调度 Command / Service / Coordinator。
- 读取只读 Data snapshot，生成 Visual 可渲染状态。
- 维护用户当前 UI 选择语义，例如当前选中的 CallFunction candidate id。
- 不直接改 Data 对象；所有写入经由 Store / Service。
- 对外只暴露面板需要的 PresenterEvent，不暴露内部 resolver 或 layout 细节。

### 3.3 Data 层

建议新增：

```text
FBlueprintHelperTaskSpecWorkbenchData
FBlueprintHelperTaskSpecWorkbenchStore
FBlueprintHelperInputDocument
FBlueprintHelperTaskSpecParseSnapshot
FBlueprintHelperReadContextExportSnapshot
FBlueprintHelperCallFunctionCardModel
FBlueprintHelperCallFunctionCandidateRowModel
FBlueprintHelperTaskSpecPreviewModel
FBlueprintHelperTaskSpecPreviewBlock
FBlueprintHelperTaskSpecPreviewConnection
FBlueprintHelperTaskSpecPreviewLayoutState
```

职责：

- 保存输入文本、输入类型、解析状态、CallFunction 候选、候选选择、预览模型、布局状态、最近导出结果状态。
- 对 Presenter 提供不可变 snapshot。
- 通过 DataChanged 通知 Presenter 刷新。
- 不依赖 Slate 类型。
- 不持有 UWidget / UEdGraph / UBlueprint 等 UI 或真实资产对象。

## 4. 服务与协调器边界

### 4.1 输入识别与解析

建议新增：

```text
FBlueprintHelperWorkbenchInputClassifier
FBlueprintHelperTaskSpecParseService
FBlueprintHelperT3DTextParseService
```

识别策略：

- 文本可解析为 JSON 且 `schema == "BlueprintHelper.TaskSpec.v1"` 时，识别为 TaskSpec。
- 文本满足 Blueprint T3D 特征时，识别为 T3D。
- 两者都不满足时，识别为 Unknown，并保留原始文本和诊断。

### 4.2 ReadContext 导出

建议新增：

```text
FBlueprintHelperReadContextExportService
FBlueprintHelperReadContextExportRequest
FBlueprintHelperReadContextExportResult
EBlueprintHelperReadContextExportFormat
```

首版格式：

```text
logicmd
logicjson
```

约束：

- 导出服务接收 T3D 解析结果和目标格式，返回导出文本与诊断。
- Presenter 只将导出文本写入剪切板并发出状态事件。
- Store 中的 `InputDocument.RawText` 不应因导出动作改变。
- 后续新增格式只扩展 format enum / strategy，不改 Visual 主结构。

### 4.3 CallFunction 候选协调

建议新增：

```text
FBlueprintHelperTaskSpecCallFunctionCandidateCoordinator
FBlueprintHelperTaskSpecCallFunctionCandidateRequest
FBlueprintHelperTaskSpecCallFunctionCandidateResult
```

链路要求：

```text
TaskSpec text
-> TaskSpec parse
-> normal TaskSpec preview / dry-run path
-> normal CallFunction resolver
-> ambiguous_function_call / candidate_functions diagnostics
-> CallFunction card model
```

规则：

- 候选函数来自正常 TaskSpec CallFunction 链路返回的候选信息。
- 每个 ambiguous CallFunction 形成一张卡片。
- 卡片内每个 candidate 形成一个 Row。
- Row 右侧使用单选框；同一卡片内最多一个 candidate 处于 selected。
- 选中 Row 只更新 Workbench 选择状态和预览高亮。
- 是否把选择写回 TaskSpec 必须由后续显式动作触发；首版不在点击单选时隐式改写输入文本。

### 4.4 TaskSpec 预览模型

建议新增：

```text
FBlueprintHelperTaskSpecPreviewModelBuilder
FBlueprintHelperTaskSpecPreviewClassifier
FBlueprintHelperTaskSpecPreviewLayoutCoordinator
```

PreviewModelBuilder 输入 TaskSpec parse snapshot，输出纯数据预览模型。

PreviewClassifier 根据 TaskSpec 字段推断块类型：

- 图逻辑：`behavior` 中会降低到 GraphWrite / CallFunction / flow / pin / graph mutation 的语义字段。
- 非图逻辑：asset target、scope policy、validation、review/save 策略、class settings、component、variable、metadata、diagnostic-only 字段等非图执行或上下文字段。

分类规则必须集中在 PreviewClassifier，不能散落在 Visual Row 或绘制代码中。

### 4.5 预览 layout 协调

TaskSpec 预览图不读取真实 UE Graph，但图逻辑块仍应接入 layout 系统的状态同步思路：

```text
TaskSpecPreviewModel
-> graph preview blocks
-> PreviewLayoutCoordinator
-> layout state revision
-> positioned graph preview blocks
-> PresenterEvent
-> Visual render
```

规则：

- 绿色图逻辑块参与 layout。
- 蓝色非图逻辑块固定在左侧 lane，只参与垂直堆叠，不参与图布局求解。
- layout state 使用 revision 防止旧计算覆盖新输入。
- 选中 CallFunction candidate 后，对应 preview block 只更新 selection/highlight 状态，不重新解析输入文本。
- 首版可以复用 GraphLayout 的 rule / solver 概念，但不得让 preview layout 依赖真实 `UEdGraph` snapshot。

## 5. 交互流程

### 5.1 输入 TaskSpec

```text
User edits text
-> VisualEvent TextChanged
-> Presenter schedules parse
-> TaskSpecParseService
-> Store updates parse snapshot
-> CandidateCoordinator runs normal TaskSpec CallFunction chain when needed
-> PreviewModelBuilder builds preview model
-> PreviewLayoutCoordinator updates layout state
-> DataChanged
-> Presenter emits refreshed candidate cards and preview blocks
```

### 5.2 输入 T3D 并导出 ReadContext

```text
User pastes T3D
-> VisualEvent TextChanged
-> InputClassifier marks T3D
-> User clicks Export logicmd / logicjson
-> ReadContextExportService builds export text
-> Presenter writes clipboard
-> Store records export status only
-> Input text remains unchanged
```

### 5.3 选择 CallFunction 候选

```text
User clicks candidate radio
-> VisualEvent CandidateSelected(card_id, candidate_id)
-> Store updates selected candidate for that card
-> PreviewModel updates highlight state
-> DataChanged
-> Presenter refreshes left cards and preview panel
```

## 6. 文件落点建议

首版建议按高内聚模块新增目录：

```text
BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/
BlueprintHelper/Source/BlueprintHelper/Private/UI/TaskSpecWorkbench/
BlueprintHelper/Source/BlueprintHelper/Public/Systems/TaskSpecWorkbench/
BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/
```

UI 层文件：

```text
SBlueprintHelperTaskSpecWorkbench.h/.cpp
SBlueprintHelperCallFunctionCandidateList.h/.cpp
SBlueprintHelperTaskSpecPreviewPanel.h/.cpp
BlueprintHelperTaskSpecWorkbenchPresenter.h/.cpp
BlueprintHelperTaskSpecWorkbenchData.h/.cpp
```

系统层文件：

```text
BlueprintHelperWorkbenchInputClassifier.h/.cpp
BlueprintHelperReadContextExportService.h/.cpp
BlueprintHelperTaskSpecCallFunctionCandidateCoordinator.h/.cpp
BlueprintHelperTaskSpecPreviewModelBuilder.h/.cpp
BlueprintHelperTaskSpecPreviewClassifier.h/.cpp
BlueprintHelperTaskSpecPreviewLayoutCoordinator.h/.cpp
```

`SBlueprintHelperMainWindow` 的 Tools 页应从旧 `SHelperMainWidget` 切换到 `SBlueprintHelperTaskSpecWorkbench`。旧 `SHelperMainWidget` 可先保留但不再作为主入口，后续确认无依赖后删除。

## 7. 分阶段实施

### 阶段 A：计划与边界落地

- 将本计划作为实现依据。
- 建立 Workbench Data / Presenter / VisualEvent / PresenterEvent 的基础类型。
- 在 `SBlueprintHelperMainWindow` 中预留新 Tools 页入口。
- 不接入真实解析和导出，先跑通空状态渲染。

### 阶段 B：输入识别与 T3D 导出

- 实现 InputClassifier。
- 实现 T3D -> ReadContext export service 的 `logicmd`、`logicjson` 首版策略。
- 导出按钮只写剪切板，不调用 `SetMainText`。
- 添加测试覆盖“导出不覆写输入框”。

### 阶段 C：TaskSpec 解析与预览模型

- 实现 TaskSpec parse snapshot。
- 实现 PreviewClassifier 字段推断规则。
- 实现绿色图逻辑块、蓝色非图逻辑块的数据模型。
- 蓝色块固定左侧 lane；绿色块交给 PreviewLayoutCoordinator。

### 阶段 D：CallFunction 正常链路接入

- CandidateCoordinator 调用正常 TaskSpec preview / dry-run / CallFunction resolver 链路。
- 从 `ambiguous_function_call` / `candidate_functions` 诊断生成左侧 CallFunction 卡片。
- Row 右侧渲染单选框。
- 单选状态写入 Store，不直接改写 TaskSpec 文本。
- 选中状态同步预览高亮。

### 阶段 E：替换旧 UI 文案与流程

- 移除主入口中 v0.1.0 JSON 蓝图工具按钮和文案。
- 文本框 Hint 更新为 TaskSpec / T3D 双输入语义。
- 左侧区域从“未匹配节点”替换为 CallFunction card list。
- 下方区域替换为 TaskSpec preview panel。

### 阶段 F：验证与闭环

- 单元测试：
  - InputClassifier 区分 TaskSpec / T3D / Unknown。
  - ReadContext export `logicmd` / `logicjson` 不改变 input raw text。
  - PreviewClassifier 根据 TaskSpec 字段稳定分类。
  - 同一卡片 candidate radio 只能选择一个。
  - CandidateCoordinator 不使用旧全量函数列表作为主路径。
- UI / Presenter 测试：
  - VisualEvent -> Presenter -> Store -> DataChanged -> PresenterEvent 链路完整。
  - 选中 candidate 后左侧单选框和预览高亮一致。
- 编译验证：
  - `TemplateEditor Win64 Development` 编译通过。
- 编辑器验证：
  - 打开面板，确认 Tools 页已是 TaskSpec / ReadContext 工作台。
  - 粘贴 TaskSpec 能生成预览块。
  - 粘贴函数 T3D 后导出 `logicmd` / `logicjson` 写入剪切板且文本框不变。

## 8. 验收标准

- 旧 JSON 蓝图工具主流程不再出现在 Tools 页。
- 文本框可容纳 TaskSpec 或 T3D，并能正确识别当前输入类型。
- T3D 导出支持 `logicmd`、`logicjson`，且只写剪切板。
- CallFunction 候选来自正常 TaskSpec CallFunction 链路。
- 每个 CallFunction 卡片内 Row 右侧有单选框，并保持单选语义。
- TaskSpec 预览图显示绿色图逻辑块和蓝色非图逻辑块。
- 图逻辑块通过 preview layout 状态同步位置。
- 非图逻辑块固定在预览面板左侧。
- 解析、导出、候选、预览、layout 逻辑均不写在 Slate Widget 中。
- 实现保持 Visual / Presenter / Data 三层边界，高内聚、低耦合，后续新增 ReadContext 格式或 TaskSpec 字段分类不需要重写 UI 主结构。

## 9. 验证记录

- 编译验证：`E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild` 通过。
- 编辑器链路验证：通过 MCP 打开 `D:\UEProjects\Template\Template.uproject` 后，`blueprint_get_runtime_profile --json "{}" --select status,summary` 返回 `status=completed`、`errors=0`、`modified=false`。
- 自动化验证：`UnrealEditor-Cmd.exe D:\UEProjects\Template\Template.uproject -Unattended -NoSplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.UI.TaskSpecWorkbench" -TestExit="Automation Test Queue Empty" -log` 通过。
- 自动化覆盖：
  - `BlueprintHelper.UI.TaskSpecWorkbench.ClassifiesAndPreviewsTaskSpec`
  - `BlueprintHelper.UI.TaskSpecWorkbench.ExportsT3DReadContextFormats`
  - `BlueprintHelper.UI.TaskSpecWorkbench.StoreCandidateRadioSelection`
- 自动化日志：`D:\UEProjects\Template\Saved\Logs\Template.log` 记录 `Automation Test Queue Empty 3 tests performed`。

## 10. 当前风险

- 正常 TaskSpec CallFunction 链路可能依赖 editor / graph context；CandidateCoordinator 需要明确无 graph context 时的诊断返回，不应让 UI 卡死或静默回退旧函数列表。
- PreviewLayoutCoordinator 如果直接复用真实 GraphLayout snapshot，会引入错误依赖；首版应保持纯 preview block 输入。
- TaskSpec 字段分类初版容易漏掉 create feature 中的复合字段；应把未知字段显示为蓝色诊断块，而不是忽略。
- 旧 `SHelperMainWidget` 可能还有外部入口或测试依赖；替换主入口前需确认引用范围。
