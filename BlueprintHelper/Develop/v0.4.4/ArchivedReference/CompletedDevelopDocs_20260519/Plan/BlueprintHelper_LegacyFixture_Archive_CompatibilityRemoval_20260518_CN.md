# BlueprintHelper Legacy TestFixtures 归档与兼容字段移除

日期：2026-05-18

## 目标

`BlueprintHelper/Develop/TestFixtures` 下的 MCPRegression、AgentImport、LogicProcessor、BridgePayloadObjectFirst fixtures 是 v0.3.x 时代的旧实现/兼容样例，不再作为当前 AgentFace、CLI、AgentGuide、TaskSpec-first 或 MCP 字段兼容依据。

本次处理目标：

1. 将旧 `Develop/TestFixtures` 内容移入版本归档目录。
2. 当前 AgentFace/MCP schema 不再暴露这些旧字段。
3. 当前 Bridge validator 和 C++ shared service 不再兼容这些旧字段。
4. 测试语义从“兼容旧字段”改为“拒绝旧字段”。

## 归档位置

- 旧入口已移除：`BlueprintHelper/Develop/TestFixtures`
- 归档位置：`BlueprintHelper/Develop/v0.4.3/ArchivedReference/LegacyTestFixtures_20260518`
- 归档说明：`BlueprintHelper/Develop/v0.4.3/ArchivedReference/LegacyTestFixtures_20260518/README.md`

## 已移除兼容字段

| 类别 | 旧字段/旧值 | 当前行为 |
|---|---|---|
| MCP/Bridge export scope | `full_graph`, `full_blueprint`, `single_graph` | 拒绝旧值；当前只接受 `graph`, `blueprint`, `selection` 或按工具上下文限制子集 |
| RawJson string-first 输出 | `include_json_text`, `json_text`, `legacy_text_json` | 不再生成、不再解析、不再作为 schema 选项 |
| RawJson import | string-valued `json` | 拒绝；`payload.json` 必须是 object |
| AgentImport 旧语义形状 | `nodes`, `links`, `declarations`, `layout` | `blueprint_import_agent_graph` 拒绝旧字段；当前要求 `logic_spec` object |
| C++ shared request/result | `JsonText`, `bIncludeJsonText` | 类型字段已移除 |

## 代码同步结果

- AgentFace MCP result normalizer 不再解析 string `json`、`json_text` 或 `legacy_text_json`。
- AgentFace MCP tool schema 不再接受旧 response mode、旧 export scope 和旧 agent graph payload 形状。
- Bridge validator 对旧字段执行显式拒绝，避免 Agent 继续误以为存在兼容路径。
- Bridge router 和 import/export shared service 已移除旧字段读写分支。
- 相关 object-first 测试已改为覆盖旧字段拒绝行为。

## 验证记录

| 验证项 | 结果 |
|---|---|
| `npm.cmd --prefix AgentFaceService\task-core run build` | 通过 |
| `npm.cmd --prefix AgentFaceService\mcp run build` | 通过 |
| `npm.cmd --prefix AgentFaceService\mcp run test:node` | 通过，12 项 |
| `npm.cmd --prefix AgentFaceService\task-core run test:node` | 通过，111 项 |
| `git diff --check` | 通过；仅有行尾提示 |
| `Test-Path BlueprintHelper\Develop\TestFixtures` | `False` |
| 归档文件存在性检查 | 通过 |
| 排除归档目录后的旧字段扫描 | 无命中 |
| `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` | 通过 |

## 当前状态

完成。旧 fixtures 已归档，当前实现不再兼容这些字段。
