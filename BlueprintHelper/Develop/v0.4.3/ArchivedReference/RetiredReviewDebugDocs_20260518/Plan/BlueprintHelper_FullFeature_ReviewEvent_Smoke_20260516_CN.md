# BlueprintHelper 全量功能 ReviewEvent Smoke 记录 2026-05-16

## 1. 测试时间

- 执行时间：2026-05-16 11:34:51 +08:00
- Editor 生命周期：已通过全局 MCP 启动，当前保持打开，供 ReviewPanel 手动验证。
- CLI 入口：使用 h.cmd，避免 PowerShell ExecutionPolicy 触发 h.ps1。

## 2. 成功生成 ReviewEvent 的测试根目录

| 根目录 | 用途 |
|---|---|
| /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818 | 覆盖 Blueprint 创建、变量、签名、Structure、DataTable、DataAsset、WidgetBlueprint 创建等主线。 |
| /Game/BlueprintHelperCliSmoke/FullFeaturePatch_20260516_113300 | 重新覆盖组件与 SemanticIR 图写入，避开前一轮失败图对编译状态的污染。 |

## 3. CLI 执行结果

| 功能 | 结果 | 备注 |
|---|---|---|
| Runtime profile / AgentGuide | 通过 | Bridge 可用。 |
| Write session | 通过 | scope=project，	tl_seconds=1200。 |
| Blueprint 创建 | 通过 | 	ask_F468FDFA49411F091D85DCB7805982F9、补跑 	ask_4C9BA38A4AA336270BB8FF9E020DB7A8。 |
| Blueprint 变量 | 通过 | 	ask_3F2E8C2248D406F9A89CF09014DB3974、补跑 	ask_E42B3F68486EB3D2034360B1227AFD45。 |
| Blueprint 签名 | 通过 | 函数、自定义事件、事件分发器写入成功，	ask_D372240E4C968D1A3D9412A6C5C162B8。 |
| Blueprint 组件 | 通过 | 原模板值失败后改用 
ame_collision_policy=reuse_if_exists，补跑通过 	ask_148021C948733E11D72C518C4B2AC2BB。 |
| SemanticIR 图写入 | 通过 | 干净 Actor 上 let/get_property/compare/branch/select/make_struct/call 组合通过，	ask_4C16C56E4C1467EA888851811BB9FB50。 |
| Structure 创建 | 通过 | 	ask_0999B36544082B2BC14F4EA84428F31A。 |
| DataTable 创建与行编辑 | 通过 | 创建 	ask_AF82C3694F042513A009BF9277A23DEF；行编辑 	ask_D81EBF5346D20EF7CF18A6BD2E3179E5。 |
| DataAsset class / instance / object properties | 通过 | class 	ask_A309D026491A3784DAB9FEACC1516FA0，变量 	ask_3286E1044EFAC3A05C7668AB8F584A52，instance 	ask_CBA54B5147D932CF040A6EB0B802002E，属性 	ask_AE0789F54A5F5BBCCE3A649B0168EDB7。 |
| WidgetBlueprint 创建 | 通过 | 	ask_49C786CF478F5C5C5C093A93CF106E16。 |
| UMG WidgetTree 编辑 | 未达期望 | TaskSpec schema 要求 parent_widget_name 非空，但 UE service 支持空 parent 创建 RootWidget，导致新 WBP 无法通过普通 TaskSpec 创建根 Widget。 |

## 4. Pending Review 查询结果

- 查询文件：D:\UEProjects\Template\Saved\BlueprintHelper\Cli\cli_1778902413873\result.json
- Pending record 数：17

| Asset | Review records | Visible changes |
|---|---:|---:|
| /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/BP_RP_FullFeatureActor_20260516_112818 | 5 | 8 |
| /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/BP_RP_FullFeatureDataAssetClass_20260516_112818 | 2 | 4 |
| /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/DA_RP_FullFeatureObject_20260516_112818 | 2 | 4 |
| /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/DT_RP_FullFeatureTable_20260516_112818 | 2 | 3 |
| /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/ST_RP_FullFeatureRow_20260516_112818 | 1 | 4 |
| /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/WBP_RP_FullFeatureWidget_20260516_112818 | 1 | 1 |
| /Game/BlueprintHelperCliSmoke/FullFeaturePatch_20260516_113300/BP_RP_FullFeaturePatchActor_20260516_113300 | 4 | 5 |

## 5. 需要在 ReviewPanel 手动验证的重点

1. 打开 BlueprintHelper 面板并切到 Review。
2. 选择 /Game/BlueprintHelperCliSmoke/FullFeaturePatch_20260516_113300/BP_RP_FullFeaturePatchActor_20260516_113300。
3. 验证 Components 面板能显示 PatchFrame、PatchTrigger Diff Row，并且选中 Row 时右侧出现 Accept/Reject。
4. 验证 MyBlueprint 能显示 SemanticFlag 与新增图/事件 Diff。
5. 双击 MyBlueprint 中的 Diff 函数/事件，中央 GraphPanel 应跳转到对应图。
6. 选择 SemanticIR 图体 Review，确认 GraphPanel 有底层 Diff 绘制，且 compare 已连到 Branch condition。
7. 选择 /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/DT_RP_FullFeatureTable_20260516_112818，验证 DataTable 中 Alpha 修改、Beta 新增的 Row Diff 与 Accept/Reject。
8. 选择 /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/DA_RP_FullFeatureObject_20260516_112818，验证 DA 属性 Diff、只读状态、Accept/Reject 后主 Panel 刷新。
9. 选择 /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/ST_RP_FullFeatureRow_20260516_112818，验证 Structure 字段 Row Diff 和 Reject 回滚。
10. 选择 /Game/BlueprintHelperCliSmoke/FullFeature_20260516_112818/WBP_RP_FullFeatureWidget_20260516_112818，仅验证 WidgetBlueprint 资产创建根事件；WidgetTree 子节点编辑本轮未生成成功事件。

## 6. 发现的问题

### Bug A：SemanticIR preview 未拦截可导致编译失败的 select/compare 类型问题

- 复现：select.condition 内使用 compare(==)，左右为 int literal。
- preview：通过。
- execute：失败，编译器结果显示 相等（整数） 无法从通配符提升到整数，并出现 could not successfuly expand pins。
- 影响：preview 与 execute 不一致；失败执行还留下 Graph Review 记录，需要确认 rollback/失败记录策略。
- 期望：preview 阶段根据 compare operand type 明确生成 typed operator pin，或在无法推断时阻止执行。

### Bug B：UMG root widget 无法通过普通 TaskSpec 创建

- 复现：新建 WidgetBlueprint 后执行 edit_umg_widget 创建 RootCanvas。
- TaskSpec schema：要求 parent_widget_name 至少 1 字符。
- UE service：AddWidget 支持空 parent，在无 RootWidget 时把 panel widget 设置为 RootWidget。
- 影响：AgentFace 普通 TaskSpec 无法完成新 WBP 的 root widget 创建，WidgetTree ReviewEvent 覆盖不完整。
- 期望：parent_widget_name 对 root 创建应允许省略或空字符串，并在 schema/模板/编译器/UE service 之间保持一致。

### Bug C：组件模板字段值与当前 UE 侧枚举不一致

- 复现：模板使用 on_name_conflict=reuse_existing，preview 被 UE 侧拒绝。
- 实际可用：
ame_collision_policy=reuse_if_exists。
- 影响：Agent 按模板执行组件任务会失败。
- 期望：更新模板和 TaskSpec compiler alias，使 on_name_conflict 与底层 
ame_collision_policy 稳定映射。

## 7. 阻塞内容

1. UMG WidgetTree 子节点 ReviewEvent 本轮未生成，因为 root widget 创建在 TaskSpec schema 层被拦截。
2. SemanticIR select(compare(int == int)) 的 preview/execute 类型一致性仍需修复；本轮已通过替代用例完成主链路 smoke，但原始失败用例不能标记完成。

## 2026-05-16 UMG Root Widget 修复记录

- 修复项：`edit_umg_widget.create_widget` 现在允许省略 `parent_widget_name`，也允许显式传入空字符串用于创建 RootWidget。
- 编译链路：Python P1 compiler 会把空 parent 视为 root creation intent，不再向 TaskPlan 下游传递空 parent 字段。
- 文档同步：TaskSpec 模板改为先创建 root，再在 root 下创建 child；AgentGuide 明确 WidgetBlueprint 资产创建和 WidgetTree 编辑不是同一能力。
- 当前状态：源码与文档已修复，TaskCore/CLI 构建通过；待编辑器内 UMG root create 执行复测。
