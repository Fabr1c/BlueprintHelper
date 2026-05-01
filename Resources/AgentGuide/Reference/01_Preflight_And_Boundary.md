# 01 - Preflight 与能力边界

## 1. 使用前判断

Agent 每次收到任务后，先判断任务属于哪一类：

```text
A. UE 编辑器资产操作 -> 使用 BlueprintHelper MCP
B. 源码 / 配置 / 文档修改 -> 使用普通文件与代码工具
C. 两者都有 -> 先分解任务，源码部分不用 MCP，资产部分用 MCP
```

## 2. BlueprintHelper MCP 适用范围

适用：

- 蓝图资产浏览、搜索、打开、保存、信息查询。
- 蓝图图表读取、导入导出、节点增删、变量增删、函数图/宏图/事件分发器处理。
- 蓝图编译、校验、JSON / 逻辑视图导出。
- UMG WidgetTree 读取、添加、删除、移动、属性读写。
- UObject / DataAsset 属性读写。
- DataTable 行读取、添加、更新、删除。
- PIE 启停、Undo / Redo、控制台命令、关闭编辑器等编辑器命令。
- 在 MCP server 侧具备环境变量时，启动 Unreal Editor 或执行项目编译。

不适用：

- 全仓库代码搜索。
- C++ / TypeScript / Python / shell 脚本编辑。
- `.uproject`、`.uplugin`、`.Build.cs`、`.Target.cs`、配置文件直接改写。
- 生成 AGENTS.md、Codex memory、普通项目文档。
- 任何不需要 Unreal Editor 参与的纯文本文件操作。

## 3. 连接前置条件

调用资产类工具前，应满足：

1. Unreal Editor 已打开目标项目。
2. BlueprintHelper Bridge 插件已加载并监听本地端口。
3. MCP Server 能连接 Bridge，默认是 `127.0.0.1:54321`，具体以当前配置为准。
4. 若需要由 MCP 启动编辑器，必须存在：
   - `UE_ENGINE_DIR`
   - `UE_PROJECT_FILE`
5. 若需要项目编译，编辑器通常应先关闭，避免文件锁和热重载不确定性。

## 4. 写操作强约束

写入前必须明确：

| 必需信息 | 示例 | 原因 |
|---|---|---|
| 资产路径 | `/Game/Blueprints/BP_Player` | 防止误改当前焦点资产 |
| 图表名称 | `EventGraph` | 防止节点写入错误图表 |
| 操作类型 | 添加变量 / 删除节点 / 设置属性 | 降低破坏面 |
| 验证方式 | 编译 / 导出 / 读取回查 | 确认结果可用 |

对于删除、重命名、断线、批量移动等破坏性操作，必须先读取现状并列出将要影响的对象。

## 5. 默认任务循环

```text
Understand user request
 -> decide MCP or normal code tool
 -> if MCP: preflight Bridge/editor/asset path
 -> read current state
 -> create minimal edit plan
 -> perform edit
 -> compile/validate
 -> save if appropriate
 -> report exact changes and remaining risks
```
