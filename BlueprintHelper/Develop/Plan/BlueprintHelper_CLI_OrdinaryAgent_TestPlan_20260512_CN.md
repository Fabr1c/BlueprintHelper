# BlueprintHelper CLI 普通 Agent 全面测试文档

Date: 2026-05-12

本文从普通 Agent 视角验证 BlueprintHelper。唯一入口是 CLI direct tool invocation:

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

本文不把任何非 CLI 调用当作入口、替代路径或通过标准。底层 Bridge、UE Task Runtime、Python compiler 可以作为实现链路被验证，但普通 Agent 只能通过 CLI 观察它们是否接通。

## 0. 运行变量

每轮测试必须先填写:

```text
PLUGIN_ROOT=<D:/UEProjects/Template/Plugins/BlueprintHelper>
PROJECT_ROOT=<absolute UE project root>
PROJECT_FILE=<absolute .uproject path>
ENGINE_DIR=<absolute UE engine root>
RUN_ID=BH_CLI_Agent_<yyyymmdd>_<nn>
SMOKE_ROOT=/Game/BlueprintHelperCliSmoke/${RUN_ID}
CLI_ARTIFACT_ROOT=<PROJECT_ROOT>/.blueprinthelper/cli-runs/${RUN_ID}
```

推荐一次性临时资产:

```text
${SMOKE_ROOT}/BP_CliAgentActor
${SMOKE_ROOT}/BPI_CliAgentInteract
${SMOKE_ROOT}/ST_CliAgentRow
${SMOKE_ROOT}/DT_CliAgentRows
${SMOKE_ROOT}/WBP_CliAgentPanel
${SMOKE_ROOT}/BP_CliAgentDataAssetClass
${SMOKE_ROOT}/DA_CliAgentData
```

全局预期:

- 所有写入都必须先 `blueprinthelper_preview_task`，再按需 `blueprinthelper_request_write_session`，最后 `blueprinthelper_execute_task`。
- 每次 execute 后必须调用 `blueprinthelper_get_task_result`，并做至少一次 read-back。
- 所有写入 TaskSpec 必须显式填写 `validation.should_compile` 和 `validation.should_save`。
- CLI stdout 必须是 JSON object，默认 `summary` 格式必须包含 `schema=BlueprintHelper.CliResult.v1`，除非命令使用 `--select` 主动裁剪该字段。
- `artifacts.full_result` 必须存在。需要排查时打开该 artifact，只验证 `toolResult.operation/status/modified/data.schema` 和 `extra` 中的业务字段，不把历史 envelope 名称作为普通 Agent 入口依据。
- 普通 Agent 不传、不请求、不输出 `auth_session`、`auth_token`、`BLUEPRINTHELPER_BRIDGE_TOKEN` 或本地 DebugBundle 绝对路径。

## 1. CLI 安装与入口面

### 1.1 CLI 可启动

命令:

```powershell
bh --help
```

预期:

- Exit code: `0`。
- stdout 包含 `BlueprintHelper CLI`。
- stdout 包含 direct tool 用法: `blueprinthelper-cli <tool_name>`。
- stdout 包含 `task preview`、`task execute`、`task result`、`context read` 兼容命令说明。

### 1.2 Direct tool 名称可调度

命令:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,artifacts.full_result
```

预期 stdout 字段:

```json
{
  "status": "completed",
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "ok": true,
    "operation": "get_runtime_profile",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "RuntimeProfile.v1"
    }
  }
}
```

失败判定:

- CLI 返回 unknown tool。
- stdout 不是 JSON。
- `status` 不是 `completed`。
- `artifacts.full_result` 缺失。

### 1.3 冻结直连命令不能被普通 CLI 暴露

命令:

```powershell
bh blueprint_exec_console_command --json "{ \"command\": \"stat fps\" }" --expert
```

预期:

- Exit code: `64`。
- stderr 包含 `Unsupported BlueprintHelper CLI command`。
- stdout 为空。
- UE 侧没有执行 console command。

同类必须拒绝的 direct tool 名称:

```text
blueprint_import_agent_graph
blueprint_compile_blueprint
blueprint_add_component
blueprint_add_variable
blueprint_save_asset
blueprint_close_editor
```

### 1.4 `--select` 字段裁剪

命令:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status
```

预期 stdout:

```json
{
  "status": "completed"
}
```

预期:

- stdout 只包含 `status`。
- 不包含 `schema`、`operation`、`tool_name`、`artifacts`。
- Exit code: `0`。

### 1.5 无效 `--select` 字段路径

命令:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,$schema
```

预期:

- Exit code: `64`。
- stderr 包含 `Invalid field path`。
- 不创建新的成功 artifact。

### 1.6 输出预算保护

命令示例:

```powershell
bh blueprinthelper_read_agent_guide --json "{}" --max-bytes 40
```

预期 stdout 字段:

```json
{
  "ok": false,
  "schema": "BlueprintHelper.CliResult.v1",
  "operation": "output",
  "status": "output_too_large",
  "message": "CLI stdout exceeds --max-bytes. Read the artifact path instead.",
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期:

- Exit code: `3`。
- `artifacts.full_result` 可打开。
- full result 中仍包含完整 tool result。

## 2. 只读预检与连通性

### 2.1 Static diagnostics

命令:

```powershell
bh blueprinthelper_diagnostics --json "{}" --select status,summary,artifacts.full_result
```

预期 stdout 字段:

```json
{
  "status": "completed",
  "summary": {
    "errors": 0,
    "modified": false
  },
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "blueprinthelper_diagnostics",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "Diagnostics.v1",
      "mode": "static",
      "format": "markdown",
      "markdown": "<contains ## Blocking>"
    }
  }
}
```

通过标准:

- `data.markdown` 中 `## Blocking` 下为 `None`，或只有已记录的非阻塞安装问题。
- 不要求 UE Editor 已启动。

### 2.2 Agent guide 可读

命令:

```powershell
bh blueprinthelper_read_agent_guide --json "{}" --select status,artifacts.full_result
```

预期 stdout 字段:

```json
{
  "status": "completed",
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "blueprinthelper_read_agent_guide",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "AgentGuideMarkdown.v1",
      "format": "markdown",
      "markdown": "<non_empty_markdown>"
    }
  }
}
```

通过标准:

- `data.markdown` 包含普通 Agent CLI-first 流程。
- 不出现要求普通 Agent 调用冻结直连工具的步骤。

### 2.3 打开 Editor 预检

仅在用户明确需要 CLI 启动目标 Editor 时执行。

命令:

```powershell
bh blueprint_open_editor --json "{ \"project_file\": \"<PROJECT_FILE>\", \"wait_timeout_ms\": 120000 }" --select status,artifacts.full_result
```

预期 stdout 字段:

```json
{
  "status": "completed",
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "blueprint_open_editor",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "EditorLaunchResult.v1",
      "code": "EDITOR_BRIDGE_AVAILABLE",
      "editor_exe": "<absolute UnrealEditor.exe>",
      "uproject_path": "<PROJECT_FILE>",
      "elapsed_ms": "<number>"
    }
  }
}
```

通过标准:

- `elapsed_ms <= wait_timeout_ms`。
- Editor 已加载项目并可响应后续 CLI runtime profile。
- 如果失败，`status` 为 `failed` 或 `bridge_unavailable`，错误信息必须可定位到启动路径或 Bridge 超时。

### 2.4 Runtime profile

命令:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary,artifacts.full_result
```

预期 stdout 字段:

```json
{
  "status": "completed",
  "summary": {
    "modified": false
  },
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "get_runtime_profile",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "RuntimeProfile.v1",
      "runtime_profile": {
        "status": "ok|degraded|blocked"
      }
    }
  }
}
```

通过标准:

- `runtime_profile.status` 为 `ok` 或 `degraded` 可继续只读测试。
- 若 `degraded` 且 `write_permission.reason=write_session_missing`，必须先 preview，再请求 write session，不能直接 execute。
- 若 `blocked`，停止写入测试并记录 `runtime_profile` 中的具体阻塞字段。

### 2.5 Runtime diagnostics

命令:

```powershell
bh blueprinthelper_diagnostics_runtime --json "{}" --select status,summary,artifacts.full_result
```

预期 stdout 字段:

```json
{
  "status": "completed",
  "summary": {
    "errors": 0,
    "modified": false
  },
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "diagnostics_runtime",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "Diagnostics.v1",
      "mode": "runtime",
      "format": "markdown",
      "markdown": "<contains ## Blocking>"
    }
  }
}
```

通过标准:

- `## Blocking` 下为 `None`。
- 如果存在 Bridge/Editor 连接阻塞，后续 UE read/write 测试全部标记为 blocked，不允许改用本地缓存文件代替。

## 3. 上下文读取

### 3.1 Task context

输入文件 `read_task_context.json`:

```json
{
  "target": {
    "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor"
  },
  "feature_name": "CliAgentSmoke"
}
```

命令:

```powershell
bh blueprinthelper_read_task_context --file read_task_context.json --select status,summary,artifacts.full_result
```

预期 stdout 字段:

```json
{
  "status": "completed",
  "summary": {
    "target_assets": ["${SMOKE_ROOT}/BP_CliAgentActor"],
    "modified": false
  },
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "read_task_context",
    "status": "completed",
    "modified": false,
    "target": {
      "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
      "target_type": "blueprint"
    },
    "data": {
      "schema": "BlueprintHelper.TaskContextPack.v1",
      "context_id": "<non_empty_string>",
      "runtime": {
        "bridge_reachable": true
      },
      "target": {
        "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
        "exists": true
      },
      "blueprint_summary": {
        "graphs": "<array>"
      },
      "recommended_constraints": {
        "prefer_new_graph": "<boolean>",
        "allow_modify_user_nodes": false,
        "graph_strategy": "append_new_owned_graph"
      }
    }
  }
}
```

通过标准:

- 已存在资产时 `target.exists=true`。
- 新建前读取不存在资产时允许 `target.exists=false`，但 `schema/context_id/runtime` 仍必须存在。

### 3.2 Read context schema

输入:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
    "target_type": "blueprint"
  },
  "view": {
    "format": "schema"
  }
}
```

命令:

```powershell
bh blueprinthelper_read_context --file read_context_schema.json --select status,artifacts.full_result
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "read_context",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "ReadContextPack.v1",
      "read_type": "blueprint_logic",
      "format": "schema",
      "payload": {
        "schema": "BlueprintLogicReadSchema.v1",
        "target_types": ["blueprint", "graph", "function", "event", "custom_event", "block"],
        "formats": ["logic_md", "logic_json", "summary", "schema"]
      },
      "truncated": false
    }
  }
}
```

### 3.3 Read context summary

输入:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
    "target_type": "graph",
    "target_name": "EventGraph"
  },
  "view": {
    "format": "summary",
    "detail": "brief"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "read_context",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "ReadContextPack.v1",
      "read_type": "blueprint_logic",
      "format": "summary",
      "scope": "target_graph",
      "payload": "<summary_object>",
      "stats": "<object>",
      "truncated": false
    }
  }
}
```

通过标准:

- summary 不内联完整 graph markdown。
- `stats.nodes` 如果存在，必须是 number。
- 大图后续必须优先用 `logic_json` 或 target-entry slice，不能直接全图 `logic_md`。

### 3.4 Read context logic_json

输入:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
    "target_type": "graph",
    "target_name": "EventGraph"
  },
  "view": {
    "format": "logic_json",
    "max_items": 200,
    "detail": "normal"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "read_context",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "ReadContextPack.v1",
      "format": "logic_json",
      "payload": "<logic_json_or_slice>",
      "stats": "<object>",
      "truncated": false
    }
  }
}
```

通过标准:

- 如果 `data.truncated=true`，必须同时存在 `payload.truncation.nodes_total` 和 `payload.truncation.nodes_returned`。
- GraphWrite patch/merge 的 anchor 只能从该 read-back 或后续 `logic_json` 读取中取得。

### 3.5 Target-entry logic_md

输入:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
    "target_type": "custom_event",
    "target_name": "BH_CliAgent_Anchor"
  },
  "view": {
    "format": "logic_md",
    "detail": "normal"
  }
}
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "read_context",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "ReadContextPack.v1",
      "format": "logic_md",
      "scope": "target_custom_event",
      "payload": "<sliced_logic_payload>",
      "stats": "<object>"
    }
  }
}
```

通过标准:

- `scope` 是 target-entry scope，不是 whole blueprint dump。
- payload 中能定位 `BH_CliAgent_Anchor`。

### 3.6 Reference context

输入:

```json
{
  "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
  "target_type": "graph",
  "graph_name": "EventGraph",
  "scope": "safety_context",
  "max_results": 50,
  "include_samples": true
}
```

命令:

```powershell
bh blueprinthelper_read_reference_context --file read_reference_context.json --select status,summary,artifacts.full_result
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "read_reference_context",
    "status": "completed",
    "modified": false,
    "target": {
      "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
      "target_type": "graph"
    },
    "data": "<reference_context_object>"
  }
}
```

通过标准:

- 不存在本地路径泄漏。
- remove 或修改既有用户内容前必须有 reference context 证据。

## 4. 写权限会话

### 4.1 成功 preview 后请求写权限

前置:

- 至少完成一个 `blueprinthelper_preview_task`，且 stdout `status=preview_passed`。
- runtime profile 显示 `write_permission.reason=write_session_missing` 或 execute 明确需要用户授权。

命令:

```powershell
bh blueprinthelper_request_write_session --json "{ \"reason\": \"CLI ordinary Agent smoke ${RUN_ID}\", \"scope\": \"project\", \"ttl_seconds\": 900, \"asset_paths\": [\"${SMOKE_ROOT}/BP_CliAgentActor\"] }" --select status,artifacts.full_result
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "blueprinthelper_request_write_session",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "WriteSession.v1",
      "write_session": {
        "scope": "project",
        "expires_at_utc": "<non_empty_or_null>",
        "asset_paths": ["${SMOKE_ROOT}/BP_CliAgentActor"]
      }
    }
  }
}
```

通过标准:

- UE Editor 弹出简洁同意/拒绝提示。
- 同意后不向 stdout 或 artifact 暴露 raw session id。
- 后续 execute 不要求 Agent 传任何 token。

### 4.2 用户拒绝写权限

执行同一命令但在 Editor 中拒绝。

预期:

- Exit code 非 0，或 stdout `status=failed`。
- artifact `toolResult.ok=false`。
- `toolResult.error.code` 为 `unauthorized`、`write_rejected` 或当前实现等价拒绝码。
- `modified=false`。
- 后续不得执行 `blueprinthelper_execute_task`。

## 5. TaskSpec 写入通用断言

每个写入用例都必须执行以下三步。

### 5.1 Preview

命令:

```powershell
bh blueprinthelper_preview_task --file task.json --select status,preview_id,summary,artifacts.full_result,artifacts.task_plan
```

成功预期 stdout:

```json
{
  "status": "preview_passed",
  "preview_id": "<non_empty_string>",
  "summary": {
    "target_assets": ["<target_asset>"],
    "task_type": "<task_type>",
    "planned_steps": "<number >= 1>",
    "warnings": 0,
    "errors": 0,
    "modified": false
  },
  "artifacts": {
    "full_result": "<non_empty_json_path>",
    "task_plan": "<non_empty_json_path>"
  }
}
```

成功预期 artifact:

```json
{
  "toolResult": {
    "operation": "preview_task",
    "status": "dry_run",
    "modified": false,
    "data": {
      "schema": "BlueprintHelper.TaskPreview.v1",
      "preview_id": "<same_as_stdout>",
      "passed": true,
      "blocked": false,
      "task_plan": "<summarized_plan>",
      "issues": []
    }
  },
  "extra": {
    "previewId": "<same_as_stdout>",
    "taskPlan": {
      "schema": "BlueprintHelper.TaskPlan.v1",
      "task_type": "<task_type>",
      "target_assets": ["<target_asset>"],
      "execution_policy": {
        "dry_run_mode": "full|quick|none",
        "should_compile": "<boolean>",
        "should_save": "<boolean>"
      },
      "steps": "<array length >= 1>"
    },
    "passed": true,
    "issues": []
  }
}
```

失败预期:

```json
{
  "status": "preview_blocked",
  "summary": {
    "errors": "<number >= 1>",
    "modified": false
  },
  "error_code": "<non_empty_code>",
  "message": "<non_empty_message>"
}
```

失败时通过标准:

- artifact `toolResult.data.issues[]` 每项有 `code`、`path`、`message`。
- 不允许继续 execute。

### 5.2 Execute

命令:

```powershell
bh blueprinthelper_execute_task --file task.json --select status,task_run_id,summary,artifacts.full_result
```

成功预期 stdout:

```json
{
  "status": "executed",
  "task_run_id": "<non_empty_string>",
  "summary": {
    "target_assets": ["<target_asset>"],
    "planned_steps": "<number >= 1>",
    "modified": true
  },
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

成功预期 artifact:

```json
{
  "toolResult": {
    "operation": "execute_task",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "BlueprintHelper.TaskExecution.v1",
      "task_run_id": "<same_as_stdout>",
      "preview_id": "<non_empty_string>",
      "task": {
        "task_run_id": "<same_as_stdout>",
        "feature_name": "<feature_name_or_null>",
        "target_assets": ["<target_asset>"],
        "applied_steps": "<number >= 1>",
        "modified_assets": "<number >= 1>"
      },
      "bridge_result": "<object>"
    }
  }
}
```

说明:

- CLI summary 的 `summary.modified=true` 表示本次任务造成 UE 侧写入。
- 当前 task-core `execute_task` tool result 可能仍是 read-style `modified=false`；最终以 `TaskExecution.v1.task.modified_assets`、UE read-back 和 Review/dirty 状态确认写入。

### 5.3 Get task result

命令:

```powershell
bh blueprinthelper_get_task_result --json "{ \"task_run_id\": \"<TASK_RUN_ID>\" }" --select status,task_run_id,summary,artifacts.full_result
```

预期 stdout:

```json
{
  "status": "result_found",
  "task_run_id": "<TASK_RUN_ID>",
  "summary": {
    "target_assets": ["<target_asset>"],
    "task_type": "<task_type>",
    "planned_steps": "<number >= 1>",
    "modified": false
  },
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

预期 artifact:

```json
{
  "toolResult": {
    "operation": "get_task_result",
    "status": "completed",
    "modified": false,
    "data": {
      "schema": "BlueprintHelper.TaskRunJournal.v1",
      "task_run_id": "<TASK_RUN_ID>",
      "task_type": "<task_type>",
      "status": "completed|failed|partial_failure",
      "target_assets": ["<target_asset>"],
      "steps": [
        {
          "step_id": "<non_empty_string>",
          "operation": "<non_empty_string>",
          "status": "completed|failed|blocked|skipped"
        }
      ]
    }
  }
}
```

通过标准:

- 每个 execute 返回的 `task_run_id` 都能查到 journal。
- journal `steps.length` 与 preview `planned_steps` 一致或有明确 blocked/skipped 解释。

## 6. 资产创建用例

### 6.1 Actor Blueprint

TaskSpec 关键字段:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "create_asset",
    "feature_name": "CreateCliAgentActor",
    "target": {
      "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
      "target_type": "asset"
    },
    "behavior": {
      "asset_strategy": "ensure_asset",
      "asset": {
        "asset_type": "blueprint_class",
        "parent_class": "Actor",
        "collision_policy": "reuse_if_exists"
      }
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": true
    }
  }
}
```

预期:

- Preview: `status=preview_passed`，`summary.task_type=create_asset`，`planned_steps>=1`。
- Execute: `status=executed`，`task_run_id` 非空，`summary.target_assets[0]=${SMOKE_ROOT}/BP_CliAgentActor`。
- Result: `data.schema=BlueprintHelper.TaskRunJournal.v1`，`status=completed`。
- Read-back: `blueprinthelper_read_task_context` 中 `target.exists=true`，asset class 是 Blueprint class，parent 是 Actor 或等价 `/Script/Engine.Actor`。
- Compile: 因 `should_compile=true`，journal 或 read-back 不能出现未解释的 compile failure。

### 6.2 Interface

TaskSpec 差异:

```json
{
  "asset_type": "blueprint_interface",
  "collision_policy": "reuse_if_exists"
}
```

预期:

- `validation.should_compile=true`。
- Read-back 显示 `${SMOKE_ROOT}/BPI_CliAgentInteract` 存在。
- 若 execute 返回 `no_op` 等价复用状态，必须由 read-back 证明资产类型正确。

### 6.3 UserDefinedStruct

TaskSpec 差异:

```json
{
  "asset_type": "structure",
  "fields": [
    { "name": "Damage", "type": "int", "default_value": 10 },
    { "name": "DisplayName", "type": "string", "default_value": "Default" }
  ],
  "collision_policy": "reuse_if_exists"
}
```

预期:

- `validation.should_compile=false`。
- Preview/execute/result 三步通过。
- Read-back 证明字段 `Damage:int` 和 `DisplayName:string` 存在。
- 没有 Blueprint compile 结果时不得判为失败。

### 6.4 DataTable

TaskSpec 差异:

```json
{
  "asset_type": "data_table",
  "row_struct": "${SMOKE_ROOT}/ST_CliAgentRow",
  "collision_policy": "reuse_if_exists"
}
```

预期:

- `validation.should_compile=false`。
- Read-back 证明 DataTable 使用 `${SMOKE_ROOT}/ST_CliAgentRow` 作为 row struct。
- `no_op` 只在 row struct 匹配时通过。

### 6.5 WidgetBlueprint

TaskSpec 差异:

```json
{
  "asset_type": "widget_blueprint",
  "parent_class": "UserWidget",
  "collision_policy": "reuse_if_exists"
}
```

预期:

- `validation.should_compile=true`。
- Read-back 显示 WidgetBlueprint 存在。
- UMG 后续编辑可在该资产上执行。

### 6.6 DataAsset Blueprint class

TaskSpec 差异:

```json
{
  "asset_type": "blueprint_class",
  "parent_class": "PrimaryDataAsset",
  "collision_policy": "reuse_if_exists"
}
```

预期:

- `validation.should_compile=true`。
- Read-back 证明 parent 是 `PrimaryDataAsset` 或等价类路径。

### 6.7 DataAsset instance

TaskSpec 差异:

```json
{
  "asset_type": "data_asset",
  "data_asset_class": "${SMOKE_ROOT}/BP_CliAgentDataAssetClass",
  "collision_policy": "reuse_if_exists"
}
```

预期:

- `validation.should_compile=false`。
- Read-back 证明 `${SMOKE_ROOT}/DA_CliAgentData` 存在，class 来自 `${SMOKE_ROOT}/BP_CliAgentDataAssetClass` 的 generated class。
- 不允许使用 `/Script/Engine.DataAsset` 或 `/Script/Engine.PrimaryDataAsset` 作为 `data_asset_class`。

## 7. Blueprint 能力用例

### 7.1 Components

TaskSpec 关键字段:

```json
{
  "task_type": "edit_blueprint_components",
  "target": {
    "asset_path": "${SMOKE_ROOT}/BP_CliAgentActor",
    "target_type": "blueprint"
  },
  "behavior": {
    "component_strategy": "component_tree",
    "changes": [
      {
        "kind": "ensure_component_present",
        "name": "CliAgentScene",
        "class": "SceneComponent",
        "on_name_conflict": "reuse"
      },
      {
        "kind": "ensure_component_present",
        "name": "CliAgentMesh",
        "class": "StaticMeshComponent",
        "attach": {
          "parent": "CliAgentScene",
          "rule": "keep_relative"
        },
        "on_name_conflict": "reuse"
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

预期:

- Preview `summary.task_type=edit_blueprint_components`。
- TaskPlan 至少包含 `blueprint_component` capability 或等价 component step。
- Execute 生成 `task_run_id`。
- Read-back 中 component tree 包含 `CliAgentScene` 和 `CliAgentMesh`，`CliAgentMesh` attach 到 `CliAgentScene` 或等价父节点。

### 7.2 Variables

TaskSpec 关键字段:

```json
{
  "task_type": "edit_blueprint_variables",
  "behavior": {
    "variable_strategy": "member_variables",
    "variables": [
      {
        "kind": "ensure_member_variable",
        "name": "CliAgentHealth",
        "pin_type": { "category": "int" },
        "value": 100
      },
      {
        "kind": "ensure_member_variable",
        "name": "CliAgentLabel",
        "pin_type": { "category": "string" },
        "value": "CLI"
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

预期:

- Preview 通过或返回明确 capability gap。
- 成功时 TaskPlan 包含 `blueprint_variable` capability。
- Read-back 显示 `CliAgentHealth:int`、`CliAgentLabel:string` 存在，默认值匹配或有 UE 默认值转换说明。
- 如果 preview blocked，`issues[]` 必须非空，并记录为 `variables_capability_blocked`，不得改用冻结直连变量工具。

### 7.3 Signature

TaskSpec 关键字段:

```json
{
  "task_type": "edit_blueprint_signature",
  "behavior": {
    "signature_strategy": "signature_edit",
    "changes": [
      {
        "kind": "ensure_function",
        "function_name": "CliAgent_Calc",
        "inputs": [{ "name": "Base", "type": "int" }],
        "outputs": [{ "name": "Result", "type": "int" }],
        "name_collision_policy": "reuse_if_exists"
      },
      {
        "kind": "ensure_custom_event",
        "event_name": "CliAgent_OnInteract",
        "graph_name": "EventGraph",
        "inputs": [{ "name": "InstigatorName", "type": "string" }],
        "name_collision_policy": "reuse_if_exists"
      },
      {
        "kind": "ensure_event_dispatcher",
        "dispatcher_name": "CliAgent_OnTriggered",
        "inputs": [{ "name": "Amount", "type": "int" }],
        "signature_mismatch_policy": "block"
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

预期:

- Preview 通过，TaskPlan 包含 `blueprint_signature` capability。
- Execute 通过。
- Read-back 中函数、CustomEvent、EventDispatcher 均可见。
- signature mismatch 必须 preview blocked，不能 execute 后失败。

### 7.4 Class settings

TaskSpec 关键字段:

```json
{
  "task_type": "edit_blueprint_class_settings",
  "behavior": {
    "class_settings_strategy": "class_settings",
    "interfaces": {
      "ensure_present": ["${SMOKE_ROOT}/BPI_CliAgentInteract"]
    },
    "class_defaults": [
      {
        "kind": "set_object_property",
        "property_path": "bCanBeDamaged",
        "value": false
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

预期:

- Preview 通过，TaskPlan 包含 `blueprint_class_settings` capability。
- Read-back 证明 interface 已实现。
- Class default `bCanBeDamaged=false`，或返回可定位的 property unsupported issue。

## 8. GraphWrite 用例

### 8.1 Append owned graph/custom event

TaskSpec 关键字段:

```json
{
  "task_type": "edit_blueprint_graph",
  "scope_policy": {
    "graph_name": "EventGraph",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "BH_CliAgent_Anchor",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "call_function",
              "name": "/Script/Engine.KismetSystemLibrary:PrintString",
              "args": {
                "InString": {
                  "kind": "literal",
                  "value_type": "string",
                  "value": "CLI Agent Anchor"
                }
              }
            }
          ]
        }
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

预期:

- Preview `status=preview_passed`，TaskPlan 包含 graph write append step。
- Execute `status=executed`。
- Result journal step `status=completed`。
- `read_context logic_json` 中存在 `BH_CliAgent_Anchor`，并能记录:

```text
ANCHOR_BLOCK_ID=<non_empty>
ANCHOR_GROUP_ENTRY_NODE_PATH=<non_empty>
ANCHOR_NODE_REF=<non_empty>
ANCHOR_PIN_REF=<non_empty>
ANCHOR_LINK_REF=<optional>
```

### 8.2 Replace owned graph/custom event body

TaskSpec 关键字段:

```json
{
  "graph_strategy": "replace_owned_graph",
  "replace": {
    "scope": "custom_event_definition",
    "selector": {
      "kind": "custom_event",
      "name": "BH_CliAgent_Anchor"
    },
    "body": {
      "schema": "BlueprintLogicSpec.v1",
      "statements": [
        {
          "kind": "call_function",
          "name": "/Script/Engine.KismetSystemLibrary:PrintString",
          "args": {
            "InString": {
              "kind": "literal",
              "value_type": "string",
              "value": "CLI Agent Replaced"
            }
          }
        }
      ]
    }
  }
}
```

预期:

- Preview 通过，TaskPlan 可拆成 signature + graph_write 或单 graph_write，但必须说明 step 数。
- Execute 通过。
- Read-back 中 `BH_CliAgent_Anchor` 仍存在。
- 原字符串 `CLI Agent Anchor` 不再出现在该 custom event body 中，新字符串 `CLI Agent Replaced` 出现。
- 无 orphan 节点增加，除非 journal 明确记录可接受的中间状态。

### 8.3 Patch owned graph

TaskSpec 关键字段:

```json
{
  "graph_strategy": "patch_owned_graph",
  "patches": [
    {
      "kind": "set_node_comment",
      "target_ref": {
        "node_ref": "<ANCHOR_NODE_REF>"
      },
      "value": "CLI Agent patched comment"
    },
    {
      "kind": "set_pin_default",
      "target_ref": {
        "node_ref": "<PRINT_NODE_REF>",
        "pin_ref": "<INSTRING_PIN_REF>"
      },
      "value": "CLI Agent patched pin"
    }
  ]
}
```

预期:

- Preview blocked if refs are missing, with `issues[].path` 指向 `target_ref.node_ref` 或 `target_ref.pin_ref`。
- Preview passed 时 execute 必须只修改 BlueprintHelper-owned 节点或 pin。
- Read-back 显示 comment/default 更新。
- 不修改 user-owned 节点。

### 8.4 Merge branch_fork owned block call

前置:

- 已有 `BH_CliAgent_Anchor` owned block。
- 另行 append `BH_CliAgent_Inserted` owned custom event，并从 read-back 记录 `INSERTED_BLOCK_ID`。

TaskSpec 关键字段:

```json
{
  "graph_strategy": "merge_owned_graph",
  "merges": [
    {
      "kind": "insert_flow",
      "scope": "owned_block_call",
      "insert_strategy": "branch_fork",
      "anchor": {
        "block_id": "<ANCHOR_BLOCK_ID>",
        "group_entry_node_path": "<ANCHOR_GROUP_ENTRY_NODE_PATH>",
        "node_ref": "<ANCHOR_NODE_REF>",
        "pin_ref": "<ANCHOR_PIN_REF>",
        "link_ref": "<ANCHOR_LINK_REF>"
      },
      "inserted": {
        "call_kind": "owned_block_call",
        "block_id": "<INSERTED_BLOCK_ID>"
      },
      "sequence_order": ["inserted_logic", "original_successor"]
    }
  ]
}
```

预期:

- Preview 通过，TaskPlan merge step 中 `insert_strategy=branch_fork`。
- Execute 通过。
- Read-back 显示 Sequence 或等价分流节点已插入。
- 插入逻辑和 original successor 均从 anchor 可达。
- 不存在 anchor 绕过分流节点直接连向旧 successor 的重复路径。

## 9. UMG 与数据用例

### 9.1 UMG widget tree

TaskSpec 关键字段:

```json
{
  "task_type": "edit_umg_widget",
  "target": {
    "asset_path": "${SMOKE_ROOT}/WBP_CliAgentPanel",
    "target_type": "blueprint"
  },
  "behavior": {
    "widget_strategy": "widget_blueprint_edit",
    "changes": [
      {
        "kind": "create_widget",
        "widget_name": "RootCanvas",
        "widget_class": "CanvasPanel"
      },
      {
        "kind": "create_widget",
        "widget_name": "CliAgentText",
        "widget_class": "TextBlock",
        "parent_widget_name": "RootCanvas"
      },
      {
        "kind": "update_widget_property",
        "widget_name": "CliAgentText",
        "property_path": "Text",
        "value": "Hello CLI Agent"
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

预期:

- Preview 通过，TaskPlan 包含 `umg_widget` capability。
- Execute 通过。
- Read-back 显示 `RootCanvas` 和 `CliAgentText`。
- `CliAgentText` parent 是 `RootCanvas`。
- `Text` 属性等于 `Hello CLI Agent` 或 UE Text 序列化等价值。

### 9.2 DataTable rows

TaskSpec 关键字段:

```json
{
  "task_type": "edit_data_table",
  "target": {
    "asset_path": "${SMOKE_ROOT}/DT_CliAgentRows",
    "target_type": "asset"
  },
  "behavior": {
    "row_strategy": "row_edit",
    "rows": [
      {
        "action": "add",
        "row_name": "Sword",
        "fields": {
          "Damage": 42,
          "DisplayName": "Sword"
        }
      },
      {
        "action": "update",
        "row_name": "Sword",
        "fields": {
          "Damage": 55
        }
      }
    ]
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

预期:

- Preview 通过，TaskPlan 包含 `data_table` capability。
- Execute 通过。
- Result journal status completed。
- Read-back 显示 row `Sword` 存在，`Damage=55`，`DisplayName=Sword`。
- 不要求 Blueprint compile。

### 9.3 Object property

TaskSpec 关键字段:

```json
{
  "task_type": "edit_object_properties",
  "target": {
    "asset_path": "${SMOKE_ROOT}/DA_CliAgentData",
    "target_type": "asset"
  },
  "behavior": {
    "property_strategy": "property_edit",
    "changes": [
      {
        "kind": "set_object_property",
        "property_path": "DisplayName",
        "value": "CLI Agent Data"
      }
    ]
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

预期:

- Preview 通过或返回 property 不存在的明确 issue。
- 如果 preview 通过，execute 后 read-back 显示 property 已更新。
- DataAsset instance 不要求 Blueprint compile。
- Invalid value 用例必须 preview blocked，不能 execute 后破坏资产。

## 10. Ownership 与 Composite 用例

### 10.1 Convert owned block to user-owned

TaskSpec 关键字段:

```json
{
  "task_type": "manage_blueprinthelper_ownership",
  "behavior": {
    "ownership_strategy": "owned_block_lifecycle",
    "changes": [
      {
        "kind": "convert_block_to_user_owned",
        "block_id": "<ANCHOR_BLOCK_ID>",
        "missing_policy": "error",
        "already_user_owned_policy": "ignore"
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

预期:

- Preview 通过，TaskPlan 包含 `graph_cleanup_ownership` capability。
- Execute 通过。
- Read-back 证明该 block 不再可作为 BlueprintHelper-owned patch/merge target。
- 再次对该 block 执行 patch preview 必须 blocked，`issues[]` 非空，message 指出 ownership 不允许。

### 10.2 Rollback cleanup transaction

仅当 10.1 journal 返回可回滚 `transaction_id` 时执行。

TaskSpec 关键字段:

```json
{
  "task_type": "manage_blueprinthelper_ownership",
  "behavior": {
    "ownership_strategy": "owned_block_lifecycle",
    "changes": [
      {
        "kind": "rollback_cleanup_transaction",
        "transaction_id": "<TRANSACTION_ID>",
        "already_rolled_back_policy": "ignore"
      }
    ]
  }
}
```

预期:

- Preview 通过或返回 `rollback_unavailable` 等明确 issue。
- 成功 execute 后 read-back 显示 ownership 恢复或 transaction 已回滚。
- 不允许静默 no-op。

### 10.3 Composite feature

TaskSpec 至少包含:

```json
{
  "task_type": "create_blueprint_feature",
  "components": [
    {
      "name": "CompositeScene",
      "class": "SceneComponent",
      "set_as_root": false
    }
  ],
  "variables": [
    {
      "name": "CompositeCounter",
      "type": "int",
      "default": 1
    }
  ],
  "class_settings": {
    "class_defaults": {
      "bCanBeDamaged": true
    }
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "BH_CliAgent_CompositeEvent",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": []
        }
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

预期:

- Preview 通过或 blocked 且 `issues[]` 非空。
- 成功时一个 execute 只产生一个 `task_run_id`。
- Journal steps 覆盖 preview 接受的 capability slice。
- Read-back 分别证明 component、variable、class default、custom event 已落盘。
- 不允许出现空 message，例如 `preview_task failed: , modified=false.`。

## 11. Debug case 摘要

### 11.1 读取 DebugCase summary

前置:

- 从失败 preview、Review reject needs_action、reject_failed 或 UE 自动化中取得 `_DEBUG_CASE_ID_`。

命令:

```powershell
bh blueprinthelper_get_debug_case --json "{ \"debug_case_id\": \"_DEBUG_CASE_ID_\" }" --select status,artifacts.full_result
```

预期 artifact 关键字段:

```json
{
  "toolResult": {
    "operation": "get_debug_case",
    "status": "completed",
    "modified": false,
    "data": {
      "debug_case": {
        "schema": "BlueprintHelper.DebugCaseSummary.v1",
        "debug_case_id": "_DEBUG_CASE_ID_",
        "asset_paths": "<array>",
        "operation": "<non_empty_string>",
        "source": "<non_empty_string>",
        "event_count": "<number>"
      }
    }
  }
}
```

通过标准:

- 只返回 summary，不返回 DebugBundle artifact 内容。
- 不返回本地绝对路径、raw payload、token、settings、source file 内容。
- 如有关联 Review，`data.debug_case.review_record_ids[]` 包含对应 record id，`data.debug_case.transaction_links[]` 包含 role/source 摘要。

### 11.2 DebugCase 缺失

命令:

```powershell
bh blueprinthelper_get_debug_case --json "{ \"debug_case_id\": \"missing_${RUN_ID}\" }" --select status,error_code,message,artifacts.full_result
```

预期:

```json
{
  "status": "failed",
  "error_code": "<non_empty_code>",
  "message": "<non_empty_message>",
  "artifacts": {
    "full_result": "<non_empty_json_path>"
  }
}
```

通过标准:

- Exit code 非 0。
- `modified=false`。
- message 明确说明 case 不存在或不可读。

## 12. 负向与安全用例

### 12.1 缺失 target asset

TaskSpec:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "edit_blueprint_graph",
    "target": {
      "asset_path": "${SMOKE_ROOT}/BP_Missing",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "graph_name": "EventGraph",
      "allow_modify_user_nodes": false
    },
    "behavior": {
      "graph_strategy": "append_new_owned_graph",
      "entries": [
        {
          "entry_type": "custom_event",
          "name": "ShouldNotCreate",
          "body": {
            "schema": "BlueprintLogicSpec.v1",
            "statements": []
          }
        }
      ]
    },
    "validation": {
      "should_compile": true,
      "should_save": false
    }
  }
}
```

预期:

- Preview `status=preview_blocked`。
- `summary.errors>=1`。
- artifact `issues[]` 非空。
- Issue code 是 `target_blueprint_not_found`、`asset_not_found` 或当前实现等价码。
- 不允许 execute。

### 12.2 旧字段 `validation.compile/save`

TaskSpec 中故意写:

```json
{
  "validation": {
    "compile": true,
    "save": true
  }
}
```

预期:

- CLI parse/schema 阶段失败。
- Exit code 非 0。
- stdout `status=cli_error` 或 artifact `toolResult.error.code` 非空。
- message 包含 `should_compile / should_save`。
- 不创建 UE 写入。

### 12.3 `TaskSpec.intent` 禁止

TaskSpec root 中故意加入:

```json
{
  "intent": "make a door"
}
```

预期:

- CLI schema 阶段失败。
- message 包含 `TaskSpec.intent is compiler/runtime generated` 或等价说明。
- 不调用 UE 写入。

### 12.4 Branch fork 缺少 `sequence_order`

TaskSpec 中 `merge_owned_graph.branch_fork` 删除 `sequence_order`。

预期:

- Preview/schema blocked。
- `issues[].path` 指向 `sequence_order`。
- message 包含 `branch_fork requires sequence_order` 或等价说明。
- 不允许 execute。

### 12.5 owned_block_call 缺少 `inserted.block_id`

TaskSpec 中:

```json
{
  "scope": "owned_block_call",
  "inserted": {
    "call_kind": "owned_block_call"
  }
}
```

预期:

- Preview/schema blocked。
- `issues[].path` 指向 `inserted.block_id`。
- message 包含 `owned_block_call requires inserted.block_id` 或等价说明。

### 12.6 remove_signature 没有引用分析策略

TaskSpec 中:

```json
{
  "kind": "remove_signature",
  "function_name": "CliAgent_Calc"
}
```

预期:

- Preview blocked。
- `issues[]` 非空。
- issue message 指出 remove 需要 reference context 或当前实现仍 blocked。
- 不允许 execute 删除。

### 12.7 Bridge/Editor 不可达

操作:

- 关闭 Editor 或设置错误 `BRIDGE_PORT`。
- 运行 `bh blueprint_get_runtime_profile --json "{}"` 或任意 UE read tool。

预期:

```json
{
  "ok": false,
  "schema": "BlueprintHelper.CliResult.v1",
  "operation": "tool.invoke",
  "status": "bridge_unavailable",
  "message": "<contains connection/refused/timeout>"
}
```

通过标准:

- Exit code 非 0。
- 错误信息能定位连接问题。
- 普通 Agent 不改用 Saved cache、`.vs` cache 或本地 JSON export。

### 12.8 Execute 前 preview 被阻塞

操作:

- 对 12.1 的 blocked TaskSpec 强行运行 execute。

预期:

- Execute stdout `status=execute_failed`。
- artifact `toolResult.operation=execute_task`。
- artifact `toolResult.error.code=task_preview_blocked`。
- `modified=false`。
- UE 资产无变化。

## 13. CLI 与 UE 接通判定矩阵

| 区域 | 必测命令 | 通过字段 | UE read-back |
|---|---|---|---|
| CLI 启动 | `bh --help` | exit `0` | 不需要 |
| Direct tool registry | `blueprint_get_runtime_profile` | `status=completed` | `RuntimeProfile.v1` |
| 旧入口隔离 | 冻结工具名 | exit `64` | UE 无副作用 |
| Static diagnostics | `blueprinthelper_diagnostics` | `Diagnostics.v1 mode=static` | 不需要 |
| Runtime diagnostics | `blueprinthelper_diagnostics_runtime` | `Diagnostics.v1 mode=runtime` | Bridge reachable |
| Task context | `blueprinthelper_read_task_context` | `TaskContextPack.v1` | target exists 或明确 false |
| Read context | `blueprinthelper_read_context` | `ReadContextPack.v1` | logic summary/json/md |
| Reference context | `blueprinthelper_read_reference_context` | `read_reference_context` completed | reference summary |
| Write auth | `blueprinthelper_request_write_session` | `WriteSession.v1` sanitized | 后续 execute 可用 |
| Preview | `blueprinthelper_preview_task` | `TaskPreview.v1 passed=true` | dry-run no write |
| Execute | `blueprinthelper_execute_task` | `TaskExecution.v1 task_run_id` | asset changed |
| Journal | `blueprinthelper_get_task_result` | `TaskRunJournal.v1` | steps match |
| Debug summary | `blueprinthelper_get_debug_case` | `DebugCaseSummary.v1` | no raw bundle |

## 14. 最终报告模板

```text
BlueprintHelper CLI Ordinary Agent Test:
Date:
RUN_ID:
Project:
Engine:
Plugin branch/commit:
CLI build path:

CLI entry:
- bh --help:
- direct runtime profile:
- frozen tool rejection:
- field projection:

Read-only preflight:
- static diagnostics:
- agent guide:
- runtime profile:
- runtime diagnostics:
- task context:
- read context summary/json/md:
- reference context:

Write authorization:
- preview before auth:
- request write session:
- token/session leakage check:

Writes:
- create Actor BP:
- create BPI:
- create Struct:
- create DataTable:
- create WidgetBlueprint:
- create DataAsset class:
- create DataAsset instance:
- components:
- variables:
- signature:
- class settings:
- graph append:
- graph replace:
- graph patch:
- graph merge branch_fork:
- UMG:
- DataTable rows:
- object property:
- ownership lifecycle:
- composite feature:

Negative:
- missing target:
- invalid validation fields:
- forbidden intent:
- branch_fork missing sequence_order:
- owned_block_call missing block_id:
- remove_signature blocked:
- bridge unavailable:
- execute after blocked preview:

Task run ids:
Preview ids:
Debug case ids:
Artifact root:
Read-back evidence:

Pass:
Partial:
Fail:
Known gaps:
Blocking bugs:
Next fixes:
```

最终通过标准:

- 所有普通 Agent 可用能力都能通过 CLI direct tool invocation 触达。
- 每个写入用例都有 preview、execute、get result、read-back 四类证据。
- 每个返回字段都有明确 expected schema/status/id/path。
- 失败用例返回可诊断 `code/path/message`，且无 UE 写入副作用。
- 未出现 token/session/raw payload/local bundle path 泄漏。
- 未使用任何非 CLI 入口作为替代路径。
