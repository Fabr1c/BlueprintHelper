# BlueprintHelper Settings DryRun 与 Developer 文案清理计划

## 背景

Setting 面板当前有三类问题：

1. `tool_clusters.graph_write.layout` 仍显示为 GraphWrite layout 设置，但当前代码只把它读入 `FBlueprintHelperGraphWriteToolClusterPolicy::Layout`，再作为 `options.layout` 默认值注入 GraphWrite payload。GraphWrite 执行侧没有读取 `options.layout`；现行架构由 `GraphLayout` 系统和 `graph_layout.rules_source` 负责节点摆放规则。
2. 各 cluster 的 `dry_run` 开关分散在 Developer ToolCluster 分类里，和其他默认策略混在一起，阅读成本高。
3. Developer 分类的 label、hint、状态文案仍有大量英文，不符合 Setting 面板中文说明预期。

## 结论

`tool_clusters.graph_write.layout` 对当前 GraphWrite 行为已经没有有效意义。它只会制造“GraphWrite payload 仍能控制 layout”的错误暗示，应从默认配置、Presenter 行、runtime consumed 列表、policy 字段和 Bridge 默认注入里移除。

保留 `graph_layout.rules_source`。该设置仍由 `FBlueprintHelperGraphLayoutRuleSourceResolver` 消费，是当前 GraphLayout 规则文件入口。

## 实施步骤

1. 配置清理
   - 从 `DefaultSetting.json` 的 `tool_clusters.graph_write` 删除 `layout`。
   - 给 `tool_clusters.graph_write` 增加缺失的 `dry_run` 默认值，和其他写入 cluster 保持一致。

2. Presenter 分组与中文化
   - 新增 `DryRun` 分类。
   - 将以下 dry run 行按 cluster 顺序集中排列：
     `asset_factory`、`component`、`class_settings`、`blueprint_variables`、`object_property`、`data_table`、`umg_widget`、`graph_write`。
   - 从 Developer ToolCluster 分类移除分散的 dry run 行。
   - 删除 `GraphWrite layout` 行。
   - 将 Developer ToolCluster、Developer UI、Developer GraphLayout 相关 label/hint 改为中文。

3. Runtime policy 清理
   - 删除 `FBlueprintHelperGraphWriteToolClusterPolicy::Layout`。
   - 删除 `LoadGraphWritePolicy()` 对 `tool_clusters.graph_write.layout` 的读取。
   - 删除 Bridge GraphWrite 默认注入 `options.layout` 的逻辑。
   - 从 `IsRuntimeConsumedSetting()` 删除 `tool_clusters.graph_write.layout`，保留 `tool_clusters.graph_write.dry_run`。

4. 验证
   - 增加 Settings Presenter 自动化测试，覆盖：
     - `DryRun` 分类存在且 8 个 dry_run 行按顺序排列。
     - `tool_clusters.graph_write.layout` 不再出现。
     - Developer 行不再携带旧英文说明片段。
   - 运行针对性自动化测试。
   - 运行 UE 5.6 编译。
   - 对本次改动文件运行 `git diff --check`。

## 不做

1. 不改 `graph_layout.rules_source`，它仍是当前有效的 GraphLayout 规则入口。
2. 不改 GraphLayout solver/coordinator 的运行逻辑。
3. 不迁移旧文档中的历史 `layout:auto` 记录；本任务只处理当前 Setting 面板与 runtime 默认注入。
