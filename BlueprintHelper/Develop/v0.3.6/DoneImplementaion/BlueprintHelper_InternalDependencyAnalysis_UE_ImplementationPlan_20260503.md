# BlueprintHelper Internal Dependency Analysis UE 实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
本文边界：确认 Dependency / Referencer / External Dependent Analysis 不导出为独立 Agent-facing MCP 工具簇，而作为 UE 插件内部辅助分析器实现，并服务 Cleanup / Replace / Remove / high-risk dry_run 等调用方工具。

---

## 0. 本次同步结论

```text
1. 不导出独立 Agent-facing MCP 工具簇。
2. UE 侧实现内部 Dependency Analysis Helpers。
3. 内部 helper 服务 Cleanup / Replace / Remove / high-risk dry_run。
4. 内部 helper 不生成 ToolResultBase。
5. 内部 helper 不生成 transaction_id。
6. 内部 helper 不直接写 Journal / Review。
7. 调用方写工具可把 helper 结果摘要写入自己的 Journal / Review。
8. 必须区分 external_dependencies 与 external_dependents。
9. external_dependents 表示外部依赖目标，删除/替换目标可能破坏外部资产。
10. external_dependencies 表示目标依赖外部，删除目标通常不破坏外部资产。
11. partial=true 必须支持，不能假精确。
12. unsupported_checks 用于记录未覆盖的分析类型。
13. Agent-facing dry_run 成功仍保持极简。
14. Agent-facing dry_run blocked / failed 时只返回必要 conflict 摘要。
15. 完整依赖/引用详情只进入内部数据、Journal、Review 或 Debug Export，不默认返回 Agent。
```

---

## 1. 定位

本簇不是独立 MCP 工具簇。

不提供以下 Agent-facing tools：

```text
read_asset_dependencies
read_asset_referencers
analyze_external_dependents
```

UE 侧应实现为内部辅助分析能力：

```text
UE Internal Dependency Analysis Helpers
```

这些 helper 被现有高风险工具调用：

```text
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
ReplaceBlueprintGraph
MergeBlueprintGraph
remove_widget_from_tree
remove_data_table_row / rows
remove_implemented_interfaces
function_definition / event_definition replace
```

---

## 2. 内部模块建议

可拆成多个 analyzer：

```text
FBlueprintHelperAssetDependencyAnalyzer
FBlueprintHelperBlueprintReferenceAnalyzer
FBlueprintHelperExternalDependentAnalyzer
```

也可以统一为一个 service：

```cpp
class FBlueprintHelperDependencyAnalysisService
{
public:
    FBlueprintHelperAssetDependencySummary AnalyzeDependencies(
        const FBlueprintHelperDependencyAnalysisTarget& Target,
        const FBlueprintHelperDependencyAnalysisOptions& Options);

    FBlueprintHelperAssetReferencerSummary AnalyzeReferencers(
        const FBlueprintHelperDependencyAnalysisTarget& Target,
        const FBlueprintHelperDependencyAnalysisOptions& Options);

    FBlueprintHelperExternalDependentSummary AnalyzeExternalDependents(
        const FBlueprintHelperDependencyAnalysisTarget& Target,
        const FBlueprintHelperDependencyAnalysisOptions& Options);

    FBlueprintHelperReferenceRiskSummary AnalyzeReferenceRisk(
        const FBlueprintHelperDependencyAnalysisTarget& Target,
        const FBlueprintHelperDependencyAnalysisOptions& Options);
};
```

---

## 3. 内部目标描述结构

建议统一目标描述，供 Cleanup / Replace / Remove 等调用方复用。

```cpp
struct FBlueprintHelperDependencyAnalysisTarget
{
    FString AssetPath;

    // asset | function | event | custom_event | block | widget | data_table_row | interface
    FString TargetType;

    FString TargetName;
    FString BlockId;
    FString GraphName;
    FString RowName;
    FString WidgetName;
    FString InterfacePath;
};
```

规则：

```text
1. AssetPath 使用完整资产路径。
2. TargetType 必须明确，不做模糊推断。
3. block 目标优先使用 BlockId。
4. function / event / custom_event 目标使用 TargetName + GraphName 或目标类型。
5. widget / data_table_row 目标使用 WidgetName / RowName。
```

---

## 4. 内部 Options

```cpp
struct FBlueprintHelperDependencyAnalysisOptions
{
    bool bIncludeHardReferences = true;
    bool bIncludeSoftReferences = true;
    bool bAnalyzeBlueprintCalls = true;
    bool bAnalyzeWidgetBindings = true;
    bool bAnalyzeDataTableRows = true;

    // First version may be false or unsupported.
    bool bScanCppSource = false;
    bool bAnalyzeRuntimeStringLookup = false;
    bool bAnalyzeDynamicSoftReferences = false;

    int32 MaxResultCount = 100;
};
```

原则：

```text
1. 不支持的检查不得伪装为已覆盖。
2. 未覆盖能力必须进入 UnsupportedChecks。
3. 超过 MaxResultCount 时可设置 partial=true。
```

---

## 5. Asset Dependencies

### 5.1 方向

```text
target -> external assets
```

含义：

```text
external_dependencies = 目标依赖外部。
删除目标通常不会破坏外部资产本身。
```

### 5.2 内部结构

```cpp
struct FBlueprintHelperAssetDependencySummary
{
    int32 DependencyCount = 0;
    TArray<FBlueprintHelperAssetRefSummary> Dependencies;
    bool bPartial = false;
    TArray<FString> UnsupportedChecks;
};
```

---

## 6. Asset Referencers

### 6.1 方向

```text
external assets -> target
```

含义：

```text
external_dependents = 外部依赖目标。
删除或破坏目标可能影响外部资产。
```

### 6.2 内部结构

```cpp
struct FBlueprintHelperAssetReferencerSummary
{
    int32 ReferencerCount = 0;
    TArray<FBlueprintHelperAssetRefSummary> Referencers;
    bool bPartial = false;
    TArray<FString> UnsupportedChecks;
};
```

---

## 7. Logical External Dependents

用于分析更细粒度的逻辑目标：

```text
function
event
custom_event
block_id
interface implementation
widget_name
data_table_row
```

内部结构：

```cpp
struct FBlueprintHelperExternalDependentSummary
{
    bool bHasExternalDependents = false;
    int32 ExternalDependentCount = 0;
    TArray<FBlueprintHelperDependentRefSummary> Dependents;
    bool bPartial = false;
    TArray<FString> UnsupportedChecks;
};
```

---

## 8. 内部 ref 摘要结构

```cpp
struct FBlueprintHelperAssetRefSummary
{
    FString AssetPath;
    FString AssetType;
};

struct FBlueprintHelperDependentRefSummary
{
    FString AssetPath;
    FString DependentType;
};
```

`DependentType` 建议枚举：

```text
asset_reference
blueprint_call
interface_call
widget_binding
data_table_row_reference
soft_reference
unknown
```

---

## 9. partial / unsupported_checks

UE 侧必须承认引用分析边界。

以下场景不能假精确：

```text
C++ 硬编码路径
运行时字符串拼接 DataTable row name
反射式函数调用
软引用运行时加载
动态 Widget 查找
外部插件自定义引用系统
```

内部返回示例：

```cpp
Result.bPartial = true;
Result.UnsupportedChecks = {
    TEXT("cpp_source_reference_scan"),
    TEXT("runtime_string_lookup"),
    TEXT("dynamic_soft_reference")
};
```

规则：

```text
1. partial=true 表示检查已执行，但覆盖不完整。
2. unsupported_checks 必须列出未覆盖的检查类别。
3. 调用方不得把 partial=true 当成绝对安全。
4. 高风险删除 / Replace 遇到 partial=true 时，应由调用方按 Safety Profile 决定 warning / blocked / stop_and_report。
```

---

## 10. 与 Agent-facing dry_run 输出的关系

内部 helper 可以返回详细结果，但 Agent-facing dry_run 要继续极简。

### 10.1 dry_run passed

不返回完整依赖列表：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

### 10.2 有 external_dependents

返回阻断摘要：

```json
{
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [
      "external_dependents_exist"
    ],
    "conflicts": [
      {
        "code": "external_dependents_exist",
        "external_dependent_count": 2,
        "message": "External dependents exist for the requested target."
      }
    ]
  }
}
```

默认不返回完整 `dependents[]`。

### 10.3 partial analysis

```json
{
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [
      "external_dependent_analysis_partial"
    ],
    "conflicts": [
      {
        "code": "external_dependent_analysis_partial",
        "message": "External dependent analysis is partial; unsupported checks exist."
      }
    ]
  }
}
```

### 10.4 只有 external_dependencies

通常不阻断。

普通成功 dry_run 不返回 external_dependencies；只有当它会影响执行策略时，调用方才可返回 warning 摘要。

---

## 11. 与 Cleanup 的关系

Cleanup 内部调用：

```text
AnalyzeExternalDependents(target block / feature)
AnalyzeDependencies(target block / feature)
```

结果用于：

```text
external_dependents 存在：
- Conservative 默认 blocked / requires_confirmation。
- stop_and_report 或用户确认。

external_dependencies 存在：
- 删除目标时保留外部依赖。
- 不默认阻断。
```

Cleanup dry_run 不需要返回完整列表，只返回：

```text
blocked_by
conflicts
can_execute
```

---

## 12. 与 Replace 的关系

Replace 内部调用：

```text
AnalyzeExternalDependents(function_definition / event_definition)
```

规则：

```text
function_definition / event_definition 有 external_dependents：
默认阻止并报告。

function_body / event_body：
只要入口、签名、外部调用身份稳定，不因 external_dependents 直接阻止，但 dry_run 可报告影响。
```

---

## 13. 与 UMG 删除的关系

`remove_widget_from_tree` 内部分析：

```text
widget bindings
graph references
named widget access
animation tracks
```

如果发现引用：

```text
widget_has_bindings_or_references
```

Agent-facing dry_run 只返回 blocked conflict 摘要，不返回完整扫描详情。

---

## 14. 与 DataTable Row 删除的关系

`remove_data_table_row / rows` 内部分析：

```text
静态 row reference
DataTable row handle
已知 Blueprint 引用
```

如果无法分析运行时字符串：

```cpp
Result.bPartial = true;
Result.UnsupportedChecks = { TEXT("runtime_string_lookup") };
```

Agent-facing dry_run 只返回：

```text
blocked / conflict / partial warning
```

不返回完整扫描详情。

---

## 15. Journal / Review 边界

这些 helper 本身不写 Journal。

规则：

```text
helper 自身只读，不生成 transaction_id。
helper 结果作为调用方工具的 dry_run plan / write plan / Journal 内部数据。
正式写工具执行后，由调用方工具生成 transaction_id 并记录必要依赖分析摘要。
```

也就是：

```text
Dependency Analysis Helper 不直接进入 Review。
Cleanup / Replace / Remove 的 transaction 可以记录其分析摘要。
```

---

## 16. Debug Export 边界

完整依赖/引用详情不默认返回 Agent。

如需完整详情，应通过：

```text
Debug Export
Transaction Debug Bundle
Review UI
Journal 内部数据
```

暴露，而不是通过普通 dry_run 返回。

---

## 17. 验收标准

```text
1. 不存在独立 Agent-facing dependency analysis MCP tools。
2. UE 侧存在内部 Dependency Analysis Helpers。
3. Helper 不返回 ToolResultBase。
4. Helper 不生成 transaction_id。
5. Helper 不直接写 Journal / Review。
6. Helper 能区分 external_dependencies 与 external_dependents。
7. Helper 支持 partial / unsupported_checks。
8. 调用方 dry_run 成功仍极简。
9. 调用方 dry_run blocked / failed 只返回必要 conflict 摘要。
10. 完整依赖详情只进入内部数据、Journal、Review 或 Debug Export。
