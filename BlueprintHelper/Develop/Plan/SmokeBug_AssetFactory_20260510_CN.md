# Smoke Bug - AssetFactory 2026-05-10

来源：`BlueprintHelper_NewProject_Full_SmokeRun_20260510.md`

本文只记录 AssetFactory / 资产创建链路问题，不记录 ReviewPanel 显示问题。

## SMOKE-AF-20260510-01: Blueprint Interface 被创建成 Actor parent

**优先级**：P0

**状态**：已修复；目标回归和 AssetFactory 组 Automation 已通过。

**现象**

- Smoke 中 `BPI_BHSmokeInteract` 创建成功，但 read-back 显示 parent class 是 `Actor`，不是 Interface。
- 后续 `edit_blueprint_class_settings` 因该资产不是有效接口而 blocked，导致 class settings 环节不能继续验证。

**修复前实现证据**

- `BlueprintHelperAssetFactoryService.cpp` 中 `CreateAsset()` 对 `BlueprintInterface` 分支直接调用 `CreateBlueprintInterface(AssetPath)`。
- `CreateBlueprintInterface()` 只设置 `Factory->BlueprintType = BPTYPE_Interface`，没有显式设置接口 Blueprint 所需的 parent / supported class。
- `TryNormalizeAssetTypeAndParent()` 对 `blueprint_interface` 只映射类型，没有补接口默认 parent。

**初步根因**

Blueprint Interface 的 UE factory 初始化不完整。当前路径依赖 `UBlueprintFactory` 默认值，导致生成出的 Blueprint 资产 parent 落到 Actor，后续所有要求接口资产的能力都会被阻断。

**修复结果**

- `CreateBlueprintInterface()` 显式设置 `Factory->BlueprintType = BPTYPE_Interface` 和 `Factory->ParentClass = UInterface::StaticClass()`。
- `TryNormalizeAssetTypeAndParent()` 对 `blueprint_interface` 清空输入 parent，避免 TaskSpec / smoke 误带 `Actor` 污染执行结果。
- 增加 automation：`BlueprintHelper.AssetFactory.CreatesBlueprintInterfaceAsset`。
- 验证报告：`Saved/Automation/AssetFactoryBPI_Green`，`Saved/Automation/AssetFactoryBPI_Group`。

## SMOKE-AF-20260510-02: PrimaryDataAsset 创建被 asset_type_mismatch 阻断

**优先级**：P1

**状态**：已修复；目标回归和 AssetFactory 组 Automation 已通过。2026-05-11 修正根因口径：Agent 不能用抽象 `DataAsset` / `PrimaryDataAsset` 直接创建 DA 实例，`data_asset_class` 必须是具体 `UDataAsset` 子类；新项目 smoke 先创建 `parent_class=PrimaryDataAsset` 的蓝图类，再用该蓝图资产路径创建具体 DA。

**现象**

- Smoke 中 `DA_BHSmokeData` 创建 blocked，错误为 `asset_type_mismatch for PrimaryDataAsset`。
- 目标 TaskSpec 使用 `asset_type=data_asset` 和 `data_asset_class=/Script/Engine.PrimaryDataAsset`。

**修复前实现证据**

- Python P1 compiler 已复制 `behavior.asset.data_asset_class` 到 `create_asset` op。
- C++ `CreateDataAsset()` 能接收 `DataAssetClass`，并通过 `UDataAssetFactory::DataAssetClass` 创建指定类。
- 但 AssetFactory 的 asset type/collision 判断仍按粗粒度 `DataAsset` 做匹配，不能把 `PrimaryDataAsset` 识别为 DataAsset 子类兼容。
- TypeScript legacy route `blueprint_create_asset` 的输入 schema 也没有暴露 `data_asset_class`，合同面仍存在入口不一致。

**初步根因**

DataAsset 创建能力本身已存在，但 Agent-facing 合同没有强制 `data_asset_class` 指向具体 `UDataAsset` 子类，导致 Agent 传入抽象基类；复用/冲突判断也仍是“精确资产类名”思路，没有 class-aware 地处理 DataAsset 子类。

**修复结果**

- DataAsset `reuse_if_exists` 改为 class-aware 判断：现有类必须是请求 `data_asset_class` 的子类；普通资产类型仍使用资产类名精确匹配。
- `data_asset_class=/Script/Engine.PrimaryAssetLabel` 这类可实例化 `UPrimaryDataAsset` 子类可以创建并复用。
- `data_asset_class=/Script/Engine.PrimaryDataAsset` 是抽象基类，当前会被干净拒绝，不再触发 UE factory ensure。
- `data_asset_class=/Game/.../BP_DataAssetClass` 现在会解析到该 Blueprint 资产的 generated class，支持先建 PrimaryDataAsset 蓝图类、再建 DA 实例的真实 smoke 链路。
- 增加 automation：`BlueprintHelper.AssetFactory.CreatesPrimaryDataAssetSubclass`、`ReuseAcceptsPrimaryDataAssetSubclass`、`RejectsAbstractDataAssetClass`。
- 增加 automation：`BlueprintHelper.AssetFactory.CreatesDataAssetFromBlueprintDataAssetClass`。
- 验证报告：`Saved/Automation/AssetFactoryPrimaryDAReuse_Green`，`Saved/Automation/AssetFactoryPrimaryDA_Group`。
- 验证报告：`Saved/Automation/AssetFactoryBlueprintDA_Green`。

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
