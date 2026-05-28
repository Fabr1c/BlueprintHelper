# Namespace 重构实施计划

## 背景
UE Unity Build 将多个 .cpp 合并为一个翻译单元，导致匿名 namespace 中的同名 static 函数冲突。需要将所有匿名 namespace 函数提取到命名工具类中。

## 编译错误清单

| # | 冲突函数 | 文件 A | 文件 B |
|---|---------|--------|--------|
| 1 | `NormalizeOperation` | StructFieldFragmentBuilder.cpp:8 | MacroControlFragmentBuilder.cpp:5 |
| 2 | `AddOwnershipTagIfPresent` | FieldFragmentBuilder.cpp:15 | GraphStatementBuilder.cpp:128 |
| 3 | `ContextEvidenceValue` | GraphStatementBuilder.cpp:98 | GraphSemanticIR.cpp:271 |
| 4 | `StableHashString` | ActionContextBundleProjector.cpp:20 | SingletonControlFlowEvidenceProvider.cpp:82 |
| 5 | `TryProjectScheduleEvidence` | GenericTransformScheduleActionResolverTests.cpp:109 | GenericScheduleFragmentTests.cpp:55 |
| 6 | `TryProjectScheduleEvidenceFromQueries` | GenericTransformScheduleActionResolverTests.cpp:157 | GenericScheduleFragmentTests.cpp:103 |

## 任务分解

### Task 1: 创建 FBlueprintHelperGraphTextUtils 工具类
- 创建 `Utils/BlueprintHelperGraphTextUtils.h`
- 创建 `Utils/BlueprintHelperGraphTextUtils.cpp`
- 包含: NormalizeOperation, NormalizeFieldToken, NormalizeDelegateOperation, NormalizeScheduleOperationToken, NormalizeSingletonControlQuery, NormalizeOpOperationToken, FirstNonEmptyString(x2), ContextEvidenceValue, StableHashString

### Task 2: 替换 StructFieldFragmentBuilder 和 MacroControlFragmentBuilder 中的 NormalizeOperation
- 移除匿名 namespace 中的 NormalizeOperation
- 替换为 FBlueprintHelperGraphTextUtils::NormalizeOperation 调用

### Task 3: 替换 FieldFragmentBuilder 和 GraphStatementBuilder 中的 AddOwnershipTagIfPresent
- 统一两个版本（FieldFragmentBuilder 检查 Key+Value, GraphStatementBuilder 只检查 Value）
- 移除重复定义，统一使用工具类版本
- 如果工具类不存在则创建 FBlueprintHelperFragmentTagUtils

### Task 4: 替换 GraphStatementBuilder 和 GraphSemanticIR 中的 ContextEvidenceValue
- 移除重复定义
- 替换为 FBlueprintHelperGraphTextUtils::ContextEvidenceValue

### Task 5: 替换 ActionContextBundleProjector 和 SingletonControlFlowEvidenceProvider 中的 StableHashString
- 移除重复定义
- 替换为 FBlueprintHelperGraphTextUtils::StableHashString

### Task 6: 修复测试文件中的 TryProjectScheduleEvidence 和 TryProjectScheduleEvidenceFromQueries
- 提取共享测试辅助函数到测试工具文件
- 或重命名其中一组以避免冲突

### Task 7: 编译验证
- 运行 UBT 编译直到零错误
