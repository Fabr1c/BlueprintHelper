# BlueprintHelper 开发规则

Date: 2026-05-20

Status: active cross-module development rules.

## 适用范围

本文档记录开发新内容时必须遵守的跨模块开发规则。它不同于模块设计文档、工具字段规则、CLI Tips、ReadContext 格式规则或 ReviewPanel 专项架构计划。

模块规则可以继续保存在对应模块文档中；当模块规则和本文档冲突时，优先按本文档的跨模块开发规则处理，并重新调整模块设计。

## 架构与实现规则

1. 所有架构相关的新功能必须以通用性、低耦合、高内聚为硬性目标设计和实现。
2. 有 workflow、async、lifecycle、state coordination 特征的逻辑，优先放到可复用 service / coordinator / presenter / model 边界中，不应堆在 UI widget 或单个工具函数里。
3. UI 层只负责展示、输入事件转发和轻量绑定；业务状态、Review 状态、异步流程、生命周期级联、持久化写入不应由 UI 本地分支各自实现。
4. 同一类用户操作必须复用统一 action pipeline，不允许每个 panel / row 单独实现一套 Accept / Reject / Refresh 逻辑。
5. 新增能力应优先扩展已有 registry / resolver / builder / adapter / handler 边界，而不是为单个场景硬编码特殊分支。
6. 不允许为了修一个具体 UI bug 引入违背数据模型语义的例外逻辑；如果 UI 表现不符合预期，应优先检查 Store / Presenter / Model 的语义是否正确。
7. Review 系统以 Review v2 为唯一架构基线；旧 Transaction / Review v1 / legacy anchor / fallback 兼容路径不应继续保留或新增。
8. DebugBundle / Review evidence / UI overlay / AcceptReject 状态必须消费同一套 Review 数据模型，不能各自维护互相冲突的解释。
9. 所有 C++ 类默认独立 `.h/.cpp` 文件；结构体、纯数据类、枚举可以例外。
10. C++实现不允许使用namespace，创建util工具类来代替。

## 配置与 Setting 规则

1. 所有硬编码的可配置变量应尽可能抽成 setting，并统一由 `BlueprintHelper/Config/DefaultSetting.json` 提供默认值，再由运行时 setting store / presenter / consumer 统一消费。
2. 新增 setting 时必须明确它属于普通用户可配置项还是 developer-only 配置项，并同步维护 `settings_visibility.user_editable` 或 `settings_visibility.developer_only`。
3. 不允许在 UI、工具 handler、TaskRuntime、Review 服务或 formatter 中各自散落同类默认值；共享行为默认值必须通过统一 setting 读取。
4. 不应为了快速验证把可配置数值、开关、路径、超时、缓存策略、UI 尺寸、诊断保留策略写死在业务实现里。确实不能配置化的常量应在代码附近说明原因。

## 兼容与旧实现规则

1. 架构变更不做旧 Agent / 旧字段 / 旧工具兼容，除非用户明确要求临时兼容。
2. 旧代码、废弃工具簇、废弃 fallback 一旦确认不再属于当前架构，应移除而不是隐藏或继续旁路调用。
3. UE 5.6 是生产兼容基线。非 5.6 兼容必须通过明确 `#if` / capability macro / 窄 fallback helper 隔离，不能改弱 UE 5.6 主路径。
4. 如果 UnrealHeaderTool 或引擎版本差异无法支持需要的条件形态，应记录 blocker，而不是降低当前架构语义。

## Review 与 Debug 基线规则

1. Reject 语义以 evidence before snapshot 为回滚目标；`current != latest after` 只能进入 DebugBundle / 用户提示，不应作为阻塞 Reject 的条件。
2. FinalReview 构树应由 Review 数据模型驱动：`ParentChangeId` 表示真实父子关系，UI 不应根据局部视觉假设屏蔽它。
3. Asset lifecycle root 语义必须区分资产级 root 和资产内对象 lifecycle root；资产级 root 可以作为同资产 ReviewEvent 的父节点，资产内对象 root 也可以继续作为其子变更的父节点。

## UI 生命周期规则

1. ReviewPanel、行 diff 渲染、row refresh、geometry readiness、panel lifecycle synchronization 和 UI 层状态传播必须使用确定性的事件驱动流程。
2. 不允许用 arbitrary timer、delay、one-frame refresh、retry loop、ActiveTimer、polling retry、geometry retry counter 或 AsyncTask 延迟来掩盖 UI 生命周期顺序问题。
3. 如果 UI 刷新需要顺序保证，应创建或复用明确 lifecycle event、broadcast、presenter state update、row registration event 或 canonical state projection boundary。

## 临时补丁规则

任何“先临时补丁”的实现必须能解释为什么不会破坏统一范式；解释不成立时应先停下来分析，不直接改代码。
