# BridgePayload Object-First — 基线指标

> **创建日期:** 2026-05-01
> **用途:** 记录 object-first 改造前后的度量对比基线，用于回归验证。

## 1. 度量定义

| 度量名称 | 说明 | 采集方式 |
|---|---|---|
| Bridge 响应字节数 | `export_to_json` 返回的完整 JSON payload 字节数 | 统计 `FJsonObject` serialize 后的字符数 |
| `result.json` 转义序列数 | 旧协议中 `result.json` 作为 string 时内部 `\"` 的数量 | 正则 `\\"` 匹配计数 |
| RawJson 节点数 | `payload.nodes` 或解析后 `nodes` 数组长度 | `TArray::Num()` |
| RawJson 连线数 | `payload.links` 或解析后 `links` 数组长度 | `TArray::Num()` |
| MCP 默认输出 content 条目数 | MCP 工具返回 `content[]` 的 item 数量 | TypeScript `content.length` |

## 2. 改造前基线（Legacy String-First）

使用固件 `small_raw_json_legacy_response.json` 采集：

| 度量 | 值 |
|---|---|
| Bridge 响应字节数 | ~680 bytes（含转义 string） |
| `result.json` 转义序列数 | ~60+（每个 `"` 和 `\` 都需要转义） |
| RawJson 节点数 | 1 |
| RawJson 连线数 | 0 |
| MCP 默认 content 条目数 | 2（text + resource_link） |

## 3. 改造后目标（Object-First）

使用固件 `small_raw_json_payload_response.json` 采集：

| 度量 | 值 |
|---|---|
| Bridge 响应字节数 | ~780 bytes（无转义开销，但增加了 `payload` 冗余） |
| `result.json` 转义序列数 | 0（`result.json` 直接是对象，不经过字符串序列化） |
| RawJson 节点数 | 1（与 legacy 一致） |
| RawJson 连线数 | 0（与 legacy 一致） |
| MCP 默认 content 条目数 | 2（text + resource_link，保持不变） |

## 4. 迁移收益总结

1. **消除双重序列化** — Bridge 不再需要 `JsonObject → string → JsonObject` 的往返，减少 CPU 开销。
2. **消除转义** — 下游消费者（MCP、Agent）直接拿到对象，无需 `JSON.parse()`。
3. **兼容性保留** — 通过 `include_json_text` 和 legacy `json_text` 字段，旧消费者仍可工作。
4. **Resource 直出** — MCP RawJson resource 返回 RawJson 本体而非 `{ json: ... }` 包裹对象。

## 5. 验证固件

| 固件 | 路径 |
|---|---|
| Legacy Bridge 响应 | `Resources/TestFixtures/BridgePayloadObjectFirst/small_raw_json_legacy_response.json` |
| Target Bridge 响应 | `Resources/TestFixtures/BridgePayloadObjectFirst/small_raw_json_payload_response.json` |
