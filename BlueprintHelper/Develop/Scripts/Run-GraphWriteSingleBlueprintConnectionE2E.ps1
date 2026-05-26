param(
  [string]$PluginRoot = "D:\UEProjects\Template\Plugins\BlueprintHelper",
  [string]$RunId = ("GraphWriteSingleBlueprintConnectionE2E_" + (Get-Date -Format "yyyyMMdd_HHmmss")),
  [string]$AssetPath = "",
  [string]$GraphName = "EG_GraphWriteConnectionE2E",
  [string]$FunctionName = "GWConnFunction"
)

$ErrorActionPreference = "Stop"

$TaskCore = Join-Path $PluginRoot "AgentFaceService\task-core"
$CliPackage = Join-Path $PluginRoot "AgentFaceService\cli"
$Cli = Join-Path $CliPackage "build\cli\index.js"
$OutRoot = Join-Path "D:\UEProjects\Template\Saved\Automation" $RunId
$SpecRoot = Join-Path $OutRoot "specs"
$ResultRoot = Join-Path $OutRoot "results"

if ([string]::IsNullOrWhiteSpace($AssetPath)) {
  $SafeRunId = ($RunId -replace '[^A-Za-z0-9_]', '_')
  $AssetPath = "/Game/BlueprintHelper/ConnectionE2E/BP_GraphWriteConnection_$SafeRunId"
}

New-Item -ItemType Directory -Force -Path $SpecRoot, $ResultRoot | Out-Null

Push-Location $TaskCore
npm.cmd run build
if ($LASTEXITCODE -ne 0) { throw "task-core build failed." }
Pop-Location

Push-Location $CliPackage
npm.cmd run build
if ($LASTEXITCODE -ne 0) { throw "cli build failed." }
Pop-Location

function Write-JsonFile {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)]$Value
  )
  $Json = $Value | ConvertTo-Json -Depth 100
  [System.IO.File]::WriteAllText($Path, $Json, [System.Text.UTF8Encoding]::new($false))
}

function Invoke-CliJson {
  param(
    [Parameter(Mandatory = $true)][string[]]$Arguments,
    [Parameter(Mandatory = $true)][string]$OutputPath
  )

  $Output = & node $Cli @Arguments
  [System.IO.File]::WriteAllText($OutputPath, ($Output -join [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
  return ($Output -join [Environment]::NewLine) | ConvertFrom-Json
}

function Invoke-TaskSpecStep {
  param(
    [Parameter(Mandatory = $true)][string]$StepName,
    [Parameter(Mandatory = $true)]$Spec
  )

  $StepSpecDir = Join-Path $SpecRoot $StepName
  $StepResultDir = Join-Path $ResultRoot $StepName
  New-Item -ItemType Directory -Force -Path $StepSpecDir, $StepResultDir | Out-Null
  $SpecPath = Join-Path $StepSpecDir "task.json"
  Write-JsonFile -Path $SpecPath -Value $Spec

  $Preview = Invoke-CliJson -Arguments @("task", "preview", "--file", $SpecPath, "--format", "json", "--artifact-dir", $StepResultDir) -OutputPath (Join-Path $StepResultDir "preview.json")
  if ($Preview.ok -ne $true -or $Preview.status -ne "preview_passed") {
    throw "Preview failed for $StepName. See $StepResultDir\preview.json"
  }

  $Execute = Invoke-CliJson -Arguments @("task", "execute", "--file", $SpecPath, "--format", "json", "--artifact-dir", $StepResultDir) -OutputPath (Join-Path $StepResultDir "execute.json")
  if ($Execute.ok -ne $true -or $Execute.status -ne "executed") {
    throw "Execute failed for $StepName. See $StepResultDir\execute.json"
  }

  return [pscustomobject]@{
    name = $StepName
    spec = $SpecPath
    result_dir = $StepResultDir
    preview_status = $Preview.status
    execute_status = $Execute.status
  }
}

function Invoke-LogicRead {
  param(
    [Parameter(Mandatory = $true)][string]$StepName,
    [Parameter(Mandatory = $true)][string]$TargetName
  )

  $ReadSpecDir = Join-Path $SpecRoot $StepName
  $ReadResultDir = Join-Path $ResultRoot $StepName
  New-Item -ItemType Directory -Force -Path $ReadSpecDir, $ReadResultDir | Out-Null
  $ReadSpecPath = Join-Path $ReadSpecDir "read.json"
  Write-JsonFile -Path $ReadSpecPath -Value ([ordered]@{
    schema = "BlueprintHelper.ReadSpec.v1"
    read_type = "blueprint_logic"
    target = [ordered]@{
      asset_path = $AssetPath
      target_type = "graph"
      target_name = $TargetName
    }
    view = [ordered]@{
      format = "logic_json"
      max_items = 2000
      detail = "full"
    }
  })

  $Read = Invoke-CliJson -Arguments @("blueprinthelper_read_context", "--file", $ReadSpecPath, "--format", "json", "--artifact-dir", $ReadResultDir) -OutputPath (Join-Path $ReadResultDir "readback.json")
  if ($Read.ok -ne $true -or $Read.status -ne "completed") {
    throw "Readback failed for $StepName. See $ReadResultDir\readback.json"
  }
  return $Read
}

function Measure-LogicReadback {
  param([Parameter(Mandatory = $true)]$Logic)

  $Groups = @($Logic.groups)
  $Nodes = @($Groups | ForEach-Object { @($_.nodes) })
  $Links = @($Nodes | ForEach-Object { @($_.links) })
  return [ordered]@{
    groups = $Groups.Count
    nodes = $Nodes.Count
    exec_links = @($Links | Where-Object { $_.type -eq "exec" }).Count
    data_links = @($Links | Where-Object { $_.type -eq "data" }).Count
    events = @($Groups | Where-Object { $_.entry.kind -eq "custom_event" }).Count
  }
}

function LitString([string]$Value) { return [ordered]@{ kind = "literal"; value_type = "string"; value = $Value } }
function LitNumber([double]$Value) { return [ordered]@{ kind = "literal"; value_type = "number"; value = $Value } }
function LitBool([bool]$Value) { return [ordered]@{ kind = "literal"; value_type = "bool"; value = $Value } }

function PrintStmt([string]$Text) {
  return [ordered]@{
    kind = "call"
    target = "PrintString"
    args = [ordered]@{
      InString = (LitString $Text)
      Duration = (LitNumber 1.0)
    }
  }
}

function TaskBase([string]$ContextId, [string]$TaskType, [string]$FeatureName, [string]$TargetType) {
  return [ordered]@{
    schema = "BlueprintHelper.TaskSpec.v1"
    context_id = $ContextId
    task_type = $TaskType
    feature_name = $FeatureName
    target = [ordered]@{
      asset_path = $AssetPath
      target_type = $TargetType
    }
    execution_policy = [ordered]@{
      dry_run_mode = "full"
      on_missing_capability = "stop_and_report"
      review_baseline_dirty_asset_policy = "save_before_archive"
    }
    validation = [ordered]@{
      should_compile = $true
      should_save = $true
    }
  }
}

$ExecutedSteps = @()

$CreateAsset = TaskBase "ctx_gw_connection_create_asset" "create_asset" "GraphWriteConnectionAsset" "asset"
$CreateAsset.behavior = [ordered]@{
  asset_strategy = "ensure_asset"
  asset = [ordered]@{
    asset_type = "blueprint_class"
    parent_class = "Actor"
    collision_policy = "fail_if_exists"
  }
}
$ExecutedSteps += Invoke-TaskSpecStep "01_create_asset" $CreateAsset

$Fixture = TaskBase "ctx_gw_connection_fixture" "create_blueprint_feature" "GraphWriteConnectionFixture" "blueprint"
$Fixture.scope_policy = [ordered]@{
  prefer_new_graph = $true
  graph_name = "EG_GraphWriteConnectionFixture"
  allow_modify_user_nodes = $false
  allow_create_assets = $false
}
$Fixture.asset_policy = [ordered]@{
  if_target_asset_missing = "fail"
  if_referenced_asset_missing = "fail"
  if_component_exists = "reuse_if_type_matches"
}
$Fixture.components = @(
  [ordered]@{ name = "SceneRoot"; class = "SceneComponent"; set_as_root = $true },
  [ordered]@{ name = "TriggerBox"; class = "BoxComponent"; attach_to = "SceneRoot" }
)
$Fixture.variables = @(
  [ordered]@{ name = "GWGenBool"; type = "bool"; default = $false; category = "GraphWriteConnectionE2E" },
  [ordered]@{ name = "GWGenInt"; type = "int"; default = 0; category = "GraphWriteConnectionE2E" },
  [ordered]@{ name = "GWGenFloat"; type = "float"; default = 0; category = "GraphWriteConnectionE2E" },
  [ordered]@{ name = "GWGenString"; type = "string"; default = ""; category = "GraphWriteConnectionE2E" },
  [ordered]@{ name = "GWGenRotator"; pin_type = [ordered]@{ category = "struct"; object_path = "/Script/CoreUObject.Rotator" }; category = "GraphWriteConnectionE2E" },
  [ordered]@{ name = "GWGenIntArray"; pin_type = [ordered]@{ category = "int"; container_type = "array" }; category = "GraphWriteConnectionE2E" },
  [ordered]@{ name = "GWGenOtherIntArray"; pin_type = [ordered]@{ category = "int"; container_type = "array" }; category = "GraphWriteConnectionE2E" },
  [ordered]@{ name = "GWGenIntSet"; pin_type = [ordered]@{ category = "int"; container_type = "set" }; category = "GraphWriteConnectionE2E" },
  [ordered]@{ name = "GWGenStringIntMap"; pin_type = [ordered]@{ category = "string"; container_type = "map"; value_type = [ordered]@{ category = "int" } }; category = "GraphWriteConnectionE2E" }
)
$ExecutedSteps += Invoke-TaskSpecStep "02_setup_fixture" $Fixture

$Signature = TaskBase "ctx_gw_connection_signature" "edit_blueprint_signature" "GraphWriteConnectionFunctionSignature" "blueprint"
$Signature.behavior = [ordered]@{
  signature_strategy = "signature_edit"
  changes = @(
    [ordered]@{
      kind = "ensure_function"
      function_name = $FunctionName
      inputs = @()
      outputs = @()
      name_collision_policy = "reuse_if_exists"
    }
  )
}
$ExecutedSteps += Invoke-TaskSpecStep "03_ensure_function" $Signature

$Append = TaskBase "ctx_gw_connection_append" "edit_blueprint_graph" "GraphWriteConnectionAppend" "blueprint"
$Append.scope_policy = [ordered]@{ graph_name = $GraphName; allow_modify_user_nodes = $false }
$Append.behavior = [ordered]@{
  graph_strategy = "append_new_owned_graph"
  entries = @(
    [ordered]@{
      entry_type = "custom_event"
      name = "GWConn_EventA"
      body = [ordered]@{
        schema = "BlueprintLogicSpec.v1"
        statements = @(
          (PrintStmt "EventA append body")
        )
      }
    },
    [ordered]@{
      entry_type = "custom_event"
      name = "GWConn_EventB"
      body = [ordered]@{
        schema = "BlueprintLogicSpec.v1"
        statements = @((PrintStmt "EventB initial append body"))
      }
    },
    [ordered]@{
      entry_type = "custom_event"
      name = "GWConn_EventC"
      body = [ordered]@{
        schema = "BlueprintLogicSpec.v1"
        statements = @(
          (PrintStmt "EventC initial append body"),
          [ordered]@{
            kind = "let"
            name = "GWConn_Component"
            value = [ordered]@{
              kind = "field"
              field_operation = "get"
              field_scope = "component_ref"
              target = "TriggerBox"
              context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "field.component_ref" }
            }
          }
        )
      }
    }
  )
}
$ExecutedSteps += Invoke-TaskSpecStep "04_append_three_events" $Append

$ReplaceEventA = TaskBase "ctx_gw_connection_replace_event_a" "edit_blueprint_graph" "GraphWriteConnectionReplaceEventA" "blueprint"
$ReplaceEventA.scope_policy = [ordered]@{ graph_name = $GraphName; allow_modify_user_nodes = $false }
$ReplaceEventA.behavior = [ordered]@{
  graph_strategy = "replace_owned_graph"
  replace = [ordered]@{
    scope = "custom_event_body"
    selector = [ordered]@{ kind = "custom_event"; name = "GWConn_EventA"; graph_id = $GraphName }
    body = [ordered]@{
      schema = "BlueprintLogicSpec.v1"
      statements = @(
        [ordered]@{
          kind = "set"
          target = "GWGenString"
          value = (LitString "EventA field replace")
        },
        [ordered]@{
          kind = "call"
          target = "PrintString"
          args = [ordered]@{
            InString = [ordered]@{
              kind = "field"
              field_operation = "get"
              field_scope = "variable"
              target = "GWGenString"
              context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "field.field_access" }
            }
            Duration = (LitNumber 1.0)
          }
        },
        [ordered]@{
          kind = "let"
          name = "GWConn_Component"
          value = [ordered]@{
            kind = "field"
            field_operation = "get"
            field_scope = "component_ref"
            target = "TriggerBox"
            context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "field.component_ref" }
          }
        },
        [ordered]@{
          kind = "let"
          name = "GWConn_CastSelf"
          value = [ordered]@{
            kind = "convert"
            transform_operation = "dynamic_cast"
            target_class_path = "/Script/Engine.Actor"
            args = [ordered]@{
              value = [ordered]@{ kind = "literal"; value_type = "object"; value = "Self" }
            }
            context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "generic_ops.transform.dynamic_cast" }
          }
        }
      )
    }
    options = [ordered]@{ strict = $true; preserve_layout = $false }
  }
}
$ExecutedSteps += Invoke-TaskSpecStep "05_replace_event_a_body" $ReplaceEventA

$ReplaceFunction = TaskBase "ctx_gw_connection_replace_function" "edit_blueprint_graph" "GraphWriteConnectionReplaceFunction" "blueprint"
$ReplaceFunction.scope_policy = [ordered]@{ graph_name = $FunctionName; allow_modify_user_nodes = $false }
$ReplaceFunction.behavior = [ordered]@{
  graph_strategy = "replace_owned_graph"
  replace = [ordered]@{
    scope = "function_body"
    selector = [ordered]@{ kind = "function"; name = $FunctionName; graph_id = $FunctionName }
    body = [ordered]@{
      schema = "BlueprintLogicSpec.v1"
      statements = @(
        [ordered]@{
          kind = "field"
          field_operation = "set"
          field_scope = "variable"
          target = "GWGenIntArray"
          value = [ordered]@{
            kind = "create"
            create_operation = "make_array"
            pin_type = [ordered]@{ category = "int"; container_type = "array" }
            args = [ordered]@{ value = (LitNumber 5) }
            context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "generic_ops.create.make_array" }
          }
        },
        [ordered]@{
          kind = "call"
          target = "PrintString"
          args = [ordered]@{
            InString = [ordered]@{
              kind = "select"
              condition = (LitBool $true)
              options = @((LitString "Function select A"), (LitString "Function select B"))
              context_evidence = [ordered]@{
                "generic.select.result_type_proof" = "string"
                "graphwrite_connection.cluster" = "generic_ops.struct_select.select"
              }
            }
            Duration = (LitNumber 1.0)
          }
        },
        [ordered]@{
          kind = "call"
          target = "PrintString"
          args = [ordered]@{
            InString = [ordered]@{
              kind = "op"
              op = "string_append"
              left = (LitString "Op")
              right = (LitString "Coverage")
              context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "op_coverage" }
            }
            Duration = (LitNumber 1.0)
          }
        }
      )
    }
    options = [ordered]@{ strict = $true; preserve_layout = $false }
  }
}
$ExecutedSteps += Invoke-TaskSpecStep "06_replace_function_body" $ReplaceFunction

$ReplaceEventB = TaskBase "ctx_gw_connection_replace_event_b" "edit_blueprint_graph" "GraphWriteConnectionReplaceEventB" "blueprint"
$ReplaceEventB.scope_policy = [ordered]@{ graph_name = $GraphName; allow_modify_user_nodes = $false }
$ReplaceEventB.behavior = [ordered]@{
  graph_strategy = "replace_owned_graph"
  replace = [ordered]@{
    scope = "custom_event_body"
    selector = [ordered]@{ kind = "custom_event"; name = "GWConn_EventB"; graph_id = $GraphName }
    body = [ordered]@{
      schema = "BlueprintLogicSpec.v1"
      statements = @(
        [ordered]@{
          kind = "set"
          target = "GWGenString"
          value = (LitString "EventB field set")
        },
        [ordered]@{
          kind = "container_action"
          container_kind = "map"
          container_operation = "add"
          target = [ordered]@{ kind = "get"; name = "GWGenStringIntMap" }
          key_type = "string"
          value_type = "int"
          key = (LitString "EventBKey")
          value = (LitNumber 11)
          context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "container_action.map" }
        },
        [ordered]@{
          kind = "container_action"
          container_kind = "array"
          container_operation = "add"
          target = [ordered]@{ kind = "get"; name = "GWGenIntArray" }
          element_type = "int"
          item = (LitNumber 22)
          context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "container_action.array" }
        },
        [ordered]@{
          kind = "container_action"
          container_kind = "set"
          container_operation = "add"
          target = [ordered]@{ kind = "get"; name = "GWGenIntSet" }
          element_type = "int"
          item = (LitNumber 33)
          context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "container_action.set" }
        },
        [ordered]@{
          kind = "control"
          control = "branch"
          condition = (LitBool $true)
          then = @((PrintStmt "EventB branch then"))
          "else" = @((PrintStmt "EventB branch else"))
          context_evidence = [ordered]@{
            "generic.control.operation" = "branch"
            "graphwrite_connection.cluster" = "generic_ops.control.branch"
          }
        }
      )
    }
    options = [ordered]@{ strict = $true; preserve_layout = $false }
  }
}
$ExecutedSteps += Invoke-TaskSpecStep "07_replace_event_b_body" $ReplaceEventB

$ReplaceEventC = TaskBase "ctx_gw_connection_replace_event_c" "edit_blueprint_graph" "GraphWriteConnectionReplaceEventC" "blueprint"
$ReplaceEventC.scope_policy = [ordered]@{ graph_name = $GraphName; allow_modify_user_nodes = $false }
$ReplaceEventC.behavior = [ordered]@{
  graph_strategy = "replace_owned_graph"
  replace = [ordered]@{
    scope = "custom_event_body"
    selector = [ordered]@{ kind = "custom_event"; name = "GWConn_EventC"; graph_id = $GraphName }
    body = [ordered]@{
      schema = "BlueprintLogicSpec.v1"
      statements = @(
        (PrintStmt "EventC replace body"),
        [ordered]@{
          kind = "control"
          control = "sequence"
          value = (LitString "EventC sequence")
          context_evidence = [ordered]@{
            "generic.control.operation" = "sequence"
            "graphwrite_connection.cluster" = "generic_ops.control.sequence"
          }
        },
        [ordered]@{
          kind = "let"
          name = "GWConn_CastSelfFinal"
          value = [ordered]@{
            kind = "convert"
            transform_operation = "dynamic_cast"
            target_class_path = "/Script/Engine.Actor"
            args = [ordered]@{
              value = [ordered]@{ kind = "literal"; value_type = "object"; value = "Self" }
            }
            context_evidence = [ordered]@{ "graphwrite_connection.cluster" = "generic_ops.transform.dynamic_cast" }
          }
        }
      )
    }
    options = [ordered]@{ strict = $true; preserve_layout = $false }
  }
}
$ExecutedSteps += Invoke-TaskSpecStep "08_replace_event_c_body" $ReplaceEventC

$MainReadBeforeStructural = Invoke-LogicRead "09_read_before_patch_merge" $GraphName
$Groups = $MainReadBeforeStructural.tool_result.data.payload.logic.groups
$AnchorGroup = $Groups | Where-Object { $_.entry.name -eq "GWConn_EventC" } | Select-Object -First 1
if (-not $AnchorGroup) { throw "GWConn_EventC group not found in readback." }
$EntryNode = $AnchorGroup.nodes | Where-Object { $_.node_ref -eq $AnchorGroup.entry.node_ref } | Select-Object -First 1
if (-not $EntryNode) { throw "GWConn_EventC entry node not found in readback." }
$ThenLink = $EntryNode.links | Where-Object { $_.type -eq "exec" -and $_.pin_ref -eq "then" } | Select-Object -First 1
if (-not $ThenLink) { throw "GWConn_EventC then exec link not found in readback." }

$Patch = TaskBase "ctx_gw_connection_patch_event_c" "edit_blueprint_graph" "GraphWriteConnectionPatchEventC" "blueprint"
$Patch.scope_policy = [ordered]@{ graph_name = $GraphName; allow_modify_user_nodes = $false }
$Patch.behavior = [ordered]@{
  graph_strategy = "patch_owned_graph"
  patches = @(
    [ordered]@{
      kind = "set_node_comment"
      target_ref = [ordered]@{
        block_id = $AnchorGroup.block_id
        group_entry_node_path = $AnchorGroup.group_entry_node_path
        node_ref = $EntryNode.node_ref
      }
      value = "GraphWrite single-blueprint E2E patch marker"
    }
  )
}
$ExecutedSteps += Invoke-TaskSpecStep "10_patch_event_c_comment" $Patch

$Merge = TaskBase "ctx_gw_connection_merge_event_c" "edit_blueprint_graph" "GraphWriteConnectionMergeEventC" "blueprint"
$Merge.scope_policy = [ordered]@{ graph_name = $GraphName; allow_modify_user_nodes = $false }
$Merge.behavior = [ordered]@{
  graph_strategy = "merge_owned_graph"
  merges = @(
    [ordered]@{
      kind = "insert_flow"
      scope = "function_call"
      insert_strategy = "insert_between"
      anchor = [ordered]@{
        block_id = $AnchorGroup.block_id
        group_entry_node_path = $AnchorGroup.group_entry_node_path
        node_ref = $EntryNode.node_ref
        pin_ref = $ThenLink.pin_ref
        link_ref = $ThenLink.link_ref
      }
      inserted = [ordered]@{
        call_kind = "function_call"
        name = $FunctionName
      }
    }
  )
}
$ExecutedSteps += Invoke-TaskSpecStep "11_merge_event_c_function_call" $Merge

$FinalMainRead = Invoke-LogicRead "12_final_main_graph_readback" $GraphName
$FinalFunctionRead = Invoke-LogicRead "13_final_function_readback" $FunctionName

$MainLogic = $FinalMainRead.tool_result.data.payload.logic
$FunctionLogic = $FinalFunctionRead.tool_result.data.payload.logic
$MainStats = Measure-LogicReadback $MainLogic
$FunctionStats = Measure-LogicReadback $FunctionLogic
$MainGroups = @($MainLogic.groups)
$FunctionGroups = @($FunctionLogic.groups)
$EventCount = @($MainGroups | Where-Object { $_.entry.kind -eq "custom_event" }).Count
$FunctionGroupCount = @($FunctionGroups | Where-Object { $_.name -eq $FunctionName -or $_.entry.name -eq $FunctionName -or $_.entry.kind -eq "function" }).Count
$TotalNodeCount = [int]$MainStats.nodes + [int]$FunctionStats.nodes
$ExpectedEventNames = @("GWConn_EventA", "GWConn_EventB", "GWConn_EventC")
$EventBodyNodeCounts = [ordered]@{}
foreach ($EventName in $ExpectedEventNames) {
  $EventGroup = $MainGroups | Where-Object { $_.entry.name -eq $EventName } | Select-Object -First 1
  $EventBodyNodeCounts[$EventName] = if ($EventGroup) { @($EventGroup.nodes).Count } else { 0 }
}
$AllEventBodiesPreserved = @($EventBodyNodeCounts.GetEnumerator() | Where-Object { [int]$_.Value -lt 2 }).Count -eq 0
$AllExpectedCounts = ($EventCount -eq 3 -and $FunctionGroupCount -ge 1 -and $TotalNodeCount -ge 12 -and $AllEventBodiesPreserved)

$Summary = [ordered]@{
  schema = "BlueprintHelper.GraphWriteSingleBlueprintConnectionE2E.v1"
  run_id = $RunId
  asset_path = $AssetPath
  graph_name = $GraphName
  function_name = $FunctionName
  ok = $AllExpectedCounts
  strategies = @("append_new_owned_graph", "replace_owned_graph", "patch_owned_graph", "merge_owned_graph")
  executed_representatives = @(
    "function_action.call_function",
    "field.field_access",
    "field.component_ref",
    "event.custom_event",
    "container_action.map",
    "container_action.array",
    "container_action.set",
    "generic_ops.control.branch",
    "generic_ops.transform.dynamic_cast",
    "generic_ops.create.make_array",
    "generic_ops.struct_select.select",
    "op_coverage.string_append"
  )
  final_readback_representatives = @(
    "event.custom_event",
    "function_action.call_function",
    "generic_ops.control.sequence",
    "generic_ops.transform.dynamic_cast",
    "generic_ops.create.make_array",
    "generic_ops.struct_select.select",
    "op_coverage.string_append"
  )
  counts = [ordered]@{
    event_count = $EventCount
    function_group_count = $FunctionGroupCount
    main_graph_nodes = [int]$MainStats.nodes
    function_graph_nodes = [int]$FunctionStats.nodes
    total_nodes = $TotalNodeCount
    main_graph_exec_links = [int]$MainStats.exec_links
    main_graph_data_links = [int]$MainStats.data_links
    function_graph_exec_links = [int]$FunctionStats.exec_links
    function_graph_data_links = [int]$FunctionStats.data_links
    event_body_node_counts = $EventBodyNodeCounts
  }
  structural_anchor = [ordered]@{
    event_name = $AnchorGroup.entry.name
    block_id = $AnchorGroup.block_id
    group_entry_node_path = $AnchorGroup.group_entry_node_path
    node_ref = $EntryNode.node_ref
    pin_ref = $ThenLink.pin_ref
    link_ref = $ThenLink.link_ref
  }
  steps = $ExecutedSteps
  run_root = $OutRoot
}

Write-JsonFile -Path (Join-Path $OutRoot "summary.json") -Value $Summary

if ($AllExpectedCounts -ne $true) {
  throw "GraphWrite single-blueprint connection E2E count gate failed. See $OutRoot\summary.json"
}

Write-Output "GraphWrite single-blueprint connection E2E finished."
Write-Output "Run root: $OutRoot"
Write-Output "Asset path: $AssetPath"
Write-Output "Events: $EventCount"
Write-Output "Function groups: $FunctionGroupCount"
Write-Output "Total nodes: $TotalNodeCount"
Write-Output "Summary: $(Join-Path $OutRoot 'summary.json')"
