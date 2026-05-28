# BlueprintHelper 插件 UE 5.3 版本适配实施计划

## 背景
当前项目基于 UE 5.6 开发，需要适配到 UE 5.3 编译器。项目已有版本兼容层：
- `BlueprintHelperVersionCompat.h` — 通用 API 兼容包装（EAllowShrinking, FMetaData, ImportText 等）
- `BlueprintHelperWidgetVersionCompat.h` — Widget 变量注册 API 兼容
- `BlueprintHelperUnrealEd53CommentNodeCompat.cpp` — UE 5.3 UEdGraphNode_Comment 缺失成员回填

## 适配策略
遵循项目现有模式：
1. 用 `ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION` 检测版本
2. 用 `#if` 宏条件编译区分 5.3 / 5.6 路径
3. 在 `BlueprintHelperVersionCompat.h` 中添加新的兼容宏和包装函数
4. 对于 UE 5.3 缺失的类型/API，在 `#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6` 中提供回填

## 任务分解
各任务独立且可并行，但依赖 Task 1（获取编译错误）完成后再分派。

### Task 1: 运行 UE 5.3 编译获取完整错误列表
运行 `G:\UE_5.3\Engine\Build\BatchFiles\Build.bat` 编译项目，收集所有编译错误，按文件分组分类。

### Task 2: 扩展 BlueprintHelperVersionCompat.h 兼容层
根据错误列表，在 VersionCompat.h 中添加缺失的 API 兼容宏和包装函数。

### Task 3: 修复源码中的 API 不兼容调用
将所有使用 5.6-only API 的调用点替换为 VersionCompat 包装函数，或添加 `#if` 条件编译。

### Task 4: 处理缺失模块/类型回填
对于 UE 5.3 中不存在的模块（如 SubobjectEditor）、类型或头文件，提供条件排除或回填实现。

### Task 5: 编译验证
反复编译→修复→编译直到 UE 5.3 下零错误。
