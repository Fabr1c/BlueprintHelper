# BlueprintHelper MCP 开发前置实施计划（Phase 0 + Phase 1）

> 基于 v2.3 完成后的实际代码状态编写，替代原实施计划中 Phase 0 / Phase 1 的高层描述。

---

## 零、现状评估

### 已有资产（v2.3 完毕）

| 维度 | 状态 | 说明 |
|------|------|------|
| NodeHandler 策略模式 | ✅ 27 种 | 所有常用 K2Node 子类已覆盖 |
| OperationHandler | ✅ 6 种 | 变量/函数/宏/分发器 创建/删除 |
| 图表对象直接导出 | ✅ | `ConvertGraphToJson` + `ExportBlueprintToJson` 已去剪贴板依赖 |
| 多图导入 | ✅ | `GenerateMultiGraphFromJson` + `FindGraphByName` |
| PinAliases 配置化 | ✅ | `Resources/PinAliases.json` |
| 回归 fixture | ✅ | `Resources/TestFixtures/` |

### 缺失项

| 缺失 | 影响 |
|------|------|
| Service 层 | 蓝图操作能力锁死在 UI 回调中，外部调用方不可用 |
| 结构化结果 DTO | 返回值只有 `FBlueprintGenerateResult`（message 文本），不足以做协议序列化 |
| 事务包裹 | `OnGenerateFromTextClicked` 未用 `FScopedTransaction`，失败无法整体 Undo |
| 显式资产定位 | `GetActiveBlueprintGraph()` 只能取焦点图表，不能按路径+图表名定位 |
| 编译服务 | 完全没有编译触发与结果聚合逻辑 |
| JSON 预校验 | 导入前无结构校验，直接走生成器解析 |

---

## 一、Phase 0 — PoC 验证（2 项）

> 原计划 4 项 PoC，PoC-1（导出路径）已随 v2.1 完成，PoC-4（Named Pipe）推迟到 Phase 2。

### PoC-2：编译结果结构化获取

**目标**：确认 `FKismetEditorUtilities::CompileBlueprint()` 后可从哪些来源聚合诊断信息。

**验证路径**：

```cpp
// 1. 触发编译
FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, nullptr);

// 2. 获取状态
bool bHasError = (Blueprint->Status == BS_Error);

// 3. 收集编译消息
FCompilerResultsLog LogResults;
// 或拦截 FMessageLog("BlueprintLog")
```

**验证方式**：
- 在 `SHelperMainWidget` 中加一个临时按钮 `[PoC] 编译当前蓝图`
- 点击后对当前焦点蓝图执行编译
- 在 Output Log 中打印状态 + 错误数 + 至少一条错误文本

**通过标准**：
- 能拿到 `BS_Error / BS_UpToDate` 状态
- 能拿到至少一条编译错误的结构化文本（节点名 + 错误描述）

**实现探查**：
```cpp
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

// 方案 A：直接读 Blueprint 成员
Blueprint->Status;  // EBlueprintStatus

// 方案 B：FCompilerResultsLog（用于 CompileBlueprint 第三参数）
FCompilerResultsLog Results;
FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
// Results 内部有 Messages / NumErrors / NumWarnings

// 方案 C：FMessageLog 拦截
// 较复杂，需要注册 Log Listener，Phase 0 暂不走此路径
```

**推荐方案**：方案 B（`FCompilerResultsLog`），因为它是编译函数的直接输出参数。

---

### PoC-3：显式资产定位与图表切换

**目标**：验证能否按资产路径打开蓝图并切换到指定图表。

**验证路径**：

```cpp
// 1. 加载蓝图资产
UBlueprint* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/BP/BP_Test.BP_Test"));

// 2. 打开编辑器
UAssetEditorSubsystem* AssetEditorSub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
AssetEditorSub->OpenEditorForAsset(BP);

// 3. 获取 BlueprintEditor 实例
IAssetEditorInstance* EditorInst = AssetEditorSub->FindEditorForAsset(BP, false);
FBlueprintEditor* BPEditor = static_cast<FBlueprintEditor*>(EditorInst);

// 4. 查找目标图表
UEdGraph* TargetGraph = TextToBlueprintGenerator::FindGraphByName(BP, TEXT("EventGraph"));

// 5. 切换到目标图表
BPEditor->OpenGraphAndBringToFront(TargetGraph);
```

**验证方式**：
- 在 `SHelperMainWidget` 中加临时按钮 `[PoC] 打开指定蓝图`
- 硬编码一个测试蓝图路径
- 验证编辑器打开并切换到指定图表

**通过标准**：
- 传入 `/Game/BP/BP_Test.BP_Test` + `EventGraph` 能打开对应图表
- 返回的 `UEdGraph*` 非空且可用于后续操作

**额外验证点**：
- 资产不存在时 `LoadObject` 返回 nullptr → 应优雅失败
- 图表名不存在时 `FindGraphByName` 返回 nullptr → 应降级到 EventGraph
- 资产已打开时是否重复打开 → 验证 `FindEditorForAsset` 行为

---

## 二、Phase 1 — Core 头less 化

### 2.1 总览

Phase 1 的核心目标是让 BlueprintHelper 的能力脱离 Slate UI 独立可用，为后续 Bridge/MCP 提供稳定的程序化调用入口。

#### 任务总表

| 序号 | 任务 | 新增/修改 | 依赖 |
|------|------|----------|------|
| T1 | 定义 Service 层公共类型（DTO） | 新增 | 无 |
| T2 | 实现 GraphResolver | 新增 | PoC-3 |
| T3 | 实现 ValidationService | 新增 | T1 |
| T4 | 实现 ExportService | 新增 | T1, T2 |
| T5 | 实现 ImportService（含 FScopedTransaction） | 新增 | T1, T2, T3 |
| T6 | 实现 CompileService | 新增 | T1, T2, PoC-2 |
| T7 | 让 SHelperMainWidget 改用 Service 层 | 修改 | T3~T6 |
| T8 | Module 初始化 Service 实例 | 修改 | T2~T6 |

#### 依赖图

```
PoC-2 ──────────────────────────┐
PoC-3 ──┐                      │
        ▼                      ▼
T1 ──> T2(GraphResolver) ──> T6(CompileService)
│       │                      │
│       ▼                      │
├──> T3(ValidationService)     │
│       │                      │
│       ▼                      │
├──> T4(ExportService)         │
│       │                      │
│       ▼                      │
└──> T5(ImportService) ────────┘
        │                      │
        ▼                      ▼
      T7(Widget 改用 Service)
        │
        ▼
      T8(Module 初始化)
```

---

### 2.2 T1：Service 层公共类型

**新增文件**：`Public/Services/BlueprintHelperServiceTypes.h`

**内容**：

```cpp
#pragma once
#include "CoreMinimal.h"

// ─── 图表定位 ───

/** 目标蓝图与图表的定位描述。 */
struct FBlueprintHelperGraphTarget
{
    /** 蓝图资产路径，例如 "/Game/BP/BP_Test.BP_Test"。为空时使用当前焦点蓝图。 */
    FString BlueprintPath;

    /** 图表名称，例如 "EventGraph"。为空时使用焦点图表或默认 EventGraph。 */
    FString GraphName;
};

// ─── 诊断信息 ───

/** 单条诊断消息的严重度。 */
enum class EBlueprintHelperDiagnosticSeverity : uint8
{
    Info,
    Warning,
    Error
};

/** 单条诊断消息。 */
struct FBlueprintHelperDiagnosticItem
{
    EBlueprintHelperDiagnosticSeverity Severity = EBlueprintHelperDiagnosticSeverity::Info;
    FString Message;
    FString NodeName;       // 关联节点名（可选）
    FString PinName;        // 关联引脚名（可选）
};

/** 诊断消息集合。 */
struct FBlueprintHelperDiagnosticSet
{
    TArray<FBlueprintHelperDiagnosticItem> Items;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;

    bool HasErrors() const { return ErrorCount > 0; }
    void Add(EBlueprintHelperDiagnosticSeverity Severity, const FString& Message, const FString& NodeName = TEXT(""))
    {
        Items.Add({ Severity, Message, NodeName });
        if (Severity == EBlueprintHelperDiagnosticSeverity::Error) ++ErrorCount;
        else if (Severity == EBlueprintHelperDiagnosticSeverity::Warning) ++WarningCount;
    }
};

// ─── 导入 ───

/** 导入请求。 */
struct FBlueprintHelperImportRequest
{
    /** 目标蓝图/图表定位。 */
    FBlueprintHelperGraphTarget Target;

    /** 待导入的 JSON 文本。 */
    FString JsonText;

    /** 是否在导入后自动编译。 */
    bool bAutoCompile = false;
};

/** 导入结果。 */
struct FBlueprintHelperImportResult
{
    bool bSuccess = false;
    int32 GeneratedNodeCount = 0;
    int32 UnresolvedNodeCount = 0;

    /** 已创建节点的名称列表（用于回滚追踪）。 */
    TArray<FString> CreatedNodeNames;

    /** 未解析节点摘要。 */
    TArray<FString> UnresolvedNodeSummaries;

    /** 诊断信息。 */
    FBlueprintHelperDiagnosticSet Diagnostics;

    /** 用于前端展示的摘要文本。 */
    FString GetSummaryText() const;
};

// ─── 导出 ───

/** 导出范围枚举。 */
enum class EBlueprintHelperExportScope : uint8
{
    /** 导出单个图表的节点和连线。 */
    SingleGraph,
    /** 导出完整蓝图（变量 + 函数签名 + 所有图表）。 */
    FullBlueprint
};

/** 导出请求。 */
struct FBlueprintHelperExportRequest
{
    FBlueprintHelperGraphTarget Target;
    EBlueprintHelperExportScope Scope = EBlueprintHelperExportScope::SingleGraph;
};

/** 导出结果。 */
struct FBlueprintHelperExportResult
{
    bool bSuccess = false;
    FString JsonText;
    FBlueprintHelperDiagnosticSet Diagnostics;
};

// ─── 校验 ───

/** 校验结果。 */
struct FBlueprintHelperValidationResult
{
    bool bValid = false;
    FString DetectedVersion;
    FBlueprintHelperDiagnosticSet Diagnostics;
};

// ─── 编译 ───

/** 编译结果。 */
struct FBlueprintHelperCompileResult
{
    bool bSuccess = false;
    EBlueprintStatus BlueprintStatus = BS_Unknown;
    FBlueprintHelperDiagnosticSet Diagnostics;
};
```

**说明**：
- 所有 DTO 都是纯数据 struct，无 UObject 依赖
- `FBlueprintHelperGraphTarget` 是贯穿所有 Service 的定位基元
- `FBlueprintHelperDiagnosticSet` 统一了错误/警告的收集格式
- 后续 Bridge 层序列化时直接操作这些 DTO

---

### 2.3 T2：GraphResolver

**新增文件**：
- `Public/Services/BlueprintHelperGraphResolver.h`
- `Private/Services/BlueprintHelperGraphResolver.cpp`

**职责**：
- 按 `FBlueprintHelperGraphTarget` 定位蓝图与图表
- 统一替代分散在 UI 和 Module 中的"取图表"逻辑

**公共 API**：

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperGraphResolver
{
public:
    /** 解析 Target，返回目标图表。失败时 OutDiag 有错误。 */
    UEdGraph* ResolveGraph(const FBlueprintHelperGraphTarget& Target,
                           FBlueprintHelperDiagnosticSet& OutDiag) const;

    /** 解析 Target，返回目标蓝图。失败时 OutDiag 有错误。 */
    UBlueprint* ResolveBlueprint(const FBlueprintHelperGraphTarget& Target,
                                 FBlueprintHelperDiagnosticSet& OutDiag) const;

    /** 获取当前焦点图表（降级路径）。 */
    UEdGraph* GetFocusedGraph() const;

    /** 获取当前焦点蓝图（降级路径）。 */
    UBlueprint* GetFocusedBlueprint() const;

private:
    /** 按路径加载并打开蓝图编辑器。 */
    UBlueprint* LoadAndOpenBlueprint(const FString& AssetPath,
                                     FBlueprintHelperDiagnosticSet& OutDiag) const;

    /** 在蓝图中查找图表（委托 FindGraphByName）。 */
    UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& GraphName,
                        FBlueprintHelperDiagnosticSet& OutDiag) const;
};
```

**实现要点**：
- `BlueprintPath` 为空 → 走 `GetFocusedBlueprint()` 降级路径（复用现有 `GetActiveBlueprintGraph` 逻辑）
- `BlueprintPath` 非空 → `LoadObject<UBlueprint>` + `OpenEditorForAsset` + `FindEditorForAsset`
- `GraphName` 为空 → 使用焦点图表或 EventGraph
- `GraphName` 非空 → 委托 `TextToBlueprintGenerator::FindGraphByName`

**对现有代码的影响**：
- `FBlueprintHelperModule::GetActiveBlueprintGraph()` 保留但标记为 deprecated，内部改用 GraphResolver

---

### 2.4 T3：ValidationService

**新增文件**：
- `Public/Services/BlueprintHelperValidationService.h`
- `Private/Services/BlueprintHelperValidationService.cpp`

**职责**：
- 在导入前进行 JSON 结构校验
- 检查 version 字段、schema 字段、必要字段存在性
- 检查节点 ID 唯一性、连线引用完整性

**公共 API**：

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperValidationService
{
public:
    /** 校验 JSON 文本的结构完整性。 */
    FBlueprintHelperValidationResult Validate(const FString& JsonText) const;

private:
    bool ValidateJsonParseable(const FString& JsonText,
                               TSharedPtr<FJsonObject>& OutRoot,
                               FBlueprintHelperDiagnosticSet& OutDiag) const;
    bool ValidateVersion(const TSharedPtr<FJsonObject>& Root,
                         FString& OutVersion,
                         FBlueprintHelperDiagnosticSet& OutDiag) const;
    bool ValidateNodeIds(const TSharedPtr<FJsonObject>& Root,
                         FBlueprintHelperDiagnosticSet& OutDiag) const;
    bool ValidateLinkReferences(const TSharedPtr<FJsonObject>& Root,
                                FBlueprintHelperDiagnosticSet& OutDiag) const;
};
```

**校验规则**：

| 检查项 | 严重度 | 说明 |
|--------|--------|------|
| JSON 解析失败 | Error | 不是合法 JSON |
| 缺少 version 字段 | Warning | 按 1.0 处理 |
| version > 当前支持版本 | Warning | 可能有不支持的字段 |
| nodes 数组缺失 | Error | v1.x 必须有 nodes |
| 节点 ID 重复 | Error | 会导致连线 ambiguous |
| link 引用不存在的节点 ID | Error | 连线无法落地 |
| link 引用不存在的 pin 名 | Warning | 可能是 alias 问题，不阻断 |

---

### 2.5 T4：ExportService

**新增文件**：
- `Public/Services/BlueprintHelperExportService.h`
- `Private/Services/BlueprintHelperExportService.cpp`

**职责**：
- 封装"定位图表 → 导出 JSON"流程
- 支持单图导出与完整蓝图导出

**公共 API**：

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperExportService
{
public:
    explicit FBlueprintHelperExportService(const FBlueprintHelperGraphResolver& InResolver);

    /** 导出蓝图/图表为 JSON。 */
    FBlueprintHelperExportResult Export(const FBlueprintHelperExportRequest& Request) const;

private:
    const FBlueprintHelperGraphResolver& Resolver;
};
```

**实现要点**：
- `SingleGraph` → `FBlueprintToTextConverter::ConvertGraphToJson(Graph)`
- `FullBlueprint` → `FBlueprintToTextConverter::ExportBlueprintToJson(Blueprint)`
- 失败路径统一写入 `FBlueprintHelperDiagnosticSet`

---

### 2.6 T5：ImportService

**新增文件**：
- `Public/Services/BlueprintHelperImportService.h`
- `Private/Services/BlueprintHelperImportService.cpp`

**职责**：
- 封装"校验 → 定位图表 → 事务开始 → 生成节点 → 事务结束"完整流程
- 失败时整体 Undo

**公共 API**：

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperImportService
{
public:
    FBlueprintHelperImportService(const FBlueprintHelperGraphResolver& InResolver,
                                  const FBlueprintHelperValidationService& InValidator);

    /** 将 JSON 导入到目标蓝图图表。 */
    FBlueprintHelperImportResult Import(const FBlueprintHelperImportRequest& Request) const;

private:
    const FBlueprintHelperGraphResolver& Resolver;
    const FBlueprintHelperValidationService& Validator;
};
```

**实现要点**：

```cpp
FBlueprintHelperImportResult FBlueprintHelperImportService::Import(
    const FBlueprintHelperImportRequest& Request) const
{
    FBlueprintHelperImportResult Result;

    // 1. 校验
    FBlueprintHelperValidationResult ValResult = Validator.Validate(Request.JsonText);
    if (!ValResult.bValid)
    {
        Result.Diagnostics = MoveTemp(ValResult.Diagnostics);
        return Result;
    }

    // 2. 定位
    FBlueprintHelperDiagnosticSet DiagSet;
    UBlueprint* Blueprint = Resolver.ResolveBlueprint(Request.Target, DiagSet);
    if (!Blueprint) { /* ... */ return Result; }

    // 3. 事务包裹
    FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Import JSON")));

    // 4. 生成（根据 JSON version 选择单图/多图）
    TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
    FBlueprintGenerateResult GenResult;

    if (/* has graphs array or blueprint_operations */)
    {
        GenResult = TextToBlueprintGenerator::GenerateMultiGraphFromJson(
            Blueprint, Request.JsonText, Unresolved);
    }
    else
    {
        UEdGraph* Graph = Resolver.ResolveGraph(Request.Target, DiagSet);
        GenResult = TextToBlueprintGenerator::GenerateBlueprintFromJson(
            Graph, Request.JsonText, Unresolved);
    }

    // 5. 转换结果
    Result.bSuccess = GenResult.bSucceed;
    Result.GeneratedNodeCount = GenResult.GeneratedNodeCount;
    Result.UnresolvedNodeCount = GenResult.UnresolvedNodeCount;
    for (auto& Item : Unresolved)
    {
        Result.UnresolvedNodeSummaries.Add(
            FString::Printf(TEXT("%s: %s"), *Item->NodeData.FunctionName, *Item->Reason));
    }

    // 6. 可选自动编译
    if (Request.bAutoCompile && Result.bSuccess) { /* 委托 CompileService */ }

    return Result;
}
```

**关键改进**：
- `FScopedTransaction` 包裹全部生成操作 → 失败可 Ctrl+Z 整体 Undo
- 返回 `UnresolvedNodeSummaries` → Bridge 层可序列化返回给 AI
- 返回标准 `FBlueprintHelperDiagnosticSet` → 统一错误格式

---

### 2.7 T6：CompileService

**新增文件**：
- `Public/Services/BlueprintHelperCompileService.h`
- `Private/Services/BlueprintHelperCompileService.cpp`

**职责**：
- 触发蓝图编译
- 聚合编译结果为结构化 DTO

**公共 API**：

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperCompileService
{
public:
    explicit FBlueprintHelperCompileService(const FBlueprintHelperGraphResolver& InResolver);

    /** 编译指定蓝图，返回结构化结果。 */
    FBlueprintHelperCompileResult Compile(const FBlueprintHelperGraphTarget& Target) const;

    /** 获取蓝图当前编译状态（不触发编译）。 */
    FBlueprintHelperCompileResult GetStatus(const FBlueprintHelperGraphTarget& Target) const;

private:
    const FBlueprintHelperGraphResolver& Resolver;
};
```

**实现要点**（基于 PoC-2 验证结果）：

```cpp
FBlueprintHelperCompileResult FBlueprintHelperCompileService::Compile(
    const FBlueprintHelperGraphTarget& Target) const
{
    FBlueprintHelperCompileResult Result;

    FBlueprintHelperDiagnosticSet DiagSet;
    UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, DiagSet);
    if (!Blueprint) { Result.Diagnostics = MoveTemp(DiagSet); return Result; }

    // 编译
    FCompilerResultsLog LogResults;
    FKismetEditorUtilities::CompileBlueprint(Blueprint,
        EBlueprintCompileOptions::None, &LogResults);

    // 收集结果
    Result.BlueprintStatus = Blueprint->Status;
    Result.bSuccess = (Blueprint->Status != BS_Error);

    // 从 LogResults 提取消息
    for (const TSharedRef<FTokenizedMessage>& Msg : LogResults.Messages)
    {
        EBlueprintHelperDiagnosticSeverity Sev;
        switch (Msg->GetSeverity())
        {
        case EMessageSeverity::Error:   Sev = EBlueprintHelperDiagnosticSeverity::Error; break;
        case EMessageSeverity::Warning: Sev = EBlueprintHelperDiagnosticSeverity::Warning; break;
        default:                        Sev = EBlueprintHelperDiagnosticSeverity::Info; break;
        }
        Result.Diagnostics.Add(Sev, Msg->ToText().ToString());
    }

    return Result;
}
```

**依赖 PoC-2 确认**：
- `FCompilerResultsLog` 是否为 `FKismetEditorUtilities::CompileBlueprint` 的合法参数
- `LogResults.Messages` 的实际类型和内容
- 编译后 `Blueprint->Status` 的可靠性

---

### 2.8 T7：SHelperMainWidget 改用 Service 层

**修改文件**：
- `Private/SHelperMainWidget.cpp`
- `Public/SHelperMainWidget.h`

**改动范围**：

| 方法 | 当前实现 | 改为 |
|------|---------|------|
| `OnParseClipboardClicked` | 直接调 `FBlueprintToTextConverter` | 可选地委托 ExportService（保留 T3D 文本输入路径不变） |
| `OnGenerateFromTextClicked` | 直接调 `TextToBlueprintGenerator::GenerateBlueprintFromJson` | 调 `ImportService::Import()` |
| `GetCurrentTargetGraph` | 调 `FBlueprintHelperModule::Get().GetActiveBlueprintGraph()` | 调 `GraphResolver::GetFocusedGraph()` |

**Widget 持有 Service 引用**：

```cpp
// SHelperMainWidget.h 新增
private:
    /** Service 引用（由 Module 传入）。 */
    const FBlueprintHelperImportService* ImportService = nullptr;
    const FBlueprintHelperExportService* ExportService = nullptr;
    const FBlueprintHelperGraphResolver* GraphResolver = nullptr;
```

**构造传参**：

```cpp
// SLATE_ARGS 新增
SLATE_ARGUMENT(const FBlueprintHelperImportService*, ImportService)
SLATE_ARGUMENT(const FBlueprintHelperExportService*, ExportService)
SLATE_ARGUMENT(const FBlueprintHelperGraphResolver*, GraphResolver)
```

**改动最小化**：
- `OnParseClipboardClicked` 的 T3D→JSON 路径暂不改（这是 UI 手工路径，不影响 headless）
- 只改 `OnGenerateFromTextClicked` 走 ImportService
- `OnCopyJsonRuleClicked` 不变

---

### 2.9 T8：Module 初始化 Service 实例

**修改文件**：
- `Public/BlueprintHelper.h`
- `Private/BlueprintHelper.cpp`

**改动**：

```cpp
// BlueprintHelper.h 新增
class FBlueprintHelperGraphResolver;
class FBlueprintHelperValidationService;
class FBlueprintHelperExportService;
class FBlueprintHelperImportService;
class FBlueprintHelperCompileService;

class BLUEPRINTHELPER_API FBlueprintHelperModule : public IModuleInterface
{
public:
    // ... 现有接口不变 ...

    /** 获取 GraphResolver。 */
    const FBlueprintHelperGraphResolver& GetGraphResolver() const;

    /** 获取 ExportService。 */
    const FBlueprintHelperExportService& GetExportService() const;

    /** 获取 ImportService。 */
    const FBlueprintHelperImportService& GetImportService() const;

    /** 获取 CompileService。 */
    const FBlueprintHelperCompileService& GetCompileService() const;

    /** 获取 ValidationService。 */
    const FBlueprintHelperValidationService& GetValidationService() const;

private:
    TUniquePtr<FBlueprintHelperGraphResolver> GraphResolver;
    TUniquePtr<FBlueprintHelperValidationService> ValidationService;
    TUniquePtr<FBlueprintHelperExportService> ExportService;
    TUniquePtr<FBlueprintHelperImportService> ImportService;
    TUniquePtr<FBlueprintHelperCompileService> CompileService;
};
```

**StartupModule 中初始化**：

```cpp
void FBlueprintHelperModule::StartupModule()
{
    // ... 现有 Handler 注册 ...

    // Service 层初始化
    GraphResolver = MakeUnique<FBlueprintHelperGraphResolver>();
    ValidationService = MakeUnique<FBlueprintHelperValidationService>();
    ExportService = MakeUnique<FBlueprintHelperExportService>(*GraphResolver);
    ImportService = MakeUnique<FBlueprintHelperImportService>(*GraphResolver, *ValidationService);
    CompileService = MakeUnique<FBlueprintHelperCompileService>(*GraphResolver);

    // ... 现有 Tab/Menu 注册 ...
}
```

**ShutdownModule 中清理**：

```cpp
void FBlueprintHelperModule::ShutdownModule()
{
    CompileService.Reset();
    ImportService.Reset();
    ExportService.Reset();
    ValidationService.Reset();
    GraphResolver.Reset();

    // ... 现有清理 ...
}
```

---

## 三、新增文件清单

| 序号 | 路径 | 说明 |
|------|------|------|
| 1 | `Public/Services/BlueprintHelperServiceTypes.h` | 所有 Service 层 DTO |
| 2 | `Public/Services/BlueprintHelperGraphResolver.h` | 图表定位器 |
| 3 | `Private/Services/BlueprintHelperGraphResolver.cpp` | 实现 |
| 4 | `Public/Services/BlueprintHelperValidationService.h` | JSON 校验 |
| 5 | `Private/Services/BlueprintHelperValidationService.cpp` | 实现 |
| 6 | `Public/Services/BlueprintHelperExportService.h` | 导出服务 |
| 7 | `Private/Services/BlueprintHelperExportService.cpp` | 实现 |
| 8 | `Public/Services/BlueprintHelperImportService.h` | 导入服务 |
| 9 | `Private/Services/BlueprintHelperImportService.cpp` | 实现 |
| 10 | `Public/Services/BlueprintHelperCompileService.h` | 编译服务 |
| 11 | `Private/Services/BlueprintHelperCompileService.cpp` | 实现 |

## 四、修改文件清单

| 序号 | 路径 | 改动 |
|------|------|------|
| 1 | `Public/BlueprintHelper.h` | 新增 5 个 Service Getter + 5 个 TUniquePtr 成员 |
| 2 | `Private/BlueprintHelper.cpp` | StartupModule 初始化 Service，ShutdownModule 清理 |
| 3 | `Public/SHelperMainWidget.h` | SLATE_ARGS 新增 Service 指针，新增 Service 成员 |
| 4 | `Private/SHelperMainWidget.cpp` | `OnGenerateFromTextClicked` 改用 ImportService |

---

## 五、实施顺序建议

### 第一批：PoC + DTO（可并行）

```
[PoC-2] 编译结果验证 ──────────────┐
[PoC-3] 资产定位验证 ──────────────┤
[T1]    ServiceTypes.h 定义 ───────┤
                                    ▼
                             PoC 结论确认
```

**估计范围**：PoC 各约 50 行临时代码，T1 约 120 行。

### 第二批：核心 Service

```
[T2] GraphResolver ──────┐
                          ├──> [T3] ValidationService
                          ├──> [T4] ExportService
                          └──> [T5] ImportService（含 FScopedTransaction）
```

**估计范围**：T2 约 150 行，T3 约 100 行，T4 约 60 行，T5 约 120 行。

### 第三批：CompileService + 集成

```
[T6] CompileService（依赖 PoC-2 结论）
[T7] SHelperMainWidget 改用 Service
[T8] Module 初始化
```

**估计范围**：T6 约 100 行，T7 约 40 行改动，T8 约 30 行改动。

---

## 六、验收标准

### Phase 0 验收
- [ ] PoC-2：能从代码获取编译状态 + 至少一条错误文本
- [ ] PoC-3：能按资产路径打开蓝图 + 切换到指定图表

### Phase 1 验收
- [ ] `FBlueprintHelperModule::Get().GetImportService().Import(Request)` 可脱离 UI 调用
- [ ] `FBlueprintHelperModule::Get().GetExportService().Export(Request)` 可脱离 UI 调用
- [ ] `FBlueprintHelperModule::Get().GetCompileService().Compile(Target)` 可脱离 UI 调用
- [ ] 导入失败时 Ctrl+Z 能整体回滚（FScopedTransaction 生效）
- [ ] 导入前校验能检测到 JSON 格式错误和节点 ID 重复
- [ ] UI 功能行为不回退：导出按钮、导入按钮、规则复制均正常
- [ ] 所有返回结果走标准 DTO，不再是纯文本 message

### Bridge Ready 验收（Phase 1 → Phase 2 交接条件）
- [ ] 所有 Service 可通过 `FBlueprintHelperModule::Get()` 访问
- [ ] 所有结果 DTO 可被 `FJsonObjectConverter` 序列化
- [ ] 无 Slate / UI 依赖泄露到 Service 层

---

## 七、与后续 Phase 的衔接

Phase 1 完成后，Phase 2 (Bridge) 的工作变为：

```
BridgeServer（Named Pipe 监听）
    ↓ 收到请求 JSON
BridgeRouter（路由命令名到 Service 方法调用）
    ↓ AsyncTask(GameThread) 投递
Service 层执行
    ↓ 写入 TaskManager
BridgeServer（回传结果 JSON）
```

Bridge 层不需要了解蓝图 API，只需要：
1. 接收 JSON
2. 反序列化为 `FBlueprintHelperImportRequest` / `FBlueprintHelperExportRequest` 等
3. 调用对应 Service
4. 序列化返回 DTO

这正是 Phase 1 的核心价值：**让 Bridge 成为纯粹的传输适配器，而非业务逻辑承载者**。
