# BlueprintHelper AI 协议设计（v0）

## 目标
本文件只回答两个问题：

1. IDE / AI 侧应该如何调用 BlueprintHelper 能力
2. MCP Server 与 UE 编辑器之间应该如何定义稳定的本地协议

不讨论文件改造与类落点，这部分放在 [Module_BlueprintHelper_AI_ImplementationPlan_20260408.md](Module_BlueprintHelper_AI_ImplementationPlan_20260408.md)。

---

## 一、协议分层

### 1.1 控制面与数据面
推荐把通信协议分成两层：

1. MCP 控制面
职责：
- 能力发现
- 资源暴露
- AI 工具调用
- 长任务状态查询
- 资源更新通知

2. UE Bridge 数据面
职责：
- 本地命令投递
- 执行状态回传
- 任务生命周期管理
- UE 编辑器上下文回读

结论：
MCP 只面向 AI，Bridge 只面向 UE，二者不要复用为同一协议实现。

### 1.2 为什么不让 UE 直接走 MCP
不推荐当前阶段让 UE 直接作为 MCP Server，原因如下：
- UE 编辑器不是轻量常驻工具进程
- 编辑器热重载、崩溃、重开会话时恢复复杂
- IDE 对本地 MCP 的常见接入方式更偏向独立进程
- UE 内部仍需要一套比 MCP 更贴近编辑器执行语义的命令层

---

## 二、Bridge Transport 选择

### 2.1 Named Pipe
推荐作为 MVP 首选。

适用场景：
- Windows 本地开发
- 同机 IDE 与 UE 编辑器通信
- 先验证链路是否闭环

优点：
- 本地 IPC 实现简单
- 不需要暴露端口
- 安全边界更容易控制

缺点：
- 跨平台扩展性一般
- 调试工具不如 WebSocket 直观

### 2.2 WebSocket
推荐作为第二阶段替代或升级方案。

适用场景：
- 需要双向长连接与事件推送
- 需要更容易抓包与联调
- 未来考虑跨平台

优点：
- 消息模型直观
- IDE 侧实现简单
- 更适合状态推送

缺点：
- 需要端口与来源控制
- 需要更多本地安全治理

### 2.3 gRPC
当前阶段不建议作为首选。

原因：
- 工程成本更高
- UE 侧接入与分发链路更重
- 当前需求还没复杂到必须依赖强类型 RPC 框架

### 2.4 推荐结论
如果当前工作环境以 Windows + UE5 Editor + 本地 IDE 为主，推荐顺序：

1. MVP：Named Pipe
2. 增强版：WebSocket
3. 稳定后再评估 gRPC

---

## 三、MCP Tool 设计

### 3.1 设计原则
- Tool 只暴露高层语义，不暴露 Unreal 底层实现细节
- 写操作必须显式传入目标资产与图表，不能只依赖当前焦点
- 长任务只返回 `taskId`，不在一次调用中塞完整日志
- 大文本优先走资源，不要在 tool result 里重复传输

### 3.2 推荐 Tools

#### A. `blueprint.get_rule_markdown`
用途：
- 获取当前 Json 转蓝图规则

输入：
- 无

输出：
- `markdown`
- `version`

#### B. `blueprint.export_selection_to_json`
用途：
- 导出当前选中的蓝图节点为插件 Json

输入：
- `targetBlueprint`
- `targetGraph`
- `selectionMode`

输出：
- `json`
- `nodeCount`
- `linkCount`
- `selectionSummary`

#### C. `blueprint.validate_json`
用途：
- 在导入前做 Json 结构与规则预检查

输入：
- `json`

输出：
- `isValid`
- `errors`
- `warnings`
- `normalizedVersion`

#### D. `blueprint.import_json_to_graph`
用途：
- 在目标蓝图图表中生成节点

输入：
- `targetBlueprint`
- `targetGraph`
- `json`
- `placementPolicy`
- `compileAfterImport`
- `requestId`

输出：
- `taskId`
- `accepted`
- `message`

#### E. `blueprint.compile_blueprint`
用途：
- 主动触发蓝图编译

输入：
- `targetBlueprint`

输出：
- `taskId`
- `accepted`

#### F. `blueprint.get_task_result`
用途：
- 查询导入或编译任务结果

输入：
- `taskId`

输出：
- `status`
- `progress`
- `result`
- `diagnostics`

#### G. `blueprint.open_asset`
用途：
- 在 UE 中打开指定蓝图资产并聚焦

#### H. `blueprint.get_editor_context`
用途：
- 获取当前编辑器上下文

输出建议包含：
- 当前资产路径
- 当前图表名
- 当前选择节点数
- 最近一次任务 ID
- 最近一次诊断摘要

---

## 四、MCP Resource 设计

### 4.1 推荐 Resources

#### A. `resource://blueprint/rules/json-to-blueprint`
内容：
- 当前规则文档全文

#### B. `resource://blueprint/context/active-graph`
内容：
- 当前激活蓝图图表摘要

#### C. `resource://blueprint/context/selection-json`
内容：
- 当前选择内容导出的 Json

#### D. `resource://blueprint/diagnostics/latest`
内容：
- 最近一次导入 / 编译 / 校验诊断

#### E. `resource://blueprint/functions/catalog`
内容：
- 当前可用 BlueprintCallable / BlueprintPure 函数目录摘要

### 4.2 资源更新策略
资源更新通知建议来自以下事件：
- 编辑器切换资产
- 图表切换
- 当前选择变化
- 导入任务完成
- 编译任务完成

MCP Server 可把这些事件转换为：
- `resources/list_changed`
- `resources/updated`

---

## 五、UE Bridge 命令模型

### 5.1 请求对象

```json
{
  "request_id": "req_123",
  "command": "import_json_to_graph",
  "payload": {
    "target_blueprint": "/Game/BP/BP_Test.BP_Test",
    "target_graph": "ExecuteUbergraph_BP_Test",
    "compile_after_import": true,
    "json": "{...}"
  }
}
```

字段建议：
- `request_id`: 幂等与链路追踪主键
- `command`: 命令名
- `payload`: 业务参数对象
- `client_id`: 可选，标记请求来源
- `timestamp`: 可选，便于排错

### 5.2 初始响应对象

```json
{
  "request_id": "req_123",
  "success": true,
  "task_id": "task_456",
  "message": "任务已开始"
}
```

适用场景：
- 接收成功但还未执行完成
- 长任务统一进入任务池

### 5.3 任务结果对象

```json
{
  "task_id": "task_456",
  "status": "completed",
  "result": {
    "generated_node_count": 12,
    "unresolved_node_count": 1,
    "unresolved": [
      {
        "display_text": "SpawnActor (Node_7)",
        "reason": "未找到蓝图函数"
      }
    ],
    "compile": {
      "success": false,
      "errors": [
        "Pin type mismatch"
      ],
      "warnings": []
    }
  }
}
```

### 5.4 事件对象
Bridge 层建议至少支持以下异步事件：
- `editor_context_changed`
- `active_graph_changed`
- `selection_changed`
- `task_updated`
- `compile_finished`

这些事件不一定直接暴露给 AI，但应允许 MCP Server 消化成资源刷新通知。

---

## 六、错误模型

### 6.1 错误分类
至少需要区分：
- `invalid_request`
- `editor_not_ready`
- `asset_not_found`
- `graph_not_found`
- `json_parse_failed`
- `node_spawn_failed`
- `link_connect_failed`
- `compile_failed`
- `task_not_found`

### 6.2 错误对象建议

```json
{
  "success": false,
  "error": {
    "code": "json_parse_failed",
    "message": "JSON 解析失败",
    "details": {
      "reader_error": "Invalid token"
    }
  }
}
```

### 6.3 设计原则
- 错误必须结构化
- 日志文本只能作为补充，不是主返回
- 错误码要稳定，便于 MCP Server 与 AI 做规则判断

---

## 七、协议级约束

### 7.1 目标图表必须显式化
写操作建议都要求传入：
- `targetBlueprint`
- `targetGraph`

若没有传入，最多作为降级策略回退到当前焦点图表，不能作为默认唯一行为。

### 7.2 Json 协议必须版本化
建议强制保留：
- `version`
- `schema`

并在校验阶段返回：
- 当前支持版本
- 兼容处理结果
- 弃用字段提示

### 7.3 写操作必须事务化
每次导入都应在独立事务内执行，并返回：
- 事务摘要
- 新增节点数
- 失败节点数

### 7.4 请求必须可追踪
建议所有变更型请求都带：
- `request_id`
- `task_id`
- `correlation_id`

---

## 八、安全与稳定性

### 8.1 本地安全建议
- Bridge 默认只监听本机
- 若使用 WebSocket，仅绑定 `127.0.0.1`
- 若使用 Named Pipe，限制命名空间与客户端来源
- Bridge 层可加入本地 token 校验

### 8.2 生命周期建议
- UE 编辑器启动时注册 Bridge
- UE 关闭时主动发送 `editor_offline`
- MCP Server 需检测 UE 不在线并返回明确错误
- AI 不应把“连接建立成功”等同于“任务执行成功”

### 8.3 幂等性建议
对于 `import_json_to_graph` 一类写操作，建议支持：
- 调用级 request id
- 最近一次任务缓存
- 重复请求保护

---

## 九、MVP 协议范围
第一阶段建议仅实现以下命令：

1. `get_rule_markdown`
2. `export_selection_to_json`
3. `import_json_to_graph`
4. `compile_blueprint`
5. `get_task_result`

这样可以先跑通最关键的 AI 闭环：

1. 读规则
2. 读当前选择 Json
3. 生成 Json
4. 导入蓝图
5. 读编译结果

---

## 十、结论
协议设计上最重要的原则不是“把所有 Unreal 能力暴露出去”，而是：

1. 用 MCP 给 AI 暴露高层语义能力
2. 用 Bridge 给 UE 暴露稳定、可回滚、可诊断的执行命令
3. 用任务与资源模型管理长流程，而不是把所有结果塞进一次同步调用

按这个方向推进，协议层会足够稳，也能支持后续的节点扩展、错误修复和上下文通知。