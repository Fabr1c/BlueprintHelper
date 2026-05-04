# BlueprintHelper Editor Lifecycle / PIE / Risk Command UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Editor Lifecycle / PIE / Risk Command 字段确认稿  
本文边界：确认编辑器生命周期读取、PIE 启停、关闭编辑器风险命令的 Agent-facing 返回字段、UE 侧结构体映射、dry_run、risk_command 授权、no_op、失败结果和只读 / UI 操作边界。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

Editor Lifecycle / PIE / Risk Command 工具簇采用以下字段口径：

```text
1. 增加 get_editor_lifecycle_status。
2. get_editor_lifecycle_status 是只读工具，modified=false。
3. get_editor_lifecycle_status 返回 editor_running / pie_running / unsaved_asset_count。
4. 增加 start_pie_session。
5. start_pie_session 成功返回 started / already_running。
6. start_pie_session 不负责 compile/save。
7. start_pie_session 编译错误阻断时通过 error.conflicts 返回。
8. 增加 stop_pie_session。
9. stop_pie_session 成功返回 stopped / was_running。
10. stop_pie_session 未运行时返回 no_op / reason=pie_not_running。
11. 增加 close_editor。
12. close_editor 必须用户明确要求，Agent 不得自动调用。
13. close_editor 必须受 risk_command 保护。
14. close_editor 必须 dry_run。
15. close_editor dry_run blocked 返回 risk_command_missing / unsaved_assets_exist 等冲突。
16. close_editor 成功只返回 close_requested。
17. 本簇所有工具 modified=false。
18. 本簇所有工具不返回 validation / write_ref / transaction_id / review / safety。
19. 所有 data.schema 使用短命名。
```

---

## 1. 工具簇边界

第一版覆盖：

```text
get_editor_lifecycle_status
start_pie_session
stop_pie_session
close_editor
```

第一版不覆盖：

```text
切换地图
重启 UE 编辑器
运行命令行 Cook / Build / Package
修改 Editor Preferences
修改 Project Settings
```

这些属于更高风险或更大范围操作，后续单独设计。

---

## 2. 通用返回原则

Editor Lifecycle 工具不是资产写工具。

统一规则：

```text
modified=false
不返回 validation
不返回 write_ref
不返回 transaction_id
不返回 journal_recorded
不返回 review
不返回 safety
data.schema 使用短命名
```

说明：

```text
start_pie / stop_pie / close_editor 都是 Editor lifecycle 操作，不是资产修改。
close_editor 是风险命令，必须受 risk_command 保护。
```

---

# 3. get_editor_lifecycle_status

## 3.1 工具定位

`get_editor_lifecycle_status` 负责读取当前 UE 编辑器生命周期状态。

它是只读工具。

---

## 3.2 operation

```json
"operation": "get_editor_lifecycle_status"
```

---

## 3.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_editor_lifecycle_status",
  "trace_id": "trace_20260503_3901",
  "status": "completed",
  "modified": false,

  "target": {
    "read_scope": "editor_lifecycle"
  },

  "data": {
    "schema": "EditorLifecycleStatus.v1",
    "editor_lifecycle": {
      "editor_running": true,
      "pie_running": false,
      "unsaved_asset_count": 2
    }
  }
}
```

---

## 3.4 editor_lifecycle 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bEditorRunning` | `bool` | `data.editor_lifecycle.editor_running` | `boolean` | 是 | UE Editor 是否正在运行。 |
| `bPieRunning` | `bool` | `data.editor_lifecycle.pie_running` | `boolean` | 是 | PIE 是否正在运行。 |
| `UnsavedAssetCount` | `int32` | `data.editor_lifecycle.unsaved_asset_count` | `number` | 是 | 当前未保存资产数量。 |

可选后置字段：

```text
active_map
play_world_type
```

第一版建议不默认返回，避免状态膨胀。

---

# 4. start_pie_session

## 4.1 工具定位

`start_pie_session` 负责启动 Play In Editor。

它不负责：

```text
保存资产
编译蓝图
修复编译错误
切换地图
修改 PIE 设置
```

如果前序写工具返回：

```json
"validation": {
  "should_compile": true,
  "should_save": true
}
```

Agent 应先按需要调用：

```text
compile_blueprint_asset
save_asset
```

再进入 PIE。

---

## 4.2 operation

```json
"operation": "start_pie_session"
```

---

## 4.3 成功启动

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "start_pie_session",
  "trace_id": "trace_20260503_4001",
  "status": "completed",
  "modified": false,

  "target": {
    "lifecycle_scope": "pie"
  },

  "data": {
    "schema": "StartPieSession.v1",
    "pie_result": {
      "started": true,
      "already_running": false
    }
  }
}
```

---

## 4.4 no_op：PIE 已运行

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "start_pie_session",
  "trace_id": "trace_20260503_4002",
  "status": "no_op",
  "modified": false,

  "target": {
    "lifecycle_scope": "pie"
  },

  "data": {
    "schema": "StartPieSession.v1",
    "pie_result": {
      "started": false,
      "already_running": true
    }
  }
}
```

---

## 4.5 pie_result 字段映射：start

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bStarted` | `bool` | `data.pie_result.started` | `boolean` | 是 | 本次是否启动 PIE。 |
| `bAlreadyRunning` | `bool` | `data.pie_result.already_running` | `boolean` | 是 | PIE 是否原本已运行。 |

---

## 4.6 启动失败

例如蓝图仍有编译错误、PIE API 失败、Bridge 断开。

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "start_pie_session",
  "trace_id": "trace_20260503_4003",
  "status": "failed",
  "modified": false,

  "target": {
    "lifecycle_scope": "pie"
  },

  "error": {
    "code": "pie_start_failed",
    "stage": "start_pie",
    "message": "Play In Editor could not be started.",
    "retryable": true,
    "conflicts": [
      {
        "code": "blueprint_compile_errors_exist",
        "message": "One or more Blueprint assets still have compile errors."
      }
    ]
  }
}
```

---

# 5. stop_pie_session

## 5.1 工具定位

`stop_pie_session` 负责停止当前 PIE session。

它不是资产写工具。

---

## 5.2 operation

```json
"operation": "stop_pie_session"
```

---

## 5.3 成功停止

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "stop_pie_session",
  "trace_id": "trace_20260503_4101",
  "status": "completed",
  "modified": false,

  "target": {
    "lifecycle_scope": "pie"
  },

  "data": {
    "schema": "StopPieSession.v1",
    "pie_result": {
      "stopped": true,
      "was_running": true
    }
  }
}
```

---

## 5.4 no_op：PIE 未运行

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "stop_pie_session",
  "trace_id": "trace_20260503_4102",
  "status": "no_op",
  "modified": false,

  "target": {
    "lifecycle_scope": "pie"
  },

  "data": {
    "schema": "StopPieSession.v1",
    "pie_result": {
      "stopped": false,
      "was_running": false,
      "reason": "pie_not_running"
    }
  }
}
```

---

## 5.5 pie_result 字段映射：stop

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bStopped` | `bool` | `data.pie_result.stopped` | `boolean` | 是 | 本次是否停止 PIE。 |
| `bWasRunning` | `bool` | `data.pie_result.was_running` | `boolean` | 是 | PIE 操作前是否正在运行。 |
| `Reason` | `FString` 或 enum | `data.pie_result.reason` | `string` | no_op 时 | 例如 `pie_not_running`。 |

---

## 5.6 停止失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "stop_pie_session",
  "trace_id": "trace_20260503_4103",
  "status": "failed",
  "modified": false,

  "target": {
    "lifecycle_scope": "pie"
  },

  "error": {
    "code": "pie_stop_failed",
    "stage": "stop_pie",
    "message": "Play In Editor could not be stopped.",
    "retryable": true
  }
}
```

---

# 6. close_editor

## 6.1 工具定位

`close_editor` 负责请求关闭 Unreal Editor。

这是风险命令，不是普通 lifecycle 操作。

必须满足：

```text
用户明确要求关闭编辑器
runtime_profile 未阻断 close_editor
risk_command 已启用并授权 close_editor
如有 unsaved assets，必须按策略处理
```

Agent 不得在普通蓝图任务完成后自动关闭编辑器。

---

## 6.2 operation

```json
"operation": "close_editor"
```

---

## 6.3 dry_run 必须支持

`close_editor` 必须 dry_run。

---

## 6.4 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
  "trace_id": "trace_20260503_4201",
  "status": "dry_run",
  "modified": false,

  "target": {
    "lifecycle_scope": "editor"
  },

  "data": {
    "schema": "CloseEditorDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

---

## 6.5 dry_run blocked：risk_command 缺失

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
  "trace_id": "trace_20260503_4202",
  "status": "dry_run",
  "modified": false,

  "target": {
    "lifecycle_scope": "editor"
  },

  "data": {
    "schema": "CloseEditorDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "risk_command_missing"
      ],
      "conflicts": [
        {
          "code": "risk_command_missing",
          "command": "close_editor",
          "message": "close_editor requires risk_command authorization."
        }
      ],
      "errors": []
    }
  }
}
```

---

## 6.6 dry_run blocked：存在未保存资产

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
  "trace_id": "trace_20260503_4203",
  "status": "dry_run",
  "modified": false,

  "target": {
    "lifecycle_scope": "editor"
  },

  "data": {
    "schema": "CloseEditorDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "unsaved_assets_exist"
      ],
      "conflicts": [
        {
          "code": "unsaved_assets_exist",
          "unsaved_asset_count": 3,
          "message": "There are unsaved assets. Close policy does not allow closing with unsaved changes."
        }
      ],
      "errors": []
    }
  }
}
```

---

## 6.7 正式成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
  "trace_id": "trace_20260503_4204",
  "status": "completed",
  "modified": false,

  "target": {
    "lifecycle_scope": "editor"
  },

  "data": {
    "schema": "CloseEditor.v1",
    "close_result": {
      "close_requested": true
    }
  }
}
```

---

## 6.8 no_op：编辑器已在关闭流程

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
  "trace_id": "trace_20260503_4205",
  "status": "no_op",
  "modified": false,

  "target": {
    "lifecycle_scope": "editor"
  },

  "data": {
    "schema": "CloseEditor.v1",
    "close_result": {
      "close_requested": false,
      "already_closing": true
    }
  }
}
```

---

## 6.9 close_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bCloseRequested` | `bool` | `data.close_result.close_requested` | `boolean` | 是 | 是否已请求关闭编辑器。 |
| `bAlreadyClosing` | `bool` | `data.close_result.already_closing` | `boolean` | no_op 时 | 编辑器是否已经处于关闭流程。 |

---

## 6.10 正式失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
  "trace_id": "trace_20260503_4206",
  "status": "failed",
  "modified": false,

  "target": {
    "lifecycle_scope": "editor"
  },

  "error": {
    "code": "risk_command_missing",
    "stage": "authorize_command",
    "message": "close_editor requires risk_command authorization.",
    "retryable": false,
    "conflicts": [
      {
        "code": "risk_command_missing",
        "command": "close_editor"
      }
    ]
  }
}
```

---

# 7. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperLifecycleErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperLifecycleStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |

不强制返回：

```text
rollback_result
```

因为本簇不是资产写事务工具。

---

# 8. UE 侧建议结构体

```cpp
struct FBlueprintHelperEditorLifecycleStatusData
{
    FString Schema; // EditorLifecycleStatus.v1
    FBlueprintHelperEditorLifecycleStatus EditorLifecycle;
};

struct FBlueprintHelperEditorLifecycleStatus
{
    bool bEditorRunning = false;
    bool bPieRunning = false;
    int32 UnsavedAssetCount = 0;
};

struct FBlueprintHelperPieResultData
{
    FString Schema; // StartPieSession.v1 / StopPieSession.v1
    FBlueprintHelperPieResult PieResult;
};

struct FBlueprintHelperPieResult
{
    bool bStarted = false;
    bool bAlreadyRunning = false;
    bool bStopped = false;
    bool bWasRunning = false;
    FString Reason;
};

struct FBlueprintHelperCloseEditorResultData
{
    FString Schema; // CloseEditor.v1
    FBlueprintHelperCloseEditorResult CloseResult;
};

struct FBlueprintHelperCloseEditorResult
{
    bool bCloseRequested = false;
    bool bAlreadyClosing = false;
};
```

明确不包含：

```cpp
FBlueprintHelperValidationResult
FBlueprintHelperWriteRef
FString TransactionId
FString ReviewStatus
```

---

# 9. 验收标准

```text
1. get_editor_lifecycle_status 是只读工具。
2. get_editor_lifecycle_status 返回 editor_running / pie_running / unsaved_asset_count。
3. start_pie_session 成功返回 started / already_running。
4. start_pie_session 不负责 compile/save。
5. stop_pie_session 成功返回 stopped / was_running。
6. stop_pie_session 未运行返回 no_op / reason=pie_not_running。
7. close_editor 必须用户明确要求。
8. close_editor 必须受 risk_command 保护。
9. close_editor 必须 dry_run。
10. close_editor blocked 返回 conflicts。
11. close_editor 成功只返回 close_requested。
12. 本簇所有工具 modified=false。
13. 本簇不返回 validation / write_ref / transaction_id / review / safety。
14. data.schema 使用短命名。
