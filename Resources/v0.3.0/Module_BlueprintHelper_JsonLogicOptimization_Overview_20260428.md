# BlueprintHelper JSON / Agent 逻辑视图优化总览（2026-04-28）

## 一、修改原因

### 1.1 当前 JSON 同时承担两种不同职责

当前 `export_to_json` 返回的是 `BlueprintHelper.JsonToBlueprint` 协议 JSON。该 JSON 对导入和回放是必要的，但它不适合作为 Agent 的主要阅读格式。

它包含大量低层字段：

- 节点类型、节点 ID、节点 GUID
- 坐标 `x` / `y`
- Pin 名称和默认值
- 原始 links
- 变量、函数、宏、图表和蓝图级操作字段

这些字段对重建蓝图很重要，但对 Agent 判断“这段蓝图逻辑做了什么”并不直接。

### 1.2 Agent 需要的是逻辑摘要，不是完整节点转储

Agent 在理解蓝图时更关注：

- 入口事件是什么
- 执行流如何展开
- 分支条件是什么
- 循环边界是什么
- 调用了哪些函数
- 读写了哪些变量
- 数据依赖从哪里流向哪里
- 哪些节点孤立或未连接

这些信息可以从原始 JSON 派生，但不应该要求 Agent 每次都自行解析原始节点和 Pin。

### 1.3 直接裁剪 raw JSON 会破坏导入链路

不建议把现有 raw JSON 改成短格式，原因是：

- 导入器依赖完整节点字段。
- fixture 和回归测试依赖可回放格式稳定。
- 增量编辑需要稳定节点引用，例如 `node_guid`。
- 坐标、Pin、默认值等字段对还原编辑器图形布局有价值。

因此优化方向应该是新增派生视图，而不是替换原协议。

### 1.4 MCP 返回需要按场景分层

当前 Bridge 的 `export_to_json` 将导出结果作为字符串放入：

```json
{
  "json": "{...raw json...}"
}
```

这种形式有两个问题：

1. JSON 被二次字符串化，Agent 需要额外解析。
2. 无法区分“给机器回放的完整 JSON”和“给 Agent 理解的短逻辑摘要”。

---

## 二、修改目标

### 2.1 保留 raw JSON 的权威性

`BlueprintHelper.JsonToBlueprint` 继续作为唯一可导入、可回放、可测试的完整协议。

### 2.2 新增逻辑视图

新增两类派生输出：

| 格式 | 用途 |
|------|------|
| `logic_json` | 结构化摘要，适合 Agent 程序化分析 |
| `logic_md` | 可读摘要，适合聊天上下文和人工审查 |

### 2.3 降低 Agent 上下文负担

对于读取蓝图逻辑类工具，MCP 默认应返回 `logic_md` 或 `logic_json`，只有在需要修改、复制、回放时才返回 `raw_json`。

### 2.4 保持向后兼容

所有新增字段和命令都应满足：

- 不破坏现有 `import_json`
- 不破坏现有 `validate_json`
- 不破坏现有 fixtures
- 不要求老 Agent 立即迁移

---

## 三、总体方案

### 3.1 新增逻辑处理器

新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperLogicProcessor.h
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicProcessor.cpp
```

处理链路：

```text
raw JSON -> parse -> semantic graph -> logic_json / logic_md
```

### 3.2 Bridge 新增导出逻辑命令

推荐新增命令：

```text
export_logic
```

请求示例：

```json
{
  "command": "export_logic",
  "payload": {
    "target_blueprint": "/Game/BP/BP_Player.BP_Player",
    "target_graph": "EventGraph",
    "scope": "single_graph",
    "format": "logic_json",
    "detail": "normal",
    "include_data_dependencies": true,
    "include_orphans": true
  }
}
```

### 3.3 可选扩展 `export_to_json`

如需减少命令数量，也可以扩展现有 `export_to_json`：

```json
{
  "format": "raw_json | logic_json | logic_md"
}
```

但更推荐新增 `export_logic`，因为它语义更清晰，风险更低。

---

## 四、非目标

本优化不在第一阶段处理以下内容：

- 不重写 `FBlueprintToTextConverter`
- 不重写 `TextToBlueprintGenerator`
- 不改变 raw JSON 的导入语义
- 不把 logic JSON 设计成可导入协议
- 不让 Agent 依赖当前编辑器焦点进行写操作

---

## 五、风险判断

| 风险 | 等级 | 说明 | 控制方式 |
|------|------|------|----------|
| 逻辑摘要误判执行线 | 中 | 当前 links 缺少明确 Pin 类型 | 先用启发式，后续补 `kind` 字段 |
| 破坏现有导入 | 低 | 只新增派生层，不改导入 | 保留 raw JSON 不变 |
| 输出过度压缩导致信息不足 | 中 | Agent 修改蓝图时仍需完整信息 | 允许 `detail=debug` 和 raw fallback |
| 多图蓝图摘要复杂 | 中 | 函数图、宏图、事件图语义不同 | 第一阶段按 graph 分组输出 |

---

## 六、最终结论

应新增 LogicProcessor 派生层，将现有 raw JSON 转换为面向 Agent 的短逻辑 JSON / Markdown。

推荐默认策略：

- 读逻辑：`export_logic format=logic_md`
- 做结构化分析：`export_logic format=logic_json`
- 准备写回或复制：`export_to_json` 返回 raw JSON

