# GraphWrite Four Cluster End-to-End Smoke Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run a representative end-to-end smoke gate across `FunctionActionCluster`, `FieldVariableActionCluster`, `EventDelegateActionCluster`, and `GenericAssetStructControlActionCluster`, proving TaskSpec -> CLI/Bridge preview -> execute -> readback/compile evidence instead of only C++ automation evidence.

**Architecture:** Smoke uses ordinary BlueprintHelper TaskSpec and CLI paths for asset setup and graph writes. Editor lifecycle remains outside CLI and should use the global BlueprintHelper lifecycle tool when available; asset reads/writes stay on CLI. The smoke must distinguish successful end-to-end coverage from controlled missing-context diagnostics and must not treat deterministic diagnostics as full success.

**Tech Stack:** UE 5.6, BlueprintHelper CLI, AgentFace task-core TypeScript/Python compiler, Bridge preview/execute, GraphWrite SemanticIR, ActionContextPipeline, four GraphWrite spawner clusters, Unreal Automation Tests, Markdown evidence records.

---

## Scope And Pass Criteria

This is not a capability implementation plan. It is an execution plan for a unified smoke gate after the 2026-05-24 Function, Field, Event, Generic Create, and Generic Convert/Schedule convergence work.

The smoke is complete only when all of these are true:

1. A stable fixture Blueprint exists at `/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524`.
2. Each positive TaskSpec preview succeeds and each positive execute succeeds.
3. Each readback proves the expected graph/event/body nodes exist in the target asset.
4. Expected diagnostic rows return a precise missing-context or unsupported-evidence code and do not write assets.
5. `BlueprintHelper.GraphWrite` automation still passes after the CLI smoke.
6. UE 5.6 `TemplateEditor Win64 Development` still builds.
7. The smoke record updates completion/gap docs with exact artifact paths and remaining blockers.

## Current Known Open Items This Smoke Targets

| Area | Current state | Smoke expectation |
|---|---|---|
| Function | C++ automation covers call/op/convert/schedule ownership; shared lifecycle gap remains. | E2E graph write must prove call + op/control branch path still previews/executes through CLI. |
| Field | C++ and compiler coverage exist; previous CLI execute was blocked because `/Game/BlueprintHelper/Smoke/BP_GraphWriteFunctionFieldSmoke` was missing. | This smoke creates a new stable fixture first, then runs real Field preview/execute/readback. |
| Event | GraphWrite must keep declaration lifecycle Signature-owned and only write body/use-site. | TaskPlan must contain Signature step before dependent GraphWrite body step; execute/readback must prove body/use-site exists. |
| Generic | Create and Convert/Schedule boundaries are connected; some operations intentionally require more evidence. | Positive Generic create/control/struct/cast rows pass where supported; type-promotion/timer/latent rows return deterministic diagnostics when evidence is missing. |

## File Structure

Create these smoke artifacts:

- Create directory: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/00_create_smoke_actor.json`
  - Ensures the smoke Actor Blueprint asset exists.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/01_prepare_smoke_fixture.json`
  - Adds smoke components, member variables, and a minimal first graph using `create_blueprint_feature`.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/02_function_field_graph.json`
  - Covers FunctionAction and FieldVariable in one graph body.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/03_event_delegate_graph.json`
  - Covers Signature-owned custom event declaration plus GraphWrite EventDelegate use-site.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/04_generic_graph.json`
  - Covers Generic control, construct, create, and cast paths.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/05_generic_expected_diagnostics.json`
  - Covers Generic type-promotion / schedule missing-evidence diagnostics without treating them as success paths.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/run_four_cluster_smoke.ps1`
  - Runs preview/execute/readback and writes stdout artifacts.
- Create after run: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md`
  - Records exact command outputs, artifact paths, pass/fail counts, and blockers.
- Create/update during run: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_FourClusterE2ESmoke_Bugs_20260524_CN.md`
  - Records every preview/execute/readback/automation/build bug or blocker found during this smoke. Terminal-only issue notes are not sufficient.

Modify after run:

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
  - Add one dated four-cluster E2E smoke row.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
  - Close only the smoke-specific gaps that actually pass; keep precise blockers open.

Do not modify C++ or AgentFace source during this smoke plan unless the smoke exposes a real defect. If it exposes a defect, stop, write the blocker into `SmokeRecord_20260524_CN.md`, and create a separate implementation plan for the fix.

Every blocker or bug found while executing this plan must also be written to `BlueprintHelper_GraphWrite_FourClusterE2ESmoke_Bugs_20260524_CN.md` with exact command, artifact path, observed result, expected result, severity, owner area, and next action.

---

## Task 1: Preflight CLI, Editor, And Build Inputs

**Files:**
- Read: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/docs/TaskSpec_CLI_QuickStart.md`
- Read: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_CLI_Tips_20260514_CN.md`

- [ ] **Step 1.1: Confirm working tree scope**

Run:

```powershell
git status --short
```

Expected:

```text
Unrelated dirty files are identified before smoke artifacts are created.
Do not stage or commit anything.
```

- [ ] **Step 1.2: Build current workspace CLI**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli run build
```

Expected:

```text
Exit code 0.
```

- [ ] **Step 1.3: Confirm Bridge is reachable**

Run:

```powershell
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js bridge ping --select status,summary
```

Expected:

```text
status is bridge_available.
```

If Bridge is not reachable, open Unreal Editor using the global BlueprintHelper lifecycle tool if available. Do not use CLI editor lifecycle aliases as the normal Agent path.

---

## Task 2: Write Smoke TaskSpec Artifacts

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/00_create_smoke_actor.json`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/01_prepare_smoke_fixture.json`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/02_function_field_graph.json`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/03_event_delegate_graph.json`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/04_generic_graph.json`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/05_generic_expected_diagnostics.json`

- [ ] **Step 2.1: Create artifact directory**

Run:

```powershell
New-Item -ItemType Directory -Force -Path 'D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524' | Out-Null
```

Expected:

```text
Directory exists.
```

- [ ] **Step 2.2: Write `00_create_smoke_actor.json`**

Create this exact JSON:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_four_cluster_e2e_create_actor_20260524",
  "task_type": "create_asset",
  "feature_name": "GraphWriteFourClusterSmokeActor",
  "target": {
    "asset_path": "/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524",
    "target_type": "blueprint_class"
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
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

- [ ] **Step 2.3: Write `01_prepare_smoke_fixture.json`**

Create this exact JSON:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_four_cluster_e2e_prepare_fixture_20260524",
  "task_type": "create_blueprint_feature",
  "feature_name": "GraphWriteFourClusterFixture",
  "target": {
    "asset_path": "/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "prefer_new_graph": true,
    "graph_name": "EG_FourClusterFixture_20260524",
    "allow_modify_user_nodes": false,
    "allow_create_assets": false
  },
  "asset_policy": {
    "if_target_asset_missing": "fail",
    "if_referenced_asset_missing": "fail",
    "if_component_exists": "reuse_if_type_matches"
  },
  "components": [
    {
      "name": "SceneRoot",
      "class": "SceneComponent",
      "set_as_root": true
    },
    {
      "name": "DoorMesh",
      "class": "StaticMeshComponent",
      "attach_to": "SceneRoot"
    },
    {
      "name": "TriggerBox",
      "class": "BoxComponent",
      "attach_to": "SceneRoot"
    }
  ],
  "variables": [
    {
      "name": "SmokeFloat",
      "type": "float",
      "default": 0.0,
      "category": "GraphWriteSmoke"
    },
    {
      "name": "TargetRoll",
      "type": "float",
      "default": 12.5,
      "category": "GraphWriteSmoke"
    },
    {
      "name": "CachedLabel",
      "type": "string",
      "default": "",
      "category": "GraphWriteSmoke"
    },
    {
      "name": "bSmokeReady",
      "type": "bool",
      "default": false,
      "category": "GraphWriteSmoke"
    }
  ],
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "BH_FourClusterFixtureReady_20260524",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "set",
              "target": "bSmokeReady",
              "value": {
                "kind": "literal",
                "value_type": "bool",
                "value": true
              }
            },
            {
              "kind": "call",
              "target": "PrintString",
              "args": {
                "InString": {
                  "kind": "literal",
                  "value_type": "string",
                  "value": "Four cluster fixture ready"
                }
              }
            }
          ]
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

- [ ] **Step 2.4: Write `02_function_field_graph.json`**

Create this exact JSON:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_four_cluster_e2e_function_field_20260524",
  "task_type": "edit_blueprint_graph",
  "feature_name": "GraphWriteFunctionFieldE2E",
  "target": {
    "asset_path": "/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EG_FourCluster_FunctionField_20260524",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "BH_FunctionFieldSmoke_20260524",
        "body": {
          "schema": "BlueprintLogicSpec.v2",
          "statements": [
            {
              "kind": "set",
              "target": "SmokeFloat",
              "value": {
                "kind": "literal",
                "value_type": "float",
                "value": 3.0
              }
            },
            {
              "kind": "control",
              "control": "branch",
              "condition": {
                "kind": "op",
                "operator": ">",
                "left": {
                  "kind": "get",
                  "target": "SmokeFloat",
                  "type": "float"
                },
                "right": {
                  "kind": "literal",
                  "value_type": "float",
                  "value": 1.0
                }
              },
              "then": [
                {
                  "kind": "call",
                  "target": "PrintString",
                  "args": {
                    "InString": {
                      "kind": "call",
                      "target": "GetDisplayName",
                      "args": {
                        "Object": {
                          "kind": "field",
                          "field_operation": "get",
                          "field_scope": "component_ref",
                          "target": "DoorMesh"
                        }
                      }
                    }
                  }
                }
              ],
              "else": [
                {
                  "kind": "call",
                  "target": "PrintString",
                  "args": {
                    "InString": {
                      "kind": "literal",
                      "value_type": "string",
                      "value": "Function else path"
                    }
                  }
                }
              ]
            },
            {
              "kind": "set_property",
              "target": "DoorMesh",
              "property_path": "RelativeRotation.Roll",
              "value": {
                "kind": "field",
                "field_operation": "get",
                "field_scope": "variable",
                "target": "TargetRoll"
              }
            },
            {
              "kind": "set",
              "target": "SmokeFloat",
              "value": {
                "kind": "field",
                "field_operation": "get",
                "field_scope": "field_access",
                "target": "DoorMesh",
                "property_path": "RelativeRotation.Pitch"
              }
            }
          ]
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

- [ ] **Step 2.5: Write `03_event_delegate_graph.json`**

Create this exact JSON:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_four_cluster_e2e_event_delegate_20260524",
  "task_type": "edit_blueprint_graph",
  "feature_name": "GraphWriteEventDelegateE2E",
  "target": {
    "asset_path": "/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EG_FourCluster_EventDelegate_20260524",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "BH_HandleSmokeOverlap_20260524",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "call",
              "target": "PrintString",
              "args": {
                "InString": {
                  "kind": "literal",
                  "value_type": "string",
                  "value": "Smoke overlap handler"
                }
              }
            }
          ]
        }
      },
      {
        "entry_type": "custom_event",
        "name": "BH_EventDelegateSmoke_20260524",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "component_bound_event",
              "component": "TriggerBox",
              "delegate": "OnComponentBeginOverlap",
              "handler": "BH_HandleSmokeOverlap_20260524"
            },
            {
              "kind": "delegate.bind",
              "target": "TriggerBox",
              "delegate": "OnComponentBeginOverlap",
              "handler": "BH_HandleSmokeOverlap_20260524"
            }
          ]
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

- [ ] **Step 2.6: Write `04_generic_graph.json`**

Create this exact JSON:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_four_cluster_e2e_generic_20260524",
  "task_type": "edit_blueprint_graph",
  "feature_name": "GraphWriteGenericE2E",
  "target": {
    "asset_path": "/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EG_FourCluster_Generic_20260524",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "BH_GenericSmoke_20260524",
        "body": {
          "schema": "BlueprintLogicSpec.v2",
          "statements": [
            {
              "kind": "control",
              "control": "branch",
              "condition": {
                "kind": "literal",
                "value_type": "bool",
                "value": true
              },
              "then": [
                {
                  "kind": "call",
                  "target": "SetActorLocation",
                  "args": {
                    "NewLocation": {
                      "kind": "construct",
                      "type": "Vector",
                      "fields": {
                        "X": 10.0,
                        "Y": 20.0,
                        "Z": 30.0
                      }
                    },
                    "bSweep": false,
                    "Teleport": false
                  }
                }
              ],
              "else": [
                {
                  "kind": "call",
                  "target": "PrintString",
                  "args": {
                    "InString": {
                      "kind": "literal",
                      "value_type": "string",
                      "value": "Generic else path"
                    }
                  }
                }
              ]
            },
            {
              "kind": "call",
              "target": "PrintString",
              "args": {
                "InString": {
                  "kind": "call",
                  "target": "GetDisplayName",
                  "args": {
                    "Object": {
                      "kind": "convert",
                      "transform_operation": "dynamic_cast",
                      "target_class_path": "/Script/Engine.Actor",
                      "args": {
                        "value": {
                          "kind": "literal",
                          "value_type": "object",
                          "value": "Self"
                        }
                      }
                    }
                  }
                }
              }
            },
            {
              "kind": "create",
              "create_operation": "make_array",
              "pin_type": {
                "category": "string"
              },
              "args": {
                "item": {
                  "kind": "literal",
                  "value_type": "string",
                  "value": "Generic create make_array smoke"
                }
              }
            }
          ]
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

- [ ] **Step 2.7: Write `05_generic_expected_diagnostics.json`**

Create this exact JSON:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_four_cluster_e2e_generic_expected_diagnostics_20260524",
  "task_type": "edit_blueprint_graph",
  "feature_name": "GraphWriteGenericExpectedDiagnostics",
  "target": {
    "asset_path": "/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EG_FourCluster_GenericDiagnostics_20260524",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "BH_GenericExpectedDiagnostics_20260524",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "convert",
              "transform_operation": "type_promotion",
              "args": {
                "value": {
                  "kind": "literal",
                  "value_type": "float",
                  "value": 1.5
                }
              }
            },
            {
              "kind": "schedule",
              "schedule_operation": "timer_delegate_node",
              "graph_latent_allowed": true,
              "args": {
                "delay": {
                  "kind": "literal",
                  "value_type": "float",
                  "value": 0.25
                }
              }
            }
          ]
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

Expected diagnostic behavior:

```text
Preview or execute must not report success for type_promotion or timer_delegate_node unless real projected spawner evidence exists.
Acceptable blocker code includes needs_more_semantic_context with message text naming type_promotion, timer_delegate_node, schedule_operation, or projected spawner evidence.
Any fallback success through FunctionAction, struct resolver, parsed-node mutation, or unsupported legacy path is a smoke failure.
```

---

## Task 3: Create Smoke Runner Script

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/run_four_cluster_smoke.ps1`

- [ ] **Step 3.1: Write runner script**

Create this exact PowerShell script:

```powershell
$ErrorActionPreference = 'Stop'

$Root = 'D:\UEProjects\Template\Plugins\BlueprintHelper'
$Cli = Join-Path $Root 'AgentFaceService\cli\build\cli\index.js'
$ArtifactRoot = Join-Path $Root 'BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524'
$ResultRoot = Join-Path $ArtifactRoot 'Results'

New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$PositiveSpecs = @(
  '00_create_smoke_actor.json',
  '01_prepare_smoke_fixture.json',
  '02_function_field_graph.json',
  '03_event_delegate_graph.json',
  '04_generic_graph.json'
)

$DiagnosticSpecs = @(
  '05_generic_expected_diagnostics.json'
)

function Invoke-TaskCommand {
  param(
    [Parameter(Mandatory = $true)][string]$Mode,
    [Parameter(Mandatory = $true)][string]$SpecName,
    [Parameter(Mandatory = $true)][string]$OutputPath
  )

  $SpecPath = Join-Path $ArtifactRoot $SpecName
  $Output = & node $Cli task $Mode --file $SpecPath --format full
  [System.IO.File]::WriteAllText($OutputPath, ($Output -join [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
  $Json = ($Output -join [Environment]::NewLine) | ConvertFrom-Json
  return $Json
}

function Test-PreviewPassed {
  param([Parameter(Mandatory = $true)]$Result)
  return $Result.ok -eq $true -and $Result.status -eq 'preview_passed'
}

function Test-ExecutePassed {
  param([Parameter(Mandatory = $true)]$Result)
  return $Result.ok -eq $true -and $Result.status -eq 'executed'
}

foreach ($SpecName in $PositiveSpecs) {
  $PreviewPath = Join-Path $ResultRoot ($SpecName + '.preview.json')
  $Preview = Invoke-TaskCommand -Mode 'preview' -SpecName $SpecName -OutputPath $PreviewPath
  if (-not (Test-PreviewPassed -Result $Preview)) {
    throw "Preview failed for $SpecName. See $PreviewPath"
  }

  $ExecutePath = Join-Path $ResultRoot ($SpecName + '.execute.json')
  $Execute = Invoke-TaskCommand -Mode 'execute' -SpecName $SpecName -OutputPath $ExecutePath
  if (-not (Test-ExecutePassed -Result $Execute)) {
    throw "Execute failed for $SpecName. See $ExecutePath"
  }
}

foreach ($SpecName in $DiagnosticSpecs) {
  $PreviewPath = Join-Path $ResultRoot ($SpecName + '.preview.json')
  $Preview = Invoke-TaskCommand -Mode 'preview' -SpecName $SpecName -OutputPath $PreviewPath
  if (Test-PreviewPassed -Result $Preview) {
    throw "Expected diagnostic preview did not block for $SpecName. See $PreviewPath"
  }
  $DiagnosticText = (($Preview | ConvertTo-Json -Depth 80) -join [Environment]::NewLine)
  if ($DiagnosticText -notmatch 'needs_more_semantic_context|type_promotion|timer_delegate_node|schedule_operation|spawner evidence') {
    throw "Expected diagnostic preview blocked with the wrong reason for $SpecName. See $PreviewPath"
  }
}

$ReadSpecPath = Join-Path $ArtifactRoot 'read_logic_flow.json'
$ReadSpec = @{
  schema = 'BlueprintHelper.ReadSpec.v1'
  read_type = 'blueprint_logic'
  target = @{
    asset_path = '/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524'
    target_type = 'custom_event'
    target_name = 'BH_GenericSmoke_20260524'
  }
  view = @{
    format = 'logic_flow'
  }
} | ConvertTo-Json -Depth 80
[System.IO.File]::WriteAllText($ReadSpecPath, $ReadSpec, [System.Text.UTF8Encoding]::new($false))

$ReadOutput = & node $Cli blueprinthelper_read_context --file $ReadSpecPath --format full
$ReadOutputPath = Join-Path $ResultRoot 'read_logic_flow.json'
[System.IO.File]::WriteAllText($ReadOutputPath, ($ReadOutput -join [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))

$AutomationReport = 'D:\UEProjects\Template\Saved\Automation\GraphWrite_FourClusterE2ESmoke_20260524_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath=$AutomationReport
if ($LASTEXITCODE -ne 0) {
  throw "BlueprintHelper.GraphWrite automation failed. See $AutomationReport"
}

& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
if ($LASTEXITCODE -ne 0) {
  throw "UE 5.6 build failed."
}

Write-Output "Four-cluster E2E smoke finished. Results: $ResultRoot"
```

- [ ] **Step 3.2: Run the runner**

Run:

```powershell
& 'D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\run_four_cluster_smoke.ps1'
```

Expected:

```text
Positive specs preview with status=preview_passed and execute with status=executed.
Diagnostic spec preview blocks with needs_more_semantic_context or exact projected-evidence diagnostic.
GraphWrite automation report exists under D:\UEProjects\Template\Saved\Automation\GraphWrite_FourClusterE2ESmoke_20260524_001.
UE 5.6 build returns Result: Succeeded.
```

---

## Task 4: Inspect Evidence And Classify Results

**Files:**
- Read: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/Results/*.json`
- Read: `D:/UEProjects/Template/Saved/Automation/GraphWrite_FourClusterE2ESmoke_20260524_001/index.json`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md`

- [ ] **Step 4.1: Verify positive result files**

Run:

```powershell
$ResultRoot = 'D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\Results'
Get-ChildItem $ResultRoot -Filter '*.execute.json' | ForEach-Object {
  $json = Get-Content -Raw -Encoding UTF8 $_.FullName | ConvertFrom-Json
  [PSCustomObject]@{
    File = $_.Name
    Status = $json.status
    Summary = $json.summary
  }
}
```

Expected:

```text
00_create_smoke_actor.json.execute.json status=executed
01_prepare_smoke_fixture.json.execute.json status=executed
02_function_field_graph.json.execute.json status=executed
03_event_delegate_graph.json.execute.json status=executed
04_generic_graph.json.execute.json status=executed
```

- [ ] **Step 4.2: Verify Event boundary in preview artifacts**

Run:

```powershell
$Preview = Get-Content -Raw -Encoding UTF8 'D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\Results\03_event_delegate_graph.json.preview.json'
$Preview -match 'blueprint_signature'
$Preview -match 'graph_write'
$Preview -match 'depends_on'
$Preview -match 'ensure_custom_event'
```

Expected:

```text
All four expressions print True.
GraphWrite owns body/use-site; Signature owns custom event declaration.
```

- [ ] **Step 4.3: Verify diagnostic artifact**

Run:

```powershell
$Diagnostic = Get-Content -Raw -Encoding UTF8 'D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\Results\05_generic_expected_diagnostics.json.preview.json'
$Diagnostic -match 'needs_more_semantic_context|type_promotion|timer_delegate_node|schedule_operation|spawner evidence'
$Diagnostic -match 'unsupported_function_cluster_semantic|unsupported_generic_cluster_semantic|parsed_node_plan_unsupported'
```

Expected:

```text
First expression prints True.
Second expression prints False.
```

- [ ] **Step 4.4: Verify automation counts**

Run:

```powershell
$Report = Get-Content -Raw -Encoding UTF8 'D:\UEProjects\Template\Saved\Automation\GraphWrite_FourClusterE2ESmoke_20260524_001\index.json' | ConvertFrom-Json
[PSCustomObject]@{
  Succeeded = $Report.succeeded
  SucceededWithWarnings = $Report.succeededWithWarnings
  Failed = $Report.failed
  NotRun = $Report.notRun
}
```

Expected:

```text
Failed = 0
NotRun = 0
Warnings are allowed only if they are existing non-fatal load/network warnings and not four-cluster smoke assertions.
```

- [ ] **Step 4.5: Generate smoke record from artifacts**

Run this script after Steps 4.1-4.4:

```powershell
$ArtifactRoot = 'D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524'
$ResultRoot = Join-Path $ArtifactRoot 'Results'
$RecordPath = Join-Path $ArtifactRoot 'SmokeRecord_20260524_CN.md'
$AutomationPath = 'D:\UEProjects\Template\Saved\Automation\GraphWrite_FourClusterE2ESmoke_20260524_001\index.json'

$Rows = @(
  @{ Spec = '02_function_field_graph.json'; Evidence = 'Function call/op plus Field variable/property/component_ref/field_access' },
  @{ Spec = '03_event_delegate_graph.json'; Evidence = 'Signature declaration plus GraphWrite body/use-site' },
  @{ Spec = '04_generic_graph.json'; Evidence = 'Generic branch/construct/create/dynamic_cast' }
)

$PositiveTable = foreach ($Row in $Rows) {
  $Preview = Get-Content -Raw -Encoding UTF8 (Join-Path $ResultRoot ($Row.Spec + '.preview.json')) | ConvertFrom-Json
  $Execute = Get-Content -Raw -Encoding UTF8 (Join-Path $ResultRoot ($Row.Spec + '.execute.json')) | ConvertFrom-Json
  "| `$($Row.Spec)` | $($Preview.status) | $($Execute.status) | $($Row.Evidence) |"
}

$Diagnostic = Get-Content -Raw -Encoding UTF8 (Join-Path $ResultRoot '05_generic_expected_diagnostics.json.preview.json') | ConvertFrom-Json
$DiagnosticJson = $Diagnostic | ConvertTo-Json -Depth 80
$DiagnosticReason = if ($DiagnosticJson -match 'needs_more_semantic_context') { 'needs_more_semantic_context' } elseif ($DiagnosticJson -match 'type_promotion|timer_delegate_node|schedule_operation|spawner evidence') { 'projected evidence diagnostic' } else { 'unexpected diagnostic reason' }

$Automation = Get-Content -Raw -Encoding UTF8 $AutomationPath | ConvertFrom-Json
$AllPositivePassed = -not ($PositiveTable | Where-Object { $_ -notmatch '\| preview_passed \| executed \|' })
$DiagnosticAccepted = $DiagnosticReason -ne 'unexpected diagnostic reason'
$AutomationPassed = ($Automation.failed -eq 0 -and $Automation.notRun -eq 0)
$OverallStatus = if ($AllPositivePassed -and $DiagnosticAccepted -and $AutomationPassed) { 'PASS' } else { 'BLOCKED' }

$Record = @(
  '# GraphWrite Four Cluster E2E Smoke Record 2026-05-24'
  ''
  '## Result'
  ''
  "Status: $OverallStatus"
  ''
  '## Fixture'
  ''
  '- Asset: `/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524`'
  '- Setup specs: `00_create_smoke_actor.json`, `01_prepare_smoke_fixture.json`'
  ''
  '## Positive Runs'
  ''
  '| Spec | Preview | Execute | Evidence |'
  '|---|---|---|---|'
  $PositiveTable
  ''
  '## Expected Diagnostics'
  ''
  '| Spec | Preview status | Accepted reason |'
  '|---|---|---|'
  "| `05_generic_expected_diagnostics.json` | $($Diagnostic.status) | $DiagnosticReason |"
  ''
  '## Automation And Build'
  ''
  "- `BlueprintHelper.GraphWrite`: succeeded=$($Automation.succeeded), succeededWithWarnings=$($Automation.succeededWithWarnings), failed=$($Automation.failed), notRun=$($Automation.notRun)."
  '- UE 5.6 build: see runner output; `Result: Succeeded` is required for PASS.'
  ''
  '## Remaining Blockers'
  ''
  '- If Status is BLOCKED, list exact artifact file and error code before updating completion docs.'
) -join [Environment]::NewLine

[System.IO.File]::WriteAllText($RecordPath, $Record, [System.Text.UTF8Encoding]::new($false))
Get-Content -Encoding UTF8 $RecordPath
```

Expected:

```text
SmokeRecord_20260524_CN.md exists and contains concrete statuses read from result artifacts.
```

---

## Task 5: Update Completion And Gap Documents

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`

- [ ] **Step 5.1: Update completion status with exact smoke row**

Append one dated section to the four-cluster completion status document. The section must use the concrete values from `SmokeRecord_20260524_CN.md` and `GraphWrite_FourClusterE2ESmoke_20260524_001/index.json`; every result cell must contain a real observed status and artifact path.

Required section shape:

```markdown
## 10. 2026-05-24 Four Cluster End-to-End Smoke

| 验证 | 结果 | 影响 |
|---|---|---|
| Fixture setup | concrete result from `00_create_smoke_actor.json` and `01_prepare_smoke_fixture.json` | `/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524` fixture status and artifact paths. |
| Function + Field E2E | concrete preview/execute result from `02_function_field_graph.json` | Function call/op plus Field variable/property/component_ref/field_access status and artifact paths. |
| EventDelegate E2E | concrete preview/execute result from `03_event_delegate_graph.json` | Signature declaration and GraphWrite body/use-site status and artifact paths. |
| Generic E2E | concrete preview/execute/diagnostic result from `04_generic_graph.json` and `05_generic_expected_diagnostics.json` | Generic branch/construct/create/dynamic_cast status plus expected missing-context diagnostic status. |
| Full GraphWrite automation | exact succeeded/warnings/failed/notRun counts | `Saved/Automation/GraphWrite_FourClusterE2ESmoke_20260524_001/index.json`. |
```

- [ ] **Step 5.2: Update gap audit only for proven closures**

Apply these rules:

```text
If Field positive execute/readback passes, close Gap 4 Field real Bridge execute smoke and cite SmokeRecord.
If Event positive execute/readback passes, narrow Gap 5 to any still-missing override/native or existing-use-site coverage, and cite SmokeRecord.
If Generic expected diagnostics pass, do not mark type-promotion/timer/latent success complete; record that fake success is guarded.
If any positive smoke blocks, keep the gap open and record exact artifact path and error code.
```

Do not mark a cluster `完全完成` unless all known semantic rows for that cluster have success or accepted diagnostic evidence and no open architecture gap remains.

---

## Task 6: Final Verification Gate

**Files:**
- Read: all files under `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/`
- Read: `D:/UEProjects/Template/Saved/Automation/GraphWrite_FourClusterE2ESmoke_20260524_001/index.json`

- [ ] **Step 6.1: Run whitespace and status checks**

Run:

```powershell
git diff --check
git status --short
```

Expected:

```text
git diff --check exits 0.
git status shows only smoke artifacts and intentional doc updates plus any pre-existing unrelated dirty files.
```

- [ ] **Step 6.2: Re-run AgentFace tests if any TaskSpec or CLI code changed**

Run only if source code changed during smoke investigation:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test
```

Expected:

```text
Node and Python tests pass.
```

- [ ] **Step 6.3: Produce final report without committing**

Final report must include:

```text
1. SmokeRecord path.
2. Positive spec preview/execute statuses.
3. Expected diagnostic result.
4. Automation and UE build counts.
5. Remaining blockers with artifact paths.
6. Suggested commit message and exact manual git add command.
```

Do not run `git add`, `git commit`, or `git push`.

## Self-Review Checklist

- [ ] Plan uses the ordinary TaskSpec/CLI path for asset writes.
- [ ] Fixture creation happens before Field/Event/Generic graph writes.
- [ ] Event declaration ownership remains Signature-owned.
- [ ] Generic type-promotion/timer/latent rows are expected diagnostics unless real spawner evidence exists.
- [ ] Full GraphWrite automation and UE 5.6 build remain final gates.
- [ ] Gap docs are updated only for proven closures.
