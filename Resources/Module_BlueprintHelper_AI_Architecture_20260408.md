# BlueprintHelper AI 通信架构设计（总览）

## 文档拆分说明
原始单文档已按职责拆分为三份，当前文件只保留总览与导航：

1. 总览架构：当前文件
2. 协议设计：[Module_BlueprintHelper_AI_Protocol_20260408.md](Module_BlueprintHelper_AI_Protocol_20260408.md)
3. 实施计划：[Module_BlueprintHelper_AI_ImplementationPlan_20260408.md](Module_BlueprintHelper_AI_ImplementationPlan_20260408.md)

推荐阅读顺序：

1. 先读总览，确认边界与分层
2. 再读协议设计，确认 MCP 与 UE Bridge 接口
3. 最后读实施计划，进入类、接口、文件落点与阶段任务

---

## 结论
推荐将 BlueprintHelper 演进为“双层通信架构”：
- UE5 内部继续保留现有的蓝图 <-> Json 核心能力；
- IDE / AI 侧通过本地 MCP Server 接入这些能力；
- MCP Server 与 UE5 编辑器之间不直接复用 MCP，而是使用更适合本地双向持续通信的桥接协议；
- 桥接协议建议优先选择 Named Pipe，其次是 WebSocket；
- MCP 负责 AI 可调用的工具与资源暴露，桥接协议负责 UE 编辑器侧实际执行与状态回传。

推荐目标链路：

```text
IDE AI Plugin -> Local MCP Server -> UE Bridge -> BlueprintHelper Core -> Blueprint Graph
```

该方案比“UE 直接实现 MCP”更稳，原因如下：
- 更符合 IDE 对本地 MCP 服务的常见接入方式；
- UE 编辑器进程不需要承担完整协议兼容、会话管理和对外暴露职责；
- UE 热重载、编辑器重启、蓝图编译失败等异常更容易隔离；
- 后续可以在不修改 UE 插件主逻辑的情况下替换 AI 接入方式。

---

## 一、背景与目标

### 1.1 背景
当前 BlueprintHelper 已经具备 AI 接入最关键的基础能力：
- Blueprint T3D -> Json
- Json -> 蓝图节点生成
- Json 规则约束文档
- 未匹配节点回退与手工映射 UI

从现有代码职责来看：
- `FBlueprintHelperModule` 负责工具窗口注册、打开主面板、获取当前激活蓝图图表、读取规则文档；
- `FBlueprintToTextConverter` 负责 Blueprint T3D 到插件 Json 的转换；
- `TextToBlueprintGenerator` 负责 Json 解析、变量声明补齐、节点生成、引脚连线、未匹配节点回退；
- `SHelperMainWidget` 负责当前基于编辑器 UI 的交互链路。

### 1.2 目标
本方案目标是把现有工具插件扩展成“AI 可操作的蓝图编辑能力中台”，支持以下闭环：

1. AI 读取 Json 规则与当前图表上下文
2. AI 生成或修改符合规则的 Json
3. IDE 通过 MCP 调用本地服务，请求 UE 执行导入
4. UE 在当前蓝图图表中生成节点、变量、连线
5. UE 返回生成结果、未匹配节点、编译结果和诊断信息
6. AI 基于结果继续修正，直到生成成功

### 1.3 非目标
当前阶段不建议把系统设计成：
- 高频图编辑事件逐帧同步总线
- 视频流、图像流或大体量二进制传输通道
- 跨网络暴露给外部机器的远程编辑服务
- 直接替代 Unreal 原生蓝图编辑器交互

---

## 二、现状判断

### 2.1 已有能力
当前插件已经支持：
- `K2Node_CallFunction`
- `K2Node_VariableGet`
- `K2Node_VariableSet`
- `K2Node_MacroInstance` 的标准 `ForLoop`
- 本地变量声明与确保存在

### 2.2 当前不足
当前仍有明显边界：
- 复杂控制流节点支持不完整；
- 编译、诊断、资产打开等编辑器能力尚未结构化暴露；
- 生成入口必须依赖当前编辑器窗口与图表焦点；
- 还没有为外部系统设计稳定的命令协议。

### 2.3 关键判断
当前插件最适合接入 AI 的部分不是 Slate UI，而是：
- 规则文档
- Json 导出能力
- Json 导入能力
- 未匹配与错误回报能力

因此后续架构演进应避免“让 AI 操作 UI”，而应改为“让 AI 调应用服务接口”。

---

## 三、目标分层

### 3.1 推荐组件职责

#### A. IDE AI Plugin
职责：
- 承载聊天、补全或 Agent 工作流
- 消费 MCP tools / resources
- 不直接理解 Unreal 内部实现细节

#### B. Local MCP Server
职责：
- 对 AI 暴露可调用工具
- 暴露规则文档、当前上下文、最近诊断等资源
- 管理任务 ID、超时、重试、日志与权限
- 将高层语义命令转换为 UE Bridge 指令

#### C. UE Bridge Adapter
职责：
- 作为 UE 编辑器侧桥接入口
- 接收桥接命令并路由到 BlueprintHelper Core
- 获取当前目标蓝图与图表上下文
- 组织执行结果、错误、编译输出

#### D. BlueprintHelper Core
职责：
- 保留并沉淀已有 Json 导出 / 导入核心能力
- 提供无 UI 的应用服务 API
- 未来承载更多节点类型与验证规则

#### E. Slate UI
职责：
- 继续作为人工调试与验收工具
- 不再作为唯一主入口
- 后续复用同一套 Core API，而不是独占流程

### 3.2 为什么不建议 UE 直接作为 MCP Server
理论上可以做，但当前阶段不是最优解，原因如下：
- UE 进程需要直接承担 MCP 生命周期管理
- 需要处理协议初始化、能力声明、请求分发、通知回传、异常恢复
- 需要考虑编辑器重启、热重载后连接恢复
- IDE 往往更习惯连接独立的本地 MCP 进程，而非重型编辑器进程本身
- MCP 更适合工具调用，不适合 UE 内部持续高频状态流

结论是：MCP 解决“AI 怎么调用能力”，Bridge 解决“能力如何在 UE 中稳定执行”，两者应分层。

---

## 四、文档边界

### 4.1 协议设计文档负责什么
[Module_BlueprintHelper_AI_Protocol_20260408.md](Module_BlueprintHelper_AI_Protocol_20260408.md) 负责：
- MCP 层职责边界
- Bridge transport 选择
- MCP tools / resources 设计
- UE Bridge 请求、响应、任务、事件模型
- 安全、幂等与生命周期约束

### 4.2 实施计划文档负责什么
[Module_BlueprintHelper_AI_ImplementationPlan_20260408.md](Module_BlueprintHelper_AI_ImplementationPlan_20260408.md) 负责：
- 模块拆分落点
- 类设计与接口建议
- 文件级改造建议
- Phase 1 到 Phase 4 的实施任务
- MVP 验收标准与风险控制

---

## 五、最终建议
结论上，建议按以下顺序推进：

1. `BlueprintHelper Core` 头less 化
2. `UE Bridge` 本地通信层
3. `Local MCP Server` AI 接入层

这条路线的优点是：
- 对 AI 友好：可读规则、可调用工具、可获取诊断
- 对 UE 友好：保留编辑器内执行、事务和编译控制
- 对工程演进友好：可以先 MVP，再逐步扩展节点类型、错误修复和实时通知