# BridgePayloadObjectFirst 测试固件

本目录包含 Bridge Payload Object-First 改造的基线固件（fixture），用于对比 string-first（改造前）与 object-first（改造后）的 Bridge 响应形状。

## 固件说明

| 固件文件 | 用途 |
|---|---|
| `small_raw_json_legacy_response.json` | 改造前 Bridge `export_to_json` 返回的响应，`result.json` 为序列化字符串。 |
| `small_raw_json_payload_response.json` | 改造后目标响应，`result.payload` 和 `result.json` 均为对象。 |
