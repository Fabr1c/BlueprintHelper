# Smoke Bug - AssetFactory 2026-05-10

来源：`BlueprintHelper_NewProject_Full_SmokeRun_20260510.md`

本文只记录 AssetFactory / 资产创建链路问题，不记录 ReviewPanel 显示问题。

## SMOKE-AF-20260510-01: Blueprint Interface 被创建成 Actor parent

**优先级**：P0

**现象**

- Smoke 中 `BPI_BHSmokeInteract` 创建成功，但 read-back 显示 parent class 是 `Actor`，不是 Interface。
- 后续 `edit_blueprint_class_settings` 因该资产不是有效接口而 blocked，导致 class settings 环节不能继续验证。

**实现证据**

- `BlueprintHelperAssetFactoryService.cpp` 中 `CreateAsset()` 对 `BlueprintInterface` 分支直接调用 `CreateBlueprintInterface(AssetPath)`。
- `CreateBlueprintInterface()` 只设置 `Factory->BlueprintType = BPTYPE_Interface`，没有显式设置接口 Blueprint 所需的 parent / supported class。
- `TryNormalizeAssetTypeAndParent()` 对 `blueprint_interface` 只映射类型，没有补接口默认 parent。

**初步根因**

Blueprint Interface 的 UE factory 初始化不完整。当前路径依赖 `UBlueprintFactory` 默认值，导致生成出的 Blueprint 资产 parent 落到 Actor，后续所有要求接口资产的能力都会被阻断。

**建议修复**

- 按 UE Interface Blueprint factory 规范显式设置接口 parent / supported class。
- 创建后验证 `GeneratedClass` 或 `SkeletonGeneratedClass` 是接口类，不满足时返回 blocked。
- 增加 automation：`AssetFactoryCreatesBlueprintInterfaceWithInterfaceParent`。

## SMOKE-AF-20260510-02: PrimaryDataAsset 创建被 asset_type_mismatch 阻断

**优先级**：P1

**现象**

- Smoke 中 `DA_BHSmokeData` 创建 blocked，错误为 `asset_type_mismatch for PrimaryDataAsset`。
- 目标 TaskSpec 使用 `asset_type=data_asset` 和 `data_asset_class=/Script/Engine.PrimaryDataAsset`。

**实现证据**

- Python P1 compiler 已复制 `behavior.asset.data_asset_class` 到 `create_asset` op。
- C++ `CreateDataAsset()` 能接收 `DataAssetClass`，并通过 `UDataAssetFactory::DataAssetClass` 创建指定类。
- 但 AssetFactory 的 asset type/collision 判断仍按粗粒度 `DataAsset` 做匹配，不能把 `PrimaryDataAsset` 识别为 DataAsset 子类兼容。
- TypeScript legacy route `blueprint_create_asset` 的输入 schema 也没有暴露 `data_asset_class`，合同面仍存在入口不一致。

**初步根因**

DataAsset 创建能力本身已存在，但类型合同和复用/冲突判断仍是“精确资产类名”思路，没有 class-aware 地处理 DataAsset 子类。

**建议修复**

- DataAsset collision/type check 改为 `ExistingClass IsChildOf RequestedDataAssetClass` 或 `IsChildOf UDataAsset`。
- Task contract 的 `create_asset` agent paths 明确包含 `behavior.asset.data_asset_class for asset_type=data_asset`。
- legacy MCP route 若继续保留，也要接受 `data_asset_class`。
- 增加 automation：`AssetFactoryCreatesPrimaryDataAsset`、`AssetFactoryReuseAcceptsDataAssetSubclass`。

## SMOKE-AF-20260510-03: create_asset 能力文档和入口 schema 漂移

**优先级**：P2

**现象**

- Smoke 已覆盖 `data_table` 和 `widget_blueprint`，但部分 legacy schema / 注释仍只列出旧资产类型。
- 这会误导普通 Agent 或回归测试，以为某些资产只能走 legacy 专家工具。

**实现证据**

- `BlueprintHelperAssetFactoryService.h` 注释仍描述旧版支持面。
- `tools.ts` 的 legacy `blueprint_create_asset` enum 没有 `data_table` 和 `widget_blueprint`，也没有 `row_struct` / `data_asset_class`。
- TaskSpec P1 compiler 与 C++ service 的真实能力比 legacy route 更宽。

**初步根因**

TaskSpec-first 能力扩展后，legacy MCP route 和说明没有同步退役或更新。

**建议修复**

- legacy route 标记为更明确的 frozen/internal，避免普通路径使用。
- 若保留 legacy route，则同步支持 `data_table`、`widget_blueprint`、`row_struct`、`data_asset_class`。
- 更新 service header 注释，避免和当前能力不一致。
