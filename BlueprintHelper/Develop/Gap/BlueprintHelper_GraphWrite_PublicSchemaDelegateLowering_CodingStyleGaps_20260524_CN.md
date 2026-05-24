# BlueprintHelper GraphWrite Public Schema / Delegate Lowering CodingStyle Gaps 2026-05-24

## 审计结论

当前 public schema / compiler lowering / C++ parser 边界总体符合本轮架构取舍：

- Agent-facing 仍允许 `delegate.bind` / `delegate.assign` / `delegate.unbind` / `delegate.unbind_all` / `delegate.call` 这类压缩输入。
- Python compiler 负责 lowering 到 canonical internal shape：`kind="delegate"` + `delegate_operation`，并为 `unbind` / `clear` 补齐 `unbind_mode`。
- C++ SemanticIR parser 只消费 lowering 后的 internal schema，不接受 dotted public delegate kinds。
- 当前实现保持强类型二级字段 `delegate_operation`，没有引入弱类型可选 `intent` 字段。

因此，本次审计没有发现需要推翻当前 delegate-first scope 的架构问题。下面记录的是仍需关闭的 CodingStyle / 通用性 / 高内聚低耦合 / hardcoded-content 差距。

## 新增 Gap 1. Delegate operation runtime invariant tests 仍不足

状态：未关闭。

严重度：Medium。

涉及范围：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
- `AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py`

当前证据：

- Python compiler 测试覆盖了 public `delegate.*` lowering 到 `kind="delegate"` + `delegate_operation`。
- C++ runtime fact 测试覆盖了 dotted public kinds 被 parser 拒绝，以及 canonical `kind="delegate"` 被接受。
- C++ parser 代码实际拥有 operation-specific invariant：`delegate_operation` 必填、operation 必须在支持集合内、`bind/assign/unbind` 需要 handler、`unbind` 需要 `unbind_mode=single`、`clear` 需要 `unbind_mode=all` 且不能携带 handler。

差距：

- C++ runtime fact 还没有独立断言上述 operation-specific invariant。
- 如果后续 C++ parser 校验退化，现有 TypeScript/Python contract 测试不一定能发现 runtime parser 行为漂移。

关闭标准：

- 增加 GraphSemanticIR runtime fact tests，至少覆盖：
  - `kind="delegate"` 缺失 `delegate_operation` -> `delegate_operation_missing`。
  - 不支持的 `delegate_operation` -> `delegate_operation_unsupported`。
  - `delegate_operation="bind"` / `"assign"` / `"unbind"` 缺失 handler -> `delegate_handler_missing`。
  - `delegate_operation="unbind"` 缺失或错误 `unbind_mode=single` -> `delegate_unbind_mode_single_missing`。
  - `delegate_operation="clear"` 带 handler 或错误 `unbind_mode` -> 对应 diagnostic。
- 测试必须直接经过 C++ `BuildFromLogicSpec` / SemanticIR parser，不只停留在 Python lowering。

## 新增 Gap 2. Delegate vocabulary 在多层重复硬编码

状态：未关闭。

严重度：Low。

涉及范围：

- `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- `AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

当前证据：

- public delegate kinds、internal kind、operation kinds、forbidden top-level kinds 在 TypeScript contract、Python compiler、Python tests、C++ tests/source contract 中重复出现。
- 当前重复是有意的边界冻结手段，短期可接受；但它不是长期的单一事实源。

差距：

- 新增或重命名 delegate operation 时，需要同步修改多处硬编码字符串集合。
- 这会增加 contract / compiler / parser / tests 漂移风险，属于 hardcoded-content 与 cohesion 风险，而不是当前行为 bug。

关闭标准：

- 建立一个单一事实源，至少满足以下之一：
  - 由 TypeScript contract 中的 boundary spec 生成 Python compiler allow/forbid map 与测试 fixture。
  - 新增共享 manifest，例如 `GraphWriteSemanticBoundary.v1.json`，由 TS/Python/C++ source-contract tests 消费。
  - 新增专门的 schema-export 验证，确保 TS contract、Python compiler、C++ parser test token sets 完全一致。
- 保持强类型字段，不引入泛化可选 `intent`。
- C++ parser 仍只消费 canonical internal schema，不直接消费 public dotted forms。

## 明确非 Gap 项

以下点本次审计判定为符合当前架构边界，不作为 gap 记录：

- delegate-first scope 本身不是硬编码违规；当前 delegate 是一个明确 semantic family，使用 `delegate_operation` 是强类型二级语义。
- `component_bound_event` 作为 use-site 特例保留是合理的，因为它对应 UE component-bound event node 的独立 spawner 语义。
- C++ parser 拒绝 `delegate.bind` 等 public dotted kinds 是正确边界；public 压缩输入只应存在于 Agent-facing TaskSpec 和 compiler lowering 前。
- 不引入通用可选 `intent` 是正确选择；后续 construct/deconstruct/convert 应按各自 semantic family 设计强类型 operation 字段。

## 当前建议

优先关闭 Gap 1，因为它直接影响 runtime parser contract 的防回退能力。

Gap 2 可以作为后续 taxonomy consolidation 的一部分处理；在收敛 get/set、construct/deconstruct/convert 等 semantic family 时，一并建立跨语言单一事实源更合适。
