# BlueprintHelper 物理目录迁移设计 v1

## 目的

按 `BlueprintHelper_Source_Architecture_Layers_v1_20260508.md` 定义的逻辑分层，将 272 个 C++ 文件从当前物理目录结构分批迁移到目标结构。

## 总览

| 阶段 | 层级 | 文件数 | 核心操作 |
|---|---|---|---|
| 1 | Shared DTO | ~36 | `Structure/` → `Shared/` |
| 2 | Transaction Facts | ~4 | `Transactions/Transactions/` → `Systems/Transactions/` |
| 3 | Tool Clusters | ~50 | `Services/*` → `Systems/ToolClusters/*` |
| 4 | Debug Diagnostics | ~16 | `Services/RuntimeDiagnostics/` → `Systems/Debug/` |
| 5 | Review Audit | ~16 | `Services/Review/` → `Systems/Review/`；`Widgets/Review/` → `UI/Review/` |
| 6 | TaskRuntime | ~40 | `TaskRuntime/` → `Runtime/TaskRuntime/` |
| 7 | Entry / Bridge | ~15 | `Bridge/` → `Entry/`；移动根文件 |
| 8 | 收尾合并 | ~95 | `GraphSupport/`、`Logic/`、`NodeHandlers/`、`OperationHandlers/`、`Safety/`、`Widgets/` 归入对应簇 |

每阶段操作 = git mv + 全局 include 替换 + 编译验证 + git commit。

## 阶段详细映射

### Phase 1 — Shared DTO

```
Private/Structure/  →  Private/Shared/
Public/Structure/   →  Public/Shared/
```

| 原子目录 | 目标子目录 |
|---|---|
| `Structure/AssetFactory/` | `Shared/AssetFactory/` |
| `Structure/BlueprintClassSettings/` | `Shared/BlueprintClassSettings/` |
| `Structure/BlueprintSignature/` | `Shared/BlueprintSignature/` |
| `Structure/BlueprintVariables/` | `Shared/BlueprintVariables/` |
| `Structure/CleanupOwnership/` | `Shared/CleanupOwnership/` |
| `Structure/DataAssetObjectProperty/` | `Shared/ObjectProperty/` |
| `Structure/DataTable/` | `Shared/DataTable/` |
| `Structure/GraphWrite/` | `Shared/GraphWrite/` |
| `Structure/Review/` | `Shared/Review/` |
| `Structure/RuntimeDiagnostics/` | `Shared/Debug/` |
| `Structure/Transactions/` | `Shared/Transactions/` |
| `Structure/UMGWidget/` | `Shared/UMGWidget/` |

Include 替换：`"Structure/` → `"Shared/`

### Phase 2 — Transaction Facts

```
Private/Transactions/Transactions/  →  Private/Systems/Transactions/
Public/Transactions/Transactions/   →  Public/Systems/Transactions/
```

Include 替换：`"Transactions/Transactions/` → `"Systems/Transactions/`

### Phase 3 — Tool Clusters

每个工具簇独立移动：

```
Private/Services/{Cluster}/  →  Private/Systems/ToolClusters/{Cluster}/
Public/Services/{Cluster}/   →  Public/Systems/ToolClusters/{Cluster}/
```

10 个工具簇：GraphWrite、AssetFactory、BlueprintComponent、BlueprintClassSettings、BlueprintSignature、BlueprintVariables、UMGWidget、DataTable、ObjectProperty（原 DataAssetObjectProperty）、CleanupOwnership

Include 替换：每个簇独立 `"Services/{Cluster}/` → `"Systems/ToolClusters/{Cluster}/`

### Phase 4 — Debug Diagnostics

```
Private/Services/RuntimeDiagnostics/  →  Private/Systems/Debug/
Public/Services/RuntimeDiagnostics/   →  Public/Systems/Debug/
```

Include 替换：
- `"Services/RuntimeDiagnostics/` → `"Systems/Debug/`
- `"Structure/RuntimeDiagnostics/` → `"Shared/Debug/`

### Phase 5 — Review Audit

```
Private/Services/Review/  →  Private/Systems/Review/
Public/Services/Review/   →  Public/Systems/Review/
Private/Widgets/Review/   →  Private/UI/Review/
Public/Widgets/Review/    →  Public/UI/Review/
```

Include 替换：
- `"Services/Review/` → `"Systems/Review/`
- `"Widgets/Review/` → `"UI/Review/`

### Phase 6 — TaskRuntime

```
Private/TaskRuntime/  →  Private/Runtime/TaskRuntime/
Public/TaskRuntime/   →  Public/Runtime/TaskRuntime/
```

Include 替换：`"TaskRuntime/` → `"Runtime/TaskRuntime/`

### Phase 7 — Entry / Bridge

```
Private/Bridge/             →  Private/Entry/
Public/Bridge/              →  Public/Entry/
Private/BlueprintHelper.cpp →  Private/Entry/BlueprintHelper.cpp
Public/BlueprintHelper.h    →  Public/Entry/BlueprintHelper.h
```

Include 替换：
- `"Bridge/` → `"Entry/`
- `"BlueprintHelper.h"` → `"Entry/BlueprintHelper.h"`

### Phase 8 — 收尾合并

| 源 | 目标 |
|---|---|
| `GraphSupport/` | `Systems/ToolClusters/GraphWrite/GraphSupport/` |
| `GraphWrite/` | `Systems/ToolClusters/GraphWrite/` |
| `Logic/` | `Systems/ToolClusters/GraphWrite/Logic/` |
| `NodeHandlers/GraphWrite/` | `Systems/ToolClusters/GraphWrite/NodeHandlers/` |
| `OperationHandlers/GraphWrite/` | `Systems/ToolClusters/GraphWrite/OperationHandlers/` |
| `OperationHandlers/BlueprintVariables/` | `Systems/ToolClusters/BlueprintVariables/OperationHandlers/` |
| `OperationHandlers/BlueprintOperationHandler.*` | `Systems/ToolClusters/GraphWrite/OperationHandlers/` |
| `Safety/` | `Shared/Safety/` |
| `Widgets/*` | `UI/` |
| `SHelperMainWidget.*` | `UI/` |
| `Services/` 残余 | `Shared/Services/` |

## 最终目标结构

```
Private/
├── Entry/                  # BlueprintHelper.cpp + Bridge
├── Runtime/
│   └── TaskRuntime/        # 编排执行
├── Systems/
│   ├── Review/             # 用户审计
│   ├── Debug/              # 开发者诊断
│   ├── Transactions/       # 事实日志
│   └── ToolClusters/
│       ├── GraphWrite/
│       ├── AssetFactory/
│       ├── BlueprintComponent/
│       ├── BlueprintClassSettings/
│       ├── BlueprintSignature/
│       ├── BlueprintVariables/
│       ├── UMGWidget/
│       ├── DataTable/
│       ├── ObjectProperty/
│       └── CleanupOwnership/
├── Shared/                 # 类型定义 + Safety + 跨簇工具
├── UI/                     # 编辑器 UI 组件
└── Tests/                  # 测试（当前不动）
```

Public/ 目录完全镜像。

## 2026-05-09 状态核查

物理目录迁移已完成到本文目标结构。

核查结果：

```text
Public 顶层目录：Entry / Runtime / Shared / Systems / UI
Private 顶层目录：Entry / Runtime / Shared / Systems / Tests / UI
旧顶层目录：Services / Structure / TaskRuntime / Widgets / Bridge 均不存在
旧 include 前缀：未发现 Services/、Structure/、TaskRuntime/、Widgets/、Bridge/、Transactions/Transactions/ 等旧路径引用
```

当前源码落点与阶段对应：

```text
Phase 1 Shared DTO：完成
Phase 2 Transaction Facts：完成
Phase 3 Tool Clusters：完成
Phase 4 Debug Diagnostics：完成
Phase 5 Review Audit：完成
Phase 6 TaskRuntime：完成
Phase 7 Entry / Bridge：完成
Phase 8 收尾合并：完成
```

验证口径：

```text
本次核查只做静态目录和 include 扫描。
用户已反馈手动编译通过；本次不再启动编译。
```

## 迁移约束

1. 每阶段完成后编译验证，不跳步。
2. Target 目录不存在时先 mkdir -p。
3. 每次 git mv 后立即对应 grep+sed 替换 include 路径。
4. Build.cs 不需要修改（UE 默认模块约定自动包含 Private/ 和 Public/ 全部子目录）。
5. 空目录在阶段末统一清理。
6. 不修改任何 .cpp/.h 业务逻辑，纯文件移动 + include 路径替换。
