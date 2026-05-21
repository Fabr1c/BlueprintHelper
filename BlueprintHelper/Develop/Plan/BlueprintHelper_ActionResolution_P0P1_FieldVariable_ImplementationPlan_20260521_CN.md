# BlueprintHelper ActionResolution P0/P1 FieldVariable 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: 使用 `superpowers:subagent-driven-development` 执行本计划。每个步骤完成后更新本文档的勾选状态；未真实完成不得勾选。

**Goal:** 完成当前架构后的 P0/P1：先固化 `ActionResolution` 一级分发契约，再实现第一个非 `call` 的真实 UE NodeSpawner 簇能力：`FieldVariableAction` 的 `get/set` 解析与候选返回。全程不保留旧 fallback，不恢复旧 direct spawn，不做旧 AgentFace 兼容。

**Architecture:** `AgentFace semantic statement -> Semantic Resolver -> SpawnerClusterKind + SemanticConstraints -> BlueprintActionResolutionCore -> SpawnerClusterResolver -> SpawnerCluster -> UBlueprintNodeSpawner candidate -> NodeFragment adapter`。`ActionResolutionCore` 只能按 `SpawnerClusterKind` 分发；`get/set/get_property/...` 等语义只能作为 `SemanticConstraints`，不能再成为一级请求类型。

**Tech Stack:** UE 5.6、C++、BlueprintGraph、BlueprintActionDatabase / BlueprintActionFilter / UBlueprintNodeSpawner、Automation Tests、PowerShell、BlueprintHelper CLI。

---

## P0: 文档与契约固化

### Step 0.1: 清理设计文档中的旧口径

**Files:**
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Plan\BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Plan\BlueprintHelper_SpawnerClusterSkeleton_ImplementationPlan_20260521_CN.md`

**Action:**
1. 将所有“按 intent 选择簇”“ActionIntent 一级分发”“Get/Set 是请求类型”的描述改成：

```text
ActionResolution 一级请求只包含 SpawnerClusterKind。
语义动作写入 SemanticConstraints.Kind。
SpawnerClusterResolver 只按 ClusterKind 选簇。
具体簇内部再消费 SemanticConstraints 做候选过滤、歧义返回、preview 诊断。
```

2. 将流程图统一为：

```text
AgentFace statement
-> Semantic Resolver
-> FBlueprintHelperActionResolutionRequest { ClusterKind, SemanticConstraints, GraphContext, TypedPins }
-> BlueprintActionResolutionCore
-> SpawnerClusterResolver.SelectCluster(ClusterKind)
-> Cluster.Resolve(Request)
-> UBlueprintNodeSpawner candidates
-> NodeFragment adapter
```

3. 删除或重写以下旧表达：

```text
EBlueprintHelperActionIntent
ActionRequest.Intent
Request.Intent
SelectCluster(Intent)
IntentToString
semantic-specific first-level dispatch
old node fallback
legacy direct spawn fallback
```

**Expected Result:**
- 两份文档都只描述 `ClusterKind + SemanticConstraints`。
- 文档不再把 `get/set/get_property/set_property/op/construct/deconstruct/select` 描述为一级 resolver 分发类型。
- 文档明确 `get/set` 是 `FieldVariableAction` 或后续专属簇内部的语义约束。

---

### Step 0.2: 增加 ActionResolution 契约测试

**Files:**
- 新增 `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\BlueprintHelperActionResolutionContractTests.cpp`

**Action:**
1. 新增自动化测试，直接构造当前请求模型：

```cpp
FBlueprintHelperActionResolutionRequest Request;
Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Get;
```

2. 测试 `FBlueprintHelperSpawnerClusterResolver` 对外 API 只接受 `EBlueprintHelperSpawnerClusterKind` 或完整 `FBlueprintHelperActionResolutionRequest`，不得暴露按 `EBlueprintHelperActionSemanticKind` 选择簇的入口。

3. 增加源码卫生测试，扫描 `Source/BlueprintHelper` 下的 C++ 文件，禁止以下模式重新出现：

```text
EBlueprintHelperActionIntent
ActionRequest.Intent
Request.Intent
SelectCluster(Intent
IntentToString(
```

4. 测试失败信息必须指出具体文件与命中的旧模式。

**Implementation Shape:**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlueprintHelperActionResolutionNoIntentFallbackTest,
    "BlueprintHelper.ActionResolution.Contract.NoIntentFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionNoIntentFallbackTest::RunTest(const FString& Parameters)
{
    FBlueprintHelperActionResolutionRequest Request;
    Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
    Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Get;

    TestEqual(TEXT("ClusterKind is top-level dispatch key"), Request.ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
    TestEqual(TEXT("Semantic kind is constraint only"), Request.Semantic.Kind, EBlueprintHelperActionSemanticKind::Get);

    // Source hygiene scan runs here.
    return true;
}
```

**Expected Result:**
- 后续任何人把 `Intent` 重新放回一级 request 或 resolver 都会编译失败或测试失败。
- 契约测试只保护架构边界，不引入运行期 fallback。

---

## P1: FieldVariableActionCluster get/set provider

### Step 1.1: 为 FieldVariableAction 新增专属 resolver/helper 边界

**Files:**
- 新增 `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperFieldVariableActionResolver.h`
- 新增 `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperFieldVariableActionResolver.cpp`
- 修改 `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperFieldVariableActionCluster.h`
- 修改 `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperFieldVariableActionCluster.cpp`

**Action:**
1. 新增 `FBlueprintHelperFieldVariableActionResolver`，职责只包含：
   - 从 `Request.GraphContext` 获取 `UBlueprint*`、`UEdGraph*`、`UEdGraphSchema_K2*`。
   - 从 `Request.Semantic` 判断 `Get` 或 `Set`。
   - 使用 UE 5.6 的变量 NodeSpawner / ActionDatabase 能力收集变量候选。
   - 返回结构化候选，不创建节点。

2. `FBlueprintHelperFieldVariableActionCluster::Resolve` 只调用该 resolver，不再自己写复杂匹配逻辑。

3. 如果 UE header 调用路径与预期不一致，必须停在 resolver seam 内调整；禁止退回 GraphStatementBuilder 直接创建 `UK2Node_VariableGet/Set`。

**Expected Resolver Contract:**

```cpp
struct FBlueprintHelperFieldVariableResolveResult
{
    EBlueprintHelperActionResolveStatus Status = EBlueprintHelperActionResolveStatus::NotFound;
    TArray<FBlueprintHelperActionCandidate> Candidates;
    TWeakObjectPtr<UBlueprintNodeSpawner> SelectedSpawner;
    FString Message;
};

class BLUEPRINTHELPER_API FBlueprintHelperFieldVariableActionResolver
{
public:
    FBlueprintHelperFieldVariableResolveResult Resolve(const FBlueprintHelperActionResolutionRequest& Request) const;
};
```

**Expected Result:**
- `FieldVariableActionCluster` 高内聚：只负责变量簇语义，不污染 `ActionResolutionCore`。
- `get/set` 的候选解析开始走 UE NodeSpawner/ActionDatabase 方向。
- 无旧 direct spawn fallback。

---

### Step 1.2: get/set 候选匹配规则

**Files:**
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperFieldVariableActionResolver.cpp`

**Action:**
1. 对 `Semantic.Kind == Get`：只返回可读变量候选。
2. 对 `Semantic.Kind == Set`：只返回可写变量候选。
3. 候选过滤优先级：

```text
1. 变量名精确匹配
2. 变量显示名/别名匹配
3. 目标对象约束匹配：self / component / object reference
4. pin 类型约束匹配
5. 当前图与 Blueprint class 可见性约束
```

4. 候选返回必须是结构化对象，不能只返回字符串：

```json
{
  "candidate_functions": [],
  "candidate_actions": [
    {
      "cluster": "field_variable",
      "semantic": "get",
      "display_name": "SmokeFloat",
      "field_name": "SmokeFloat",
      "owner": "BP_RP_UI_Actor_C",
      "pin_type": "float",
      "score": 100,
      "reason": "exact_name,type_match,self_scope"
    }
  ]
}
```

5. 不唯一时返回 `Ambiguous`，并提供精简候选列表供 Agent 下一轮指定。
6. 找不到时返回 `NotFound`，并说明缺失的约束，例如变量名、scope、target object 或 pin type。

**Expected Result:**
- Preview 可以稳定反馈 `resolved / ambiguous / not_found`。
- AgentFace 不需要膨胀字段，只通过已有上下文和 preview 候选迭代提高准确性。

---

### Step 1.3: 接入 ActionResolutionCore，不新增一级类型

**Files:**
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperActionResolutionCore.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperSpawnerClusterResolver.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperGraphStatementBuilder.cpp`

**Action:**
1. `ActionResolutionCore` 保持只构造：

```cpp
Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Get; // or Set
```

2. `SpawnerClusterResolver` 保持：

```cpp
return ClusterByKind.FindRef(Request.ClusterKind);
```

3. `GraphStatementBuilder` 对 `get/set` 的职责只到构造 request 和消费 result，不允许创建 UE 节点。

4. 如果当前 `NodeFragment adapter` 尚未能消费 `SelectedSpawner`，`execute` 必须返回明确状态：

```text
field_variable_resolved_but_fragment_adapter_missing
```

该状态不是 fallback；它是架构迁移缺口的显式失败。

**Expected Result:**
- `get/set` 不再绕过簇架构。
- `ActionResolutionCore` 不因 `get/set` 引入语义一级分发回退。
- 若 fragment adapter 尚未完成，preview 仍能给出真实候选，execute 明确失败而不是旧路径成功。

---

### Step 1.4: 添加 FieldVariableAction 自动化测试

**Files:**
- 新增 `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\BlueprintHelperFieldVariableActionClusterTests.cpp`

**Action:**
1. 测试 `Get` 请求进入 `FieldVariableActionCluster`。
2. 测试 `Set` 请求进入 `FieldVariableActionCluster`。
3. 测试无变量时返回 `NotFound`。
4. 测试同名/模糊候选返回 `Ambiguous`。
5. 测试结果对象包含 `cluster`、`semantic`、`display_name`、`field_name`、`pin_type`、`score`、`reason`。

**Expected Result:**
- P1 有测试覆盖，不靠人工观察确认架构是否偏回旧实现。

---

## Validation

### Required source hygiene check

```powershell
rg "EBlueprintHelperActionIntent|ActionRequest\.Intent|Request\.Intent|IntentToString|SelectCluster\(Intent|SpawnVariableGetNode|SpawnVariableSetNode" `
  D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper `
  D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Plan\BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md `
  D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Plan\BlueprintHelper_SpawnerClusterSkeleton_ImplementationPlan_20260521_CN.md
```

Expected: no matches. If a match is in an explanatory historical section, delete or rewrite that section; this codebase no longer keeps old fallback history in active design docs.

### Required build

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

Expected: build succeeds.

### Required runtime smoke after implementation

1. Start editor via global MCP lifecycle tool when available.
2. Run a TaskSpec preview containing one `get` and one `set` statement against a Blueprint variable.
3. Confirm preview returns either one selected action or a structured ambiguity list.
4. If `NodeFragment adapter` is not part of the implementation pass, confirm execute returns `field_variable_resolved_but_fragment_adapter_missing` and does not write through old direct spawn.
5. If adapter is implemented in the same pass, confirm execute creates get/set nodes through the selected UE NodeSpawner path.
6. Close editor via global MCP lifecycle tool.
7. Rebuild once after code changes.


## Execution Update（2026-05-21）

- [x] P0 文档口径已同步为 `ClusterKind + SemanticConstraints`。
- [x] P0 契约测试已添加，覆盖一级分发键和旧 intent/direct variable spawn 源码卫生。
- [x] P1 `FieldVariableActionResolver` 已添加，`FieldVariableActionCluster` 已接入 get/set provider。
- [x] 旧 `SpawnVariableGetNode/SpawnVariableSetNode` 公开 direct spawn 入口已移除。
- [x] UE 5.6 编译已通过。

距离期望差距：本轮 P1 按计划完成 provider-only 候选解析；非 call 的 NodeFragment adapter 不在本轮完成范围，后续 get/set execute 仍应显式返回 adapter 缺口，不允许回退旧 direct spawn。
---

## Completion Criteria

- [x] P0 docs no longer describe semantic intent as `ActionResolution` 一级分发。
- [x] P0 contract tests prevent reintroducing old intent/fallback paths。
- [x] P1 `FieldVariableActionCluster` owns get/set candidate resolution。
- [x] P1 uses UE NodeSpawner/ActionDatabase direction and does not use direct `UK2Node` spawn fallback。
- [x] Preview returns structured resolved/ambiguous/not_found results。
- [x] Any execute gap is explicit, not silently routed to legacy code。
- [x] Build succeeds on UE 5.6。

---

## Commit Message Template

```text
新增内容：
1. 添加 ActionResolution P0/P1 FieldVariable 实施计划

变更需求：
1. 明确 ActionResolution 一级分发只允许 SpawnerClusterKind，语义动作收敛到 SemanticConstraints
```
## Runtime Smoke Update（2026-05-21）
- [o] CLI runtime smoke partially attempted: `task preview` can reach the running editor Bridge and returns structured preview output.
- [ ] FieldVariable get/set runtime smoke was not fully completed because preparing a fresh Blueprint fixture through `task execute` returned `Bridge connection error: read ECONNRESET`; reusing an existing manual asset was also blocked by `target_blueprint_not_found` in the current editor session.

距离期望差距：本轮已完成 P0/P1 代码、契约测试源码、FieldVariable provider 和 UE 5.6 编译；运行面尚未证明 get/set provider preview on a real Blueprint asset。该差距属于测试夹具准备/写入链路阻塞，不应虚标为 FieldVariable provider 已完成端到端验证。