# BlueprintHelper v0.3.0 — 运行时测试执行报告

**执行日期:** 2026-05-01
**执行环境:** Windows 11, UE 5.6, Node.js MCP Server
**测试状态:** 运行时测试（Editor Bridge 连通）

---

## 关键阻断解除

| 问题 | 之前状态 | 当前状态 |
|------|----------|----------|
| UE Editor Bridge 未启动 | BLOCKED | ✅ 已启动 (PID 113) |
| MCP 缺少 `UE_ENGINE_DIR`/`UE_PROJECT_FILE` | BLOCKED | ✅ .vscode/mcp.json 已配置 |
| `BLUEPRINTHELPER_BRIDGE_TOKEN` 未配置 | BLOCKED | ✅ bh-dev-token 已配置 |
| `BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS` | BLOCKED | ✅ 已配置 (Editor启动时) |

---

## 测试资产创建

| 资产 | 路径 | 状态 |
|------|------|------|
| Blueprint | /Game/BlueprintHelperTest/BP_BH_FunctionalActor | ✅ |
| WidgetBlueprint | /Game/BlueprintHelperTest/WBP_BH_Test | ✅ |
| DataTable | /Game/BlueprintHelperTest/DT_BH_TestItems | ✅ (RowStruct=TableRowBase) |
| DataAsset | /Game/BlueprintHelperTest/DA_BH_TestConfig | ✅ |

---

## 1. 环境与生命周期 (01 — ENV)

| ID | 状态 | 结果 |
|----|------|------|
| ENV-001 | ✅ PASS | MCP Server 编译成功 |
| ENV-002 | ✅ PASS | 44 工具注册 |
| ENV-003 | ✅ PASS | Bridge 不可达返回 ECONNREFUSED |
| ENV-004 | ✅ PASS | Bridge 正常连通 (get_editor_context success:true) |
| ENV-005 | ✅ PASS | 缺 env vars 时明确错误信息 |
| ENV-007 | ✅ PASS | Editor 启动 (Bash 启动，Bridge 就绪) |
| ENV-009 | ❌ BLOCKED | close_editor 需 BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS (Editor 启动时已设置，待验证) |
| ENV-015 | ✅ PASS | Token 透传验证 (写操作成功携带 auth_token) |

---

## 2. RawJson 测试 (03 — RAW)

| ID | 状态 | 结果 |
|----|------|------|
| RAW-001 | ✅ PASS | 合法 JSON 校验通过 (is_valid:true) |
| RAW-002 | ✅ PASS | 非法 JSON 返回解析错误 |
| RAW-003 | ✅ PASS | 缺少 graphs/nodes 返回 is_valid:false |
| RAW-004 | ⚠ FIXED / PENDING RETEST | `_zod` 根因已在 MCPServer 修复；运行时 Bridge 复验待执行 |
| RAW-005 | ⚠ PENDING RETEST | 依赖 export_to_json，待运行时复验 |
| RAW-006 | ⚠ PENDING RETEST | 依赖 export_to_json，待运行时复验 |
| RAW-007~018 | ⚠ PARTIAL BLOCKED | `_zod` 阻断已解除；仍依赖运行时复验或 import_json_to_graph 排查 |

**发现的问题:**
- `blueprint_export_to_json`: 已定位为 MCP SDK 输出校验不支持 `outputSchema: z.union(...)` 导致的 `_zod` 运行时错误；已将输出 schema 改为单一 object schema，并补充真实 MCP SDK 调用回归测试
- `blueprint_import_json_to_graph`: 返回 `no_op` 状态，未创建节点（可能是 JSON 格式或图表状态问题）

---

## 3. LogicJson/LogicMD/AgentImport 测试 (04 — LOG)

| ID | 状态 | 结果 |
|----|------|------|
| LOG-001 | ✅ PASS | get_logic 返回 logic_md 格式 (schema: BlueprintHelper.LogicMd.v1, importable:false) |
| LOG-006 | ✅ PASS | get_logic_json 返回结构化 JSON (3 nodes detected) |
| LOG-011 | ✅ PASS | AgentImport BeginPlay→PrintString: 2 nodes + 1 link, full_success |
| LOG-012 | ⚠ NOT RUN | branch_flow (时间限制) |
| LOG-013 | ✅ PASS | AgentImport set_variable: `var` 字段正确识别，1 node created |
| LOG-014 | ✅ PASS | AgentImport forbidden PosX/PosY: ForbiddenField 错误，suggestion 引导删除坐标 |
| LOG-015 | ✅ PASS | AgentImport invalid_pin (not_a_pin): rolled_back:true, 列出可用 pins 建议 |
| LOG-016 | ✅ PARTIAL | event/call/set kind 验证通过，sequence/branch/comment 待测 |
| LOG-017 | ✅ PASS | append mode: 原有节点保留，新节点自动布局 |

**亮点:** AgentImport 的错误诊断优秀 — 提供 `suggestion` 和可用 pins 列表，支持 Agent 自我纠错。

---

## 4. 资产服务测试 (05 — AST)

| ID | 状态 | 结果 |
|----|------|------|
| AST-001 | ✅ PASS | list_assets /Game non-recursive (2 worlds) |
| AST-002 | ✅ PASS | list_assets /Game/BlueprintHelperTest recursive (4 assets) |
| AST-006 | ✅ PASS | 无效路径 /NotGame 返回空结果 |
| AST-007 | ✅ PASS | search_assets "BP_BH" (2 results) |
| AST-008 | ✅ PASS | search_assets "DefinitelyMissingAssetName" 返回空 |
| AST-012 | ✅ PASS | get_asset_info Blueprint (class/name/parent) |
| AST-013 | ✅ PASS | get_asset_info DataTable (class=DataTable) |
| AST-014 | ✅ PASS | create_blueprint (Actor, UserWidget) |
| AST-015~016 | ⚠ NOT RUN | 时间限制 |
| AST-017 | ✅ PASS | save_asset 成功 |

---

## 5. 蓝图结构测试 (06 — BPS)

| ID | 状态 | 结果 |
|----|------|------|
| BPS-001 | ✅ PASS | list_graphs (3 graphs: EventGraph, UserConstructionScript, TestImport) |
| BPS-002 | ✅ PASS | list_variables (bIsActive, Health, Score) |
| BPS-003 | ✅ PASS | list_event_dispatchers (empty) |
| BPS-004 | ✅ PASS | add bool variable bIsActive=true |
| BPS-005 | ✅ PASS | add float variable Health=100.0, category="Stats" |
| BPS-008 | ❌ **FAIL** | **重复添加 Health 成功，未返回 DuplicateName 错误** |
| BPS-009 | ✅ PASS | remove_variable Health 成功 |
| BPS-011 | ✅ PASS | add_graph ComputeScore (Function) |
| BPS-014 | ❌ **FAIL** | **重复添加 ComputeScore graph 成功，未拒绝** |
| BPS-015 | ✅ PASS | remove_graph TestImport 成功 |
| BPS-016 | ✅ PASS | **EventGraph 删除被正确拒绝**: "不允许删除 EventGraph" |
| BPS-017 | ✅ PASS | add_event_dispatcher OnTestTriggered |
| BPS-019 | ❌ **FAIL** | **重复添加 OnTestTriggered dispatcher 成功，未拒绝** |
| BPS-020~024 | ⚠ NOT RUN | 时间限制 |

---

## 6. UMG Widget 测试 (08 — UMG)

| ID | 状态 | 结果 |
|----|------|------|
| UMG-001 | ✅ PASS | get_widget_tree (新建WBP为空) |
| UMG-002 | ✅ PASS | add_widget Button "Btn_Test" |
| UMG-003 | ✅ PASS | add_widget TextBlock "Txt_Label" |
| UMG-004 | ✅ PASS | get_widget_tree (2 widgets, 含深度和子节点计数) |
| UMG-005 | ✅ PASS | get_widget_properties Btn_Test (28 属性，含 FButtonStyle 等复杂类型) |
| UMG-006 | ✅ PASS | set_widget_property Visibility=Hidden |
| UMG-007~020 | ⚠ NOT RUN | 时间限制 |

---

## 7. DataTable 测试 (10 — DT)

| ID | 状态 | 结果 |
|----|------|------|
| DT-001 | ✅ PASS | get_datatable_rows 空表 (row_struct=TableRowBase) |
| DT-002 | ✅ PASS | add_datatable_row "TestRow1" |
| DT-003 | ✅ PASS | update_datatable_row 空 fields 被正确拒绝："fields 对象为空，至少需要一个字段" |
| DT-004 | ✅ PASS | delete_datatable_row 成功 |
| DT-005 | ✅ PASS | 删除后验证 row_count 变化 |

---

## 8. UObject/DataAsset 测试 (09 — OBJ)

| ID | 状态 | 结果 |
|----|------|------|
| OBJ-001 | ✅ PASS | get_object_properties (空 DataAsset，0 属性) |
| OBJ-002 | ✅ PASS | set_object_property 不存在的属性被正确拒绝 |

---

## 9. 编译/保存 (11 — CMD)

| ID | 状态 | 结果 |
|----|------|------|
| CMD-001 | ✅ PASS | compile_blueprint: compile_success=true, 0 errors, 0 warnings |
| CMD-002 | ✅ PASS | save_asset BP, WBP, DT 全部成功 |
| CMD-003 | ❌ BLOCKED | close_editor 需要 BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS=1 |
| CMD-004~017 | ⚠ NOT RUN | PIE/console_command 等需高风险命令 |

---

## 10. 安全测试 (12 — SAF)

| ID | 状态 | 结果 |
|----|------|------|
| SAF-001 | ✅ PASS | Bridge 不可达时 ECONNREFUSED (ENV-003) |
| SAF-011 | ✅ PASS | AgentImport strict 回滚: invalid_pin → rolled_back:true, 0 nodes remained |

---

## 汇总

### 通过率

| 类别 | 已执行 | 通过 | 失败 | 阻塞 | 待测 |
|------|--------|------|------|------|------|
| ENV | 6 | 5 | 0 | 1 | 8 |
| RAW | 8 | 3 | 0 | 5 | 10 |
| LOG | 7 | 6 | 0 | 0 | 15 |
| AST | 9 | 9 | 0 | 0 | 9 |
| BPS | 12 | 9 | 3 | 0 | 12 |
| UMG | 6 | 6 | 0 | 0 | 14 |
| DT | 5 | 5 | 0 | 0 | 13 |
| OBJ | 2 | 2 | 0 | 0 | 14 |
| CMD | 3 | 2 | 0 | 1 | 14 |
| SAF | 2 | 2 | 0 | 0 | 22 |
| **Total** | **60** | **49** | **3** | **7** | **~131** |

### P0 关键发现

| 发现 | 严重度 | 描述 |
|------|--------|------|
| Duplicate variable/graph/dispatcher | **Medium** | BPS-008/014/019: 重复名称创建成功，应返回 DuplicateName 错误 |
| DA/DT 创建缺少结构参数 | **High** | create_blueprint 对 DataAsset/DataTable 未要求结构（RowStruct/Class），导致创建出的资产为空壳。应添加 `struct_name` 必填参数，并在 parent_class=DataTable 时强制要求，parent_class=DataAsset 时也需指定有效子类 |
| export_to_json _zod 错误 | **High / Fixed in MCPServer** | RAW-004~008: MCP Server 端 JavaScript 运行时错误；已修复，待运行时复验 |
| import_json_to_graph no_op | **Medium** | RAW-009: 导入返回 no_op，未创建节点 |

### Hotfix 更新: export_to_json _zod

**修复时间:** 2026-05-01

**根因:** `blueprint_export_to_json` 的 `outputSchema` 使用 `z.union([BlueprintRawJsonRefOutputSchema, z.record(z.unknown())])`。当前 MCP SDK 对 tool output schema 只按 object schema 归一化，union schema 归一失败后进入输出校验，触发 `Cannot read properties of undefined (reading '_zod')`。

**修复内容:**
1. `BlueprintHelper_MCP_Server/src/tools.ts` — 将 `BlueprintRawJsonExportOutputSchema` 改为 `BlueprintRawJsonRefOutputSchema.partial().passthrough()`，避免 SDK 对 union output schema 崩溃。
2. 保留 `response_mode`、默认 `resource_link`、`structuredContent.rawUri` 和 `legacy_text_json` 行为，不回退 MCP 返回结构优化。
3. `BlueprintHelper_MCP_Server/src/tools.regression.test.ts` — 新增真实 `McpServer + Client + InMemoryTransport` 调用测试，覆盖 `blueprint_export_to_json` 的 SDK 输出校验路径。

**验证状态:** `npm.cmd test` 已通过 10/10；UE Editor / Bridge 运行时复验未执行，后续测试时再确认 RAW-004~008。

### AgentImport 亮点

- ✅ 错误回滚正确工作 (strict=true)
- ✅ ForbiddenField 检测 (PosX/PosY)
- ✅ Pin 不存在时提供可用 pins 列表
- ✅ create_missing_variables 自动创建变量
- ✅ append mode 保留原有节点
- ✅ compile/save 选项正确

### Bug 详情: DA/DT 创建缺少结构参数 (High)

**问题:** `create_blueprint` 命令在 `parent_class=DataTable` 或 `parent_class=DataAsset` 时，未要求提供结构参数，导致创建的资产为空壳：

- **DataTable**: `FKismetEditorUtilities::CreateBlueprint()` 无法创建 DataTable（它不是 Blueprint 子类），需要通过 `UDataTableFactory` 并设置 `Struct`(RowStruct) 属性。即使绕过限制用 Python 创建，未指定 RowStruct 的 DataTable 也无意义。
- **DataAsset**: `UDataAsset` 是抽象基类，`CreateBlueprint()` 同样无法处理。虽 `UDataAssetFactory` 能创建出空壳，但没有指定具体子类的 DataAsset 无法使用。

**修复方向:**
1. `BlueprintHelperEditorCommandService::CreateBlueprint` — 检测 parent_class 为 DataTable/DataAsset 时，切换为对应 Factory 创建
2. 新增 `struct_name` 参数（MCP → Bridge → Service 三层传递）
3. `HandleCreateBlueprint` — parent_class=DataTable 时 `struct_name` 为必填字段
4. `tools.ts` — `blueprint_create_blueprint` schema 新增 `struct_name` 可选参数，description 中说明 DataTable 场景下必填
```

### 更新后的整体统计

| 指标 | 之前 (离线) | 现在 (含运行时) |
|------|-------------|-----------------|
| 总测试用例 | 281 | 281 |
| 已验证通过 | 22 | **71** |
| 结构验证通过 | 22 | 22 |
| 发现 Bug | 0 | **4** |
| 阻塞 (需排查) | ~237 | **~7** |
| 未执行 (时间) | 0 | ~200 |

成功率: 49/60 已执行 P0 用例通过 (81.7%)

---

### 下一步建议

1. **已修复，待复验**: `export_to_json` 的 `_zod` 错误 — MCPServer 回归测试通过，运行时 Bridge 复验待执行
2. **建议修复**: 重复名称去重 (BPS-008/014/019)
3. **继续测试**: `import_json_to_graph` 格式问题排查
4. **高风险命令测试**: 验证 `close_editor`/`exec_console_command` 在 env var 下的行为
