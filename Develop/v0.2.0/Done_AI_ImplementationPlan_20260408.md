# BlueprintHelper AI 实施计划（v0）

## 目标
本文件聚焦“怎么做”，覆盖：
- 模块拆分
- 类与接口建议
- 文件落点
- 分阶段实施顺序
- MVP 验收标准

协议本身见 [Module_BlueprintHelper_AI_Protocol_20260408.md](Module_BlueprintHelper_AI_Protocol_20260408.md)。

---

## 一、实施原则

### 1.1 优先级原则
推荐实施顺序：

1. 先抽离 Core
2. 再加 UE Bridge
3. 最后接 MCP Server

原因：
- 当前最有价值的资产已经在插件内部
- 如果 Core 不先稳定，后续 Bridge 和 MCP 只是把耦合放大
- 先做头less 化，Slate UI 也能继续复用

### 1.2 控制改造范围
当前阶段不建议：
- 直接重写现有生成器
- 一次性引入大量新节点支持
- 在 UE 插件内直接堆完整 MCP 实现

建议策略是“抽离而不推翻”：
- 保留现有 `FBlueprintToTextConverter`
- 保留现有 `TextToBlueprintGenerator`
- 在其外层补应用服务与桥接层

---

## 二、推荐模块拆分

### 2.1 目标三层

#### A. BlueprintHelperCore
职责：
- 蓝图导出 Json
- Json 校验
- Json 导入蓝图
- 编译与诊断结果聚合

建议包含：
- 现有 `FBlueprintToTextConverter`
- 现有 `TextToBlueprintGenerator`
- 新增服务对象与结果 DTO

#### B. BlueprintHelperEditor
职责：
- Tab、菜单、Slate UI
- 手工映射与人工调试入口

建议包含：
- 现有 `FBlueprintHelperModule`
- 现有 `SHelperMainWidget`

#### C. BlueprintHelperBridge
职责：
- 本地桥接监听
- 请求分发
- 任务池
- 编辑器上下文查询
- 任务结果缓存

---

## 三、类设计建议

### 3.1 Core 层新增类

#### A. `FBlueprintHelperExportService`
职责：
- 面向外部暴露导出服务
- 封装“获取目标图表 -> 导出 Json”流程

建议接口：

```cpp
class FBlueprintHelperExportService
{
public:
    bool ExportGraphSelectionToJson(const FBlueprintHelperGraphTarget& Target, FString& OutJson, FBlueprintHelperDiagnosticSet& OutDiagnostics) const;
};
```

#### B. `FBlueprintHelperImportService`
职责：
- 面向外部暴露导入服务
- 封装“校验 -> 生成 -> 编译 -> 结果聚合”流程

建议接口：

```cpp
class FBlueprintHelperImportService
{
public:
    FBlueprintHelperImportResult ImportJsonToGraph(const FBlueprintHelperImportRequest& Request) const;
};
```

#### C. `FBlueprintHelperValidationService`
职责：
- 承担 Json 结构校验、版本校验、规则预检查

建议接口：

```cpp
class FBlueprintHelperValidationService
{
public:
    FBlueprintHelperValidationResult ValidateJson(const FString& JsonText) const;
};
```

#### D. `FBlueprintHelperCompileService`
职责：
- 触发蓝图编译
- 聚合错误与警告输出

#### E. `FBlueprintHelperGraphResolver`
职责：
- 统一解析目标蓝图与图表
- 替代分散在 UI 或模块中的“取当前图表”逻辑

### 3.2 Bridge 层新增类

#### A. `FBlueprintHelperBridgeServer`
职责：
- Named Pipe 或 WebSocket 监听入口
- 管理连接生命周期

#### B. `FBlueprintHelperBridgeRouter`
职责：
- 将外部命令路由到对应应用服务

#### C. `FBlueprintHelperTaskManager`
职责：
- 分配 `taskId`
- 维护任务状态
- 缓存任务结果

#### D. `FBlueprintHelperContextService`
职责：
- 获取当前资产、图表、选择、最近诊断

#### E. `FBlueprintHelperBridgeMessageSerializer`
职责：
- 桥接请求、响应、事件对象序列化与反序列化

### 3.3 DTO 建议
建议新增以下结构体：

```cpp
struct FBlueprintHelperGraphTarget;
struct FBlueprintHelperImportRequest;
struct FBlueprintHelperImportResult;
struct FBlueprintHelperValidationResult;
struct FBlueprintHelperDiagnosticItem;
struct FBlueprintHelperDiagnosticSet;
struct FBlueprintHelperBridgeRequest;
struct FBlueprintHelperBridgeResponse;
struct FBlueprintHelperTaskSnapshot;
```

这些 DTO 的意义是把“领域执行结果”从 UI 文本中解耦出来。

---

## 四、文件落点建议

### 4.1 基于当前插件结构的最小落点
建议仍放在现有插件 `Plugins/BlueprintHelper/Source/BlueprintHelper` 下，先不拆成多个 UE Module。

#### Public
- `Public/Services/BlueprintHelperExportService.h`
- `Public/Services/BlueprintHelperImportService.h`
- `Public/Services/BlueprintHelperValidationService.h`
- `Public/Services/BlueprintHelperCompileService.h`
- `Public/Services/BlueprintHelperContextService.h`
- `Public/Services/BlueprintHelperGraphResolver.h`
- `Public/Bridge/BlueprintHelperBridgeServer.h`
- `Public/Bridge/BlueprintHelperBridgeRouter.h`
- `Public/Bridge/BlueprintHelperTaskManager.h`
- `Public/Bridge/BlueprintHelperBridgeProtocol.h`
- `Public/Types/BlueprintHelperResultTypes.h`

#### Private
- `Private/Services/BlueprintHelperExportService.cpp`
- `Private/Services/BlueprintHelperImportService.cpp`
- `Private/Services/BlueprintHelperValidationService.cpp`
- `Private/Services/BlueprintHelperCompileService.cpp`
- `Private/Services/BlueprintHelperContextService.cpp`
- `Private/Services/BlueprintHelperGraphResolver.cpp`
- `Private/Bridge/BlueprintHelperBridgeServer.cpp`
- `Private/Bridge/BlueprintHelperBridgeRouter.cpp`
- `Private/Bridge/BlueprintHelperTaskManager.cpp`
- `Private/Bridge/BlueprintHelperBridgeProtocol.cpp`

### 4.2 对现有文件的建议改造点

#### `Private/SHelperMainWidget.cpp`
建议：
- 只保留 UI 事件处理与结果展示
- 调用新的 Import / Export / Validation Service

#### `Private/BlueprintHelper.cpp`
建议：
- 继续负责模块启动、Tab 注册、规则读取
- 可在 `StartupModule()` 中挂载 Bridge 启动点
- 但不要把桥接路由逻辑堆进模块类本身

#### `Private/TextToBlueprintGenerator.cpp`
建议：
- 保持“低层生成器”定位
- 不负责外部协议、任务管理、上下文查询

#### `Private/BlueprintTextConverter.cpp`
建议：
- 继续保持纯转换器职责
- 不混入桥接命令或编辑器状态逻辑

---

## 五、Phase 拆解

### 5.1 Phase 1: Core 头less 化
目标：
- 让现有能力脱离 Slate UI 独立复用

任务：
- 新增 GraphResolver
- 新增 ExportService
- 新增 ImportService
- 新增 ValidationService
- 新增 CompileService
- 让 `SHelperMainWidget` 改为调用这些服务

本阶段完成标准：
- 不打开 UI 也能从代码层调用导入导出能力
- UI 功能行为不回退

### 5.2 Phase 2: Bridge 落地
目标：
- 让 UE 编辑器接收外部本地命令

任务：
- 新增 BridgeServer
- 新增 BridgeRouter
- 新增 TaskManager
- 新增 ContextService
- 实现 `get_rule_markdown`
- 实现 `export_selection_to_json`
- 实现 `import_json_to_graph`
- 实现 `compile_blueprint`

本阶段完成标准：
- 外部本地客户端可在无 Slate 手工操作的情况下驱动 UE 执行
- 任务结果可查询

### 5.3 Phase 3: MCP Server 接入
目标：
- 向 IDE AI 暴露标准工具与资源

任务：
- 将 Bridge 命令映射为 MCP tools
- 建立 resources URI
- 支持 `get_task_result`
- 支持最近一次诊断资源

本阶段完成标准：
- IDE AI 能完成一条完整闭环：读规则 -> 生成 Json -> 导入 -> 读结果

### 5.4 Phase 4: 成功率增强
目标：
- 提升 AI 生成蓝图的命中率与可修复性

任务：
- 增加更多节点类型支持
- 增加节点 schema 校验
- 增加 pin alias 修正
- 增加编译错误到 Json 修复建议的回路

---

## 六、MVP 范围

### 6.1 MVP 需要交付什么

#### UE 侧
- Core Service 完成
- Named Pipe Bridge 完成
- 4 个核心命令完成：
  - `get_rule_markdown`
  - `export_selection_to_json`
  - `import_json_to_graph`
  - `compile_blueprint`

#### MCP 侧
- 本地独立服务进程
- 暴露对应 tools
- 暴露最近一次诊断资源

#### IDE / AI 侧
- 可完成以下工作流：
  - 先读规则
  - 再读当前选择 Json
  - 生成 Json
  - 执行导入
  - 再读取编译结果

### 6.2 为什么这是最优先组合
- 对现有插件侵入最小
- 已覆盖最关键的 AI 闭环
- 调试成本低
- 可以快速验证蓝图 VibeCoding 是否成立

---

## 七、验收标准

### 7.1 Phase 1 验收
- UI 仍可正常导出 Json
- UI 仍可正常从 Json 生成蓝图
- Service 层可脱离 UI 单独调用

### 7.2 Phase 2 验收
- 外部客户端能请求 UE 执行导入
- 失败时返回结构化错误
- 成功时返回结构化结果和任务状态

### 7.3 Phase 3 验收
- IDE AI 可调用 MCP tools
- 导入任务结果可通过 task 查询
- 最近一次诊断可通过 resource 读取

### 7.4 整体验收
- 在一个示例蓝图上完成自然语言 -> Json -> 节点生成 -> 编译反馈闭环
- 至少能稳定处理函数节点、变量节点、ForLoop 宏节点

---

## 八、风险与对策

### 8.1 风险：UI 与核心逻辑继续耦合
对策：
- 先做 Service 层，不直接在 Slate 中加桥接逻辑

### 8.2 风险：目标图表定位不稳定
对策：
- 引入显式 `FBlueprintHelperGraphTarget`
- 所有写操作优先依赖显式资产路径与图表名

### 8.3 风险：Bridge 返回只有日志没有结构
对策：
- 强制定义 DTO 与错误码
- 日志只做补充

### 8.4 风险：首版试图支持过多节点
对策：
- MVP 限定当前已支持节点
- 先追求闭环，再扩展覆盖率

### 8.5 风险：GraphResolver 需要新写资产定位逻辑
现状：
- 现有 `GetActiveBlueprintGraph()` 遍历所有打开的 AssetEditor，返回第一个 `FBlueprintEditor` 的 `GetFocusedGraph()`
- 完全没有"按资产路径查找"或"按资产路径 + 图表名定位"的能力

具体缺失：
- 资产未打开时是否需要 `LoadObject<UBlueprint>` 后自动打开编辑器
- 打开后如何切换到指定子图表（EventGraph / 自定义函数图）
- 多个 BlueprintEditor 同时打开时如何区分

对策：
- Phase 1 将 `GraphResolver` 标为高风险项
- MVP 阶段允许降级到"当前焦点图表"模式，但 DTO 中预留 target 字段
- 优先验证 `GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset()` + `FBlueprintEditor::OpenGraphAndBringToFront()` 路径

### 8.6 风险：导入操作无事务语义，失败后图表残留节点
现状：
- `TextToBlueprintGenerator::GenerateBlueprintFromJson` 是纯静态调用
- 节点逐个 Spawn，中途失败不回滚
- `FBlueprintGenerateResult` 只记录 success/count/message，不包含已创建节点列表
- 没有 `FScopedTransaction` 包裹

影响：
- AI 发送有错误的 JSON 时，图表上会残留部分节点，用户只能手动 Undo
- 对 AI 重试修复流程不友好

对策：
- 在 `ImportService` 中用 `FScopedTransaction` 包裹整个 `GenerateBlueprintFromJson` 调用
- 失败时可以整体 Undo
- `FBlueprintGenerateResult` 补充已创建节点列表字段

### 8.7 风险：Bridge IO 线程与 GameThread 的调度
现状：
- UE 编辑器的蓝图操作必须在 GameThread 上执行
- Named Pipe 的监听运行在独立 IO 线程
- 文档未讨论线程模型

影响：
- 跨线程调用蓝图操作会导致崩溃或未定义行为
- 长任务的进度回报需要从 GameThread 回到 Bridge IO 线程

对策：
- Bridge IO 线程收到请求后用 `AsyncTask(ENamedThreads::GameThread, Lambda)` 投递
- TaskManager 使用线程安全容器（`FCriticalSection` 或 `TAtomic`）
- 完成回调从 GameThread 写入 TaskManager，IO 线程轮询或事件通知读取

### 8.8 风险：编译结果获取路径不明
现状：
- 现有代码中没有任何编译相关逻辑
- UE 蓝图编译通过 `FKismetEditorUtilities::CompileBlueprint()` 触发
- 编译结果分散在 `Blueprint->Status`、`FCompilerResultsLog`、`FMessageLog` 等多处

影响：
- `CompileService` 不只是简单封装，需要聚合多个来源的诊断信息
- 编译可能触发级联编译（依赖的子蓝图）

对策：
- Phase 1 就应原型验证编译结果获取方式，不要推迟到 Phase 2
- 优先调研 `FMessageLog("BlueprintLog")` + `FCompilerResultsLog` 的拦截与结构化
- MVP 阶段可只返回 `Blueprint->Status` + 错误数量，不做完整日志结构化

### 8.9 风险：导出依赖剪贴板，AI 工作流不可行
现状：
- `FBlueprintToTextConverter::ConvertClipboardToJson()` 读取系统剪贴板中的 T3D 文本
- 没有直接从 `UEdGraph` 对象导出的路径

影响：
- AI 工作流需要用户手动选节点 + Ctrl+C，无法自动化
- `export_selection_to_json` 命令名义上可用但实际依赖人工操作

对策：
- Phase 1 新增 `ExportFromGraph(UEdGraph*, SelectionMode)` 方法
- 使用 `FEdGraphUtilities::ExportNodesToText()` 直接从图表对象序列化 T3D
- 保留剪贴板路径作为 UI 层的降级方案

### 8.10 风险：Pin Alias 硬编码，扩展新节点时易遗漏
现状：
- pin alias（`execute`→`PN_Execute`、`then`→`PN_Then` 等）在生成器中硬编码
- 新增节点类型时需要同步修改 alias 表
- AI 生成的 pin 名称可能不在 alias 表中，导致连线静默失败

对策：
- Phase 1 把 pin alias 表外置到配置文件（JSON 或规则文档扩展章节）
- 连线失败时在诊断信息中明确报告"未知 pin 别名"

### 8.11 风险：MCP Server 技术栈未选型
影响：
- Phase 3 工期不可估计

待决项：
- 语言选型：推荐 TypeScript + `@modelcontextprotocol/sdk`（VS Code 生态最成熟）
- 进程管理：谁启动 MCP Server？推荐由 IDE 侧配置文件声明，IDE 自动拉起
- Named Pipe 客户端：需确认选型语言是否有成熟的 Named Pipe 库

对策：
- Phase 2 结束前完成 MCP Server 技术选型文档
- Phase 2 联调时用最小 Python/TS 脚本验证 Named Pipe 通信

### 8.12 风险：缺少自动化回归测试
影响：
- JSON → 蓝图的节点正确性无法回归
- Bridge 协议格式退化无人检测
- 新增节点支持后旧 JSON 是否兼容无从验证

对策：
- Phase 1 结束时建立一组 JSON fixture 文件（覆盖所有已支持节点类型）
- 配合 UE Automation Test (`FAutomationTestBase`) 做回归验证
- Phase 2 结束时补充 Bridge 协议的请求/响应 fixture 验证

---

## 九、前置验证任务（PoC）

以下任务应在正式 Phase 1 编码之前完成，用于消除高风险不确定性。

### 9.1 PoC-1：从 UEdGraph 直接导出 T3D
目标：
- 验证 `FEdGraphUtilities::ExportNodesToText()` 能否替代剪贴板路径
- 确认输出的 T3D 格式与现有 `FBlueprintToTextConverter::ConvertTextToJson()` 兼容

验证方式：
- 在现有插件中写一个临时测试按钮
- 选中若干节点后调用 `ExportNodesToText`
- 将结果传入 `ConvertTextToJson` 验证 JSON 输出

通过标准：
- 函数节点、变量节点、ForLoop 宏节点均能正确导出并转换为 JSON

### 9.2 PoC-2：编译结果结构化获取
目标：
- 确认 `FKismetEditorUtilities::CompileBlueprint()` 后可以从哪些来源聚合诊断信息

验证路径：
- `Blueprint->Status`（`BS_Error` / `BS_UpToDate`）
- `Blueprint->ErrorCount` / `Blueprint->WarningCount`（如适用）
- `FMessageLog("BlueprintLog")` 的日志拦截
- `FCompilerResultsLog` 回调或输出捕获

通过标准：
- 能在代码中获取"编译成功/失败 + 错误数量 + 至少一条错误文本"

### 9.3 PoC-3：显式资产定位与图表切换
目标：
- 验证能否按资产路径打开蓝图并切换到指定图表

验证路径：
- `LoadObject<UBlueprint>(nullptr, AssetPath)`
- `GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint)`
- `FBlueprintEditor::OpenGraphAndBringToFront(TargetGraph)`

通过标准：
- 传入 `/Game/BP/BP_Test.BP_Test` + `EventGraph` 能打开对应图表

### 9.4 PoC-4：Named Pipe UE 侧监听
目标：
- 验证 UE 编辑器进程内能否稳定运行 Named Pipe Server

验证路径：
- `FPlatformNamedPipe`（UE 原生）
- `FRunnable` + `FRunnableThread` 开监听线程
- `AsyncTask(ENamedThreads::GameThread, ...)` 将命令投递到主线程

通过标准：
- 外部 Python 脚本能连接 Named Pipe，发送字符串消息，UE 侧在 GameThread 上打印收到的内容

---

## 十、建议的第一批落地任务

### 10.1 前置 PoC 任务（Phase 0）
1. 验证 `ExportNodesToText` 兼容性（PoC-1）
2. 验证编译结果获取路径（PoC-2）
3. 验证显式资产定位（PoC-3）
4. 验证 Named Pipe 监听（PoC-4）

### 10.2 代码层任务（Phase 1）
1. 抽出 `GraphResolver`（含显式资产定位，降级到焦点图表）
2. 抽出 `ImportService`（包裹 `FScopedTransaction`）
3. 抽出 `ExportService`（新增 `ExportFromGraph` 直接导出路径）
4. 抽出 `ValidationService`
5. 抽出 `CompileService`（基于 PoC-2 结果实现）
6. Pin alias 表外置到配置文件
7. 让 `SHelperMainWidget` 改为调用服务层
8. 建立 JSON fixture 回归测试集

### 10.3 Bridge 层任务（Phase 2）
1. 定义 `BridgeProtocol` DTO
2. 建 `TaskManager`（线程安全）
3. 建 `BridgeRouter`
4. 落一个 Named Pipe Server（含 GameThread 调度）
5. 打通导入、导出、编译三条命令
6. 补充 Bridge 协议请求/响应 fixture 验证

### 10.4 联调任务
1. 写一个最小本地客户端脚本（Python 推荐）
2. 验证任务查询
3. 验证错误结构
4. 验证最近一次诊断缓存
5. MCP Server 技术选型文档产出

---

## 十一、信息缺失项

以下事项在当前文档中尚未覆盖，需要在对应阶段开始前补齐：

| 缺失项 | 影响阶段 | 影响程度 | 建议处理时机 |
|--------|---------|---------|------------|
| `FEdGraphUtilities::ExportNodesToText` 输出格式验证 | Phase 1 | 高 | PoC-1 |
| `FKismetEditorUtilities::CompileBlueprint` 诊断聚合方式 | Phase 1 | 高 | PoC-2 |
| Named Pipe 在 UE 进程内的实现方式（`FPlatformNamedPipe` 可用性） | Phase 2 | 高 | PoC-4 |
| Bridge IO 线程与 GameThread 调度模型 | Phase 2 | 高 | PoC-4 + Phase 2 设计 |
| MCP Server 语言选型与 SDK 选择 | Phase 3 | 中 | Phase 2 联调期间确认 |
| MCP Server 进程管理（谁启动、如何重连） | Phase 3 | 中 | Phase 3 设计 |
| IDE 侧 MCP Client 配置方式（目标 IDE 确认） | Phase 3 | 中 | Phase 2 结束前确认 |

---

## 十二、可行性评级

### 12.1 分维度评价

| 维度 | 评级 | 说明 |
|------|------|------|
| 架构分层 | 合理 | Core / Bridge / MCP 三层职责清晰，方向正确 |
| 改造策略 | 合理 | "抽离而不推翻"代价可控，现有代码体量适中 |
| 协议设计 | 偏理想 | Tool / Resource 定义完整，但部分缺乏 UE 侧实现路径验证 |
| Phase 1 可行性 | 基本可行 | 需补充 GraphResolver 资产定位 + 剪贴板脱离 + 事务包裹 |
| Phase 2 风险 | 中高 | 线程模型 + 编译日志捕获 + Named Pipe UE 实现均需提前验证 |
| Phase 3 风险 | 中 | 技术栈未选型，但 MCP SDK 生态成熟，Phase 2 稳了问题不大 |
| 测试保障 | 缺失 | 无自动化回归手段，需在 Phase 1 建立 |

### 12.2 关键行动项
以下三项应优先处理，否则后续阶段风险不收敛：

1. **Phase 0 PoC 验证**：导出路径兼容性 + 编译日志捕获 + Named Pipe UE 侧可行性
2. **Phase 1 补强**：`FScopedTransaction` 事务包裹 + `ExportFromGraph` 直接导出 + Pin alias 外置
3. **Phase 2 前置**：线程模型设计 + Named Pipe 实现方案确定

---

## 十三、结论
实施顺序上，最重要的是先让 BlueprintHelper 从“编辑器 UI 工具”升级为“可复用的应用服务”，然后再在外层补 Bridge 和 MCP。

如果直接跳到 MCP，会把现有耦合原样暴露到协议层，后续维护成本会更高。反过来，先做 Core 和 Bridge，则 UI、自动化、AI 三个入口都能共享同一套能力，这才是长期可维护的结构。
补充建议：在原计划 Phase 1 之前插入 Phase 0（PoC 验证），用最小代价消除四个高风险不确定性。PoC 全部通过后再进入正式编码阶段，可以显著降低 Phase 1 和 Phase 2 的返工概率。