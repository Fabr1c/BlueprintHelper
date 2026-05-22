# BlueprintHelper GraphWrite 80% 能力与正确率路线图

日期：2026-05-22

## 1. 目标

本路线图采用方案 B：正确率基线优先。目标是在保持当前 GraphStatement Framework 边界正确的前提下，持续推进 GraphWrite 能力，最终达到：

1. GraphWrite 能力覆盖达到 80% 以上。
2. 在最大利用现有 TaskSpec / SemanticIR / UE runtime context 的情况下，GraphWrite 正确率达到 80% 以上。
3. 不再因为旧残留、旧 fallback、局部硬编码或绕过统一分发链路引入新的不稳定 bug。

这里的 80% 不是按 UE 节点总数平均计算，而是按真实复杂需求与覆盖型合成需求形成的场景矩阵计算。第一版基线暂定为 3 个复杂需求编程测试：1 个真实需求 + 2 个覆盖型合成需求。

## 2. 已确认硬边界

| 边界 | 规则 |
|---|---|
| Legacy 严格模式 | 只要代码路径保留旧语义、旧 fallback、旧模型、旧 transaction/review 解释，默认视为 legacy，不能复用进新主链路。 |
| Legacy 删除规则 | 判定为旧路径后默认直接删除；如果暂时不能删除，必须写入 `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`，并附带不能删除的具体原因。 |
| Direct spawn 边界 | `branch`、`sequence`、`select` 等 canonical singleton / 唯一控制流可以 direct spawn，但必须仍走 `SpawnerClusterKind -> cluster -> semantic constraint -> singleton/control-flow evidence provider -> shared spawn adapter`。 |
| Direct spawn 禁止项 | builder、composer、mutation helper 不允许私自 new / spawn UE node；direct spawn 不能成为 wide-surface semantic 搜索失败后的 fallback。 |
| Wide-surface resolver 边界 | `callfunction`、field/property、delegate/bind、create/convert/schedule 等宽范围或高歧义语义必须最大化利用现有上下文，走完整 context / resolver / evidence / spawner 链路。 |
| NeedsMoreSemanticContext 口径 | 必要上下文缺失不等于候选超过阈值。缺关键 evidence、候选过多、候选歧义过高都可以进入 `NeedsMoreSemanticContext`，但必须用不同原因码表达。 |
| No-guessing 规则 | 不允许为了返回成功而 fallback 到旧字段猜测、旧扫描路径、局部硬编码或无 evidence 的直接成功。 |
| 正确率口径 | 先通过 3 个复杂需求编程测试收集 GraphWrite 流程错误率和 call 正确率，形成测试文档；后续再扩大场景矩阵。 |

## 3. Resolver 失败分类

| 分类 | 触发条件 | 期望结果 |
|---|---|---|
| `missing_required_evidence` | 进入搜索前缺关键 evidence，例如 callable name、owner scope、field owner、property path、binding object、delegate signature、target type。 | 返回 `NeedsMoreSemanticContext`，列出缺失字段。 |
| `candidate_threshold_exceeded` | ActionDatabase / filter 后候选数量超过安全阈值。 | 返回 `NeedsMoreSemanticContext`，附 top candidates 与缩小范围建议。 |
| `ambiguous_candidates` | 候选数量未超阈值，但多个候选得分接近，不能安全选择。 | 返回 `NeedsMoreSemanticContext`，附歧义候选与所需 disambiguation evidence。 |
| `not_found` | 上下文足够但没有合法候选。 | 返回明确 not found，不伪装成缺上下文。 |
| `unsupported_intent` | 当前架构尚未实现该语义。 | 返回 unsupported，并记录能力 gap 或测试期望。 |
| `spawn_or_link_failure` | resolver 成功但 spawn、默认值、pin binding、link 或 readback 失败。 | 计入 GraphWrite 流程错误率，并输出 DebugBundle。 |
| `silent_wrong_graph` | 表面成功但 readback 或编译/preview 证明 graph 语义错误。 | 最高优先级缺陷；不能计为正确。 |

## 4. 三个复杂需求测试

### 4.1 真实需求：PhysicalDoor_InteractableOnly

目标：验证 GraphWrite 在真实可交互门 Actor 内部逻辑上的能力，不扩展玩家侧交互、输入、射线、UI 或角色实现。

#### Setup Phase，不计入 GraphWrite 正确率

| 步骤 | 说明 | 失败归类 |
|---|---|---|
| 创建 Blueprint 资产 | 自动创建门 Actor Blueprint。 | `setup_failure` |
| 添加基础组件 | 自动准备 `Root` / `Hinge` / `DoorMesh`，并确保后续 GraphWrite 可定位。 | `setup_failure` |
| 保存/打开/校验资产 | 确认资产可进入 GraphWrite Phase。 | `setup_failure` |

Setup Phase 的失败记录在测试文档中，但不计入 GraphWrite 正确率或 call 正确率。

#### GraphWrite Phase，计入正确率

| 需求 | 期望 |
|---|---|
| 关闭静止状态 | 门关闭且静止时，`DoorMesh` 不开启 physics simulation。 |
| 交互接口 | 只保留门内部交互接口，例如轻推与猛开入口；不实现玩家侧调用。 |
| 两种打开方式 | 轻推与猛开都能打开门，但强度、速度或 impulse 参数不同。 |
| 打开角度 | 门围绕门页/hinge 旋转约 177 度。 |
| 打开后物理 | 门打开后开启 physics simulation，可以响应碰撞。 |
| 碰撞合上 | 玩家或其他物体可通过碰撞把门合上；门回到关闭阈值内后判定关闭。 |
| 合上后物理关闭 | 判定关闭后关闭 physics simulation，回到关闭静止状态。 |

#### 计分重点

| 指标 | 说明 |
|---|---|
| GraphWrite 流程错误率 | 统计 context、resolver、spawn、default、link、readback、DebugBundle 失败。 |
| Call 正确率 | 统计物理、旋转、状态更新相关 callable 是否选对目标、owner、参数与执行顺序。 |
| Silent wrong graph | 若表面成功但门逻辑不符合 readback/preview/编译语义，单独记录为最高优先级错误。 |

### 4.2 合成需求 A：Function / Field / Control 综合

目标：集中覆盖 `FunctionActionCluster`、`FieldVariableActionCluster` 和 canonical control direct spawn 边界。

建议场景：`TimedAccessGate_StateMachine`。在一个 Actor Blueprint 内实现门禁或冷却状态机：根据状态变量、时间戳、权限布尔值和冷却时间决定是否允许进入、拒绝、延迟重试或切换状态。

覆盖范围：

1. function call：时间、日志、状态更新、比较、数学或 utility 调用。
2. field/property：变量 get/set、简单 property path、默认值写入。
3. control：branch、sequence、select 等唯一控制流 direct spawn。
4. pin/link/default：exec/data link、布尔/float/name 默认值、select 输入输出类型。

验收重点：

1. wide-surface call / field 必须走 resolver evidence。
2. control direct spawn 必须走 cluster 内二级语义映射和 shared adapter。
3. 不允许 builder、composer、mutation helper 直接产生控制流节点。

### 4.3 合成需求 B：Struct / Event / Delegate 综合

目标：覆盖 `GenericAssetStructControlActionCluster` 与 `EventDelegateActionCluster` 的高风险缺口，并验证缺上下文时的诊断质量。

建议场景：`EventDrivenConfigApplier`。Actor Blueprint 接收一份配置 struct，拆解后应用到组件属性；通过 custom event 触发应用流程；在有完整 component/delegate/signature evidence 时尝试绑定事件，在 evidence 不完整时必须返回可执行的 `NeedsMoreSemanticContext`。

覆盖范围：

1. struct：make/break struct、配置字段默认值、typed data edge。
2. event：custom event 成功路径。
3. delegate/bind：component-bound event、bind/assign 的 projected evidence 与失败诊断。
4. generic create/convert/schedule：先记录能力状态与上下文缺口，不允许硬编码成功。

验收重点：

1. custom event 可成功生成并 readback。
2. component-bound event / bind 缺 evidence 时返回细分原因，而不是 unsupported 的泛化错误。
3. 如果 evidence 完整后仍不能生成，应记录为 resolver/spawner gap，而不是 fallback 到旧路径。

## 5. 分阶段推进计划

| 阶段 | 目标 | 完成条件 |
|---|---|---|
| P0：基线文档与测试矩阵 | 固化边界、三复杂需求、错误分类、计分表。 | 生成路线图与测试记录模板；gap 文档明确 legacy 删除规则。 |
| P1：Legacy 清理门禁 | 清理仍可达的旧 fallback、旧字段猜测、旧 parsed-node 主路径。 | 新主链路不能调用 legacy；暂不能删的旧路径写入 gap 并说明原因。 |
| P2：统一测试与指标采集 | 让 3 个复杂需求能产出稳定测试记录，即使尚未全部通过。 | 每次运行都能记录 setup failure、GraphWrite failure、call correctness、silent wrong graph。 |
| P3：Function / Field 正确率优先 | 优先拉高 call、get/set、property 的 resolver 正确率。 | Function / Field 相关错误下降；候选歧义能给出可执行诊断。 |
| P4：Direct spawn provider 与 Generic control | 把 branch/sequence/select 等唯一控制流收敛到 singleton/control-flow evidence provider。 | builder/composer/mutation helper 不再私自产生 singleton control node。 |
| P5：Struct / Event / Delegate 补齐 | 补 struct make/break、custom event、component-bound/bind evidence 链路。 | Event/Delegate 缺 context 能给细分诊断；完整 context 能进入对应 spawner family。 |
| P6：Readback / DebugBundle / 正确率闭环 | 把成功结果、readback、DebugBundle 和测试文档绑定。 | 达到或超过 80% 能力覆盖与 80% 正确率，且 silent wrong graph 接近 0。 |

## 6. 测试记录模板

后续建议新增独立测试记录文档：

`BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`

建议表格：

| 测试 | Phase | GraphWrite 能力项 | 期望结果 | 实际结果 | 错误分类 | Call 正确 | GraphWrite 正确 | DebugBundle / evidence | Gap 文档更新 |
|---|---|---|---|---|---|---|---|---|---|
| PhysicalDoor_InteractableOnly | Setup | 创建资产和组件 | 不计入 GraphWrite |  |  | N/A | N/A |  |  |
| PhysicalDoor_InteractableOnly | GraphWrite | 物理门内部逻辑 |  |  |  |  |  |  |  |
| TimedAccessGate_StateMachine | GraphWrite | Function / Field / Control |  |  |  |  |  |  |  |
| EventDrivenConfigApplier | GraphWrite | Struct / Event / Delegate |  |  |  |  |  |  |  |

## 7. 推进原则

1. 每一轮先更新测试记录，再判断是否修改代码。
2. 任何旧路径如果被发现仍可达，优先删除；暂不能删除则写入 gap 文档并说明原因。
3. 宽范围语义不追求猜中，优先追求可解释失败和可补上下文。
4. 唯一控制流允许 direct spawn，但必须通过统一 evidence/provider/adapter 边界。
5. 正确率统计必须区分 setup failure、GraphWrite failure、call failure 和 silent wrong graph。
6. 阶段完成不能只看测试通过，还要看 DebugBundle / readback / gap 文档是否一致。
