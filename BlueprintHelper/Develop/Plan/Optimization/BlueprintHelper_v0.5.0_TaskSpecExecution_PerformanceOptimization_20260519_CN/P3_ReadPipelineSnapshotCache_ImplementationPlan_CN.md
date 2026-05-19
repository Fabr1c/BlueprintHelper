# P3 读链路快照、缓存与 Bridge Gap 收口实施计划

日期：2026-05-19

## 目标

P3 承接当前 v0.5.0 文档中的 R1-R5。它不是把 UE 读操作并发化，而是在 GameThread 读取 UE 对象后，把纯 DTO 格式化、同请求快照复用、纯数据缓存和 Bridge gap 细分做成通用架构。

读链路必须坚持：UObject / Blueprint / UEdGraph / UWidgetTree / FProperty 只在 GameThread 读取，后台线程只能处理已经脱离 UE 对象生命周期的 Snapshot DTO。

更细的 R0-R5 执行 checklist、目标文件结构、验收命令和回归门槛见：`R0_R5_ReadPipeline_ExecutablePlan_CN.md`。P3 文档保留阶段背景和架构收口，R0-R5 文档作为实际执行入口。

## 范围

- GameThread Snapshot DTO。
- 后台 formatter。
- request-local snapshot cache。
- 纯数据缓存。
- Bridge gap 拆分：queue、transport、response serialization、AgentFace JSON parse / receive。
- 读工具指标回归门槛。

## 架构边界

- UE read service 负责 GameThread snapshot。
- Formatter 只消费 Snapshot DTO，不访问 UE 对象。
- AgentFace `read_context` 只做 route、Bridge payload、compact/filter 和 result wrap。
- DebugBundle、Review evidence、UI overlay 如需读模型，应复用 Snapshot/DTO，不重新解释 Blueprint 状态。
- 长期资产内容缓存不进 v0.5.0，除非先有明确失效策略。

## DTO formatter migration template

后续 component、WidgetTree、object property 等读工具迁移时复用同一模板：

1. 新建 `SnapshotTypes`：只保存字符串、数字、数组、map、JSON value，不保存 `UObject*` / `UBlueprint*` / `UEdGraph*` / `UWidgetTree*` / `FProperty*`。
2. 新建 `SnapshotService`：唯一职责是在 GameThread 读取 UE 对象并拷贝 DTO；不做 Markdown、compact、filter、UI 解释。
3. 新建 `SnapshotFormatter`：只消费 DTO，可在后台或纯函数路径运行；通过 registry / handler map 扩展格式，不在 route 中写格式特判。
4. route 只负责 parse request、调用 snapshot、调用 formatter、wrap response、记录 timing。
5. request-local cache 只能由 route 创建和销毁；asset 内容 snapshot 不跨 CLI 请求、不跨 editor lifecycle。
6. AgentFace 只做 schema parse、Bridge request、UE timing extract、payload size marker、compact/filter/logic_flow 后处理。

当前已落地的模板实例：

- `FBlueprintHelperLogicReadSnapshotService`
- `FBlueprintHelperLogicReadSnapshotFormatter`
- `FBlueprintHelperLogicReadRequestSnapshotCache`
- `FBlueprintHelperReadCachePolicy`

## 文件结构

### UE Read

- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Read/BlueprintHelperReadSnapshotService.h`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Read/BlueprintHelperReadSnapshotService.cpp`
  - 统一 GameThread snapshot 入口。
- 新增 Snapshot DTO headers
  - `BlueprintHelperBlueprintLogicSnapshot.h`
  - `BlueprintHelperComponentSnapshot.h`
  - `BlueprintHelperWidgetTreeSnapshot.h`
  - `BlueprintHelperObjectPropertySnapshot.h`
  - DTO 可以按领域聚合，行为类仍独立 `.h/.cpp`。
- 新增 formatter classes
  - `BlueprintHelperLogicSnapshotFormatter.h/.cpp`
  - `BlueprintHelperComponentSnapshotFormatter.h/.cpp`
  - `BlueprintHelperWidgetSnapshotFormatter.h/.cpp`
  - `BlueprintHelperObjectPropertySnapshotFormatter.h/.cpp`
- 修改现有 read service
  - `LogicJsonReadService` / `LogicMdReadService` 改为 snapshot + formatter。
  - Component / Widget / ObjectProperty 后续按 timing 证据迁移。
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Read/BlueprintHelperReadRequestSnapshotCache.h`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Read/BlueprintHelperReadRequestSnapshotCache.cpp`
  - 只在一次 CLI/tool request 生命周期内复用。

### AgentFace Read

- 修改 `AgentFaceService/task-core/src/read-context/read-context-runner.ts`
  - 保持 route orchestration。
  - develop timing 追加 `read_context.receive_bridge_payload`、`read_context.parse_bridge_json`、`read_context.payload_size_bytes`。
- 修改 `AgentFaceService/task-core/src/read-context/read-context-result.ts`
  - 统一 payload size 和 compact/filter timing。
- 新增/修改测试
  - `AgentFaceService/task-core/src/tests/read-context/read-context-timing.test.ts`
  - `AgentFaceService/task-core/src/tests/read-context/read-context-payload-size.test.ts`

## Snapshot DTO 规则

- DTO 字段必须是值类型、字符串、数组、map 或自有结构体。
- 禁止保存 `UObject*`、`UEdGraph*`、`UEdGraphNode*`、`UEdGraphPin*`、`UWidget*`、`FProperty*`。
- DTO 必须能在没有 Editor 对象访问的单元测试里格式化。
- cache key 至少包含 asset path、target graph/function/event/block、read detail、format options。
- request 结束后释放 snapshot cache。

## TDD Checklist

### R1 / P3-1 GameThread Snapshot + 后台格式化

- [ ] 测试：Logic formatter 可以只用 `BlueprintLogicSnapshot` 生成 `LogicJson.v1`。
- [ ] 测试：Logic formatter 可以只用 `BlueprintLogicSnapshot` 生成 `LogicMd.v1`。
- [ ] 测试：formatter 不依赖 UObject。
- [ ] UE timing 增加 `snapshot_read` 和 `format_output`。
- [ ] `blueprint_logic_json` / `blueprint_logic_md` 优先迁移到 snapshot + formatter。
- [ ] 输出 schema 与现有 `LogicJson.v1`、`LogicMd.v1`、`ReadContextPack.v1` 保持一致。

### R2 / P3-2 DTO / Formatter 复用

- [ ] 抽出 Logic formatter 边界。
- [ ] 按 timing 证据抽出 Component / Widget / ObjectProperty formatter。
- [ ] `read_context` 不内联领域业务解释。
- [ ] AgentFace compact/filter 不改变 UE DTO 业务含义。
- [ ] DebugBundle / Review evidence / UI overlay 如需读模型，复用同一 Snapshot/DTO。

### R3 / P3-3 同请求快照复用

- [ ] 测试：同一次请求中 `blueprint_logic_json` 与 `blueprint_logic_md` 可以复用同一个 `BlueprintLogicSnapshot`。
- [ ] 测试：不同 target graph/function/event 不误用同一 snapshot。
- [ ] 实现 request-local snapshot cache。
- [ ] 请求结束释放 cache。
- [ ] 不实现跨请求 Blueprint 内容缓存。

### R4 / P3-4 纯数据缓存

- [ ] 测试：runtime profile / capability matrix 缓存不依赖 UObject 生命周期。
- [ ] 缓存仅用于 CLI schema metadata、capability matrix、纯 runtime profile。
- [ ] Blueprint 图、WidgetTree、DataTable rows、DataAsset properties 不进入长期缓存。
- [ ] 如果未来需要资产内容缓存，先定义 package dirty、asset save、editor change event 或 request-local invalidation。

### R5 / P3-5 Bridge Gap 细分和回归指标

- [ ] AgentFace timing 增加 receive / parse / payload size。
- [ ] UE read router timing 增加 response serialization 阶段。
- [ ] 如果 Bridge 层可测，增加 queue wait / transport write / transport read timing。
- [ ] 读优化后重跑当前 11 个 ReadSpec。
- [ ] 每个长读案例记录 min / avg / p50 / max / payload size。

## 验收命令

```powershell
npm.cmd --prefix .\AgentFaceService\task-core run build
npm.cmd --prefix .\AgentFaceService\task-core run test:node -- read-context
E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild
git diff --check
```

读链路测速样本：

```powershell
node .\AgentFaceService\cli\build\cli\index.js read context --spec "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519" --develop --format full
```

## 指标表

| Spec | total_ms | bridge_ms | ue_route_ms | snapshot_read_ms | format_output_ms | payload_size_bytes | bridge_gap_ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 02_blueprint_logic_json.json |  |  |  |  |  |  |  |
| 03_blueprint_logic_md.json |  |  |  |  |  |  |  |
| 11_blueprint_logic_flow.json |  |  |  |  |  |  |  |

完成标准：
- 后台 formatter 不触碰 UObject / Blueprint / UEdGraph / UWidgetTree / FProperty。
- `blueprint_logic_json/md` 至少能显示 `snapshot_read` 与 `format_output` 占比。
- 普通读工具不返回 develop timing。
- 同一 ReadSpec 优化前后 payload schema 兼容。
- Bridge gap 能进一步拆分，不再只有 `bridge - UE route` 单一差值。

## 风险控制

- 不把 full blueprint read 已覆盖的场景改成泛化 batch read 主线。
- 不把用户可编辑资产内容放进长期缓存。
- 不让后台线程直接访问 UE 反射对象。
- 不在 CLI 或 UI 入口写 read type 特判。
