# BlueprintHelper Settings 与 CLI Artifact 路径审计计划（2026-05-31）

## 结论

- CLI JSON artifact 默认根路径原先来自 `AgentFaceService/cli/src/cli/artifacts.ts` 的硬编码 fallback：`cwd/Saved/BlueprintHelper/Cli`。
- 本轮把默认路径接入 `cli.artifacts.default_output_dir`，优先级为 `--artifact-dir`、`BPH_CLI_ARTIFACT_DIR`、effective Setting、fallback。
- Setting 来源合并 `Config/DefaultSetting.json`、项目 `.blueprinthelper/setting.json`、项目 `Saved/BlueprintHelper/setting.user.json`；相对 setting 路径按项目根解析，找不到项目根时才按 CLI `cwd` 解析。
- `DefaultSetting.json` 补齐已被 runtime 消费但缺外部默认项的 `tool_clusters.graph_write.action_resolution.*`。
- Settings 面板补齐已消费 setting：CLI artifact 路径、runtime bridge、TaskRuntime cache / execution policy、Review artifact / DebugBundle、GraphWrite action resolution、UI layout / notification / workbench / review panel layout 等。
- `review.version`、DebugBundle schema/hash 等合同元数据继续保留在 setting 文件中，但不做普通可编辑 row。
- 当前确认应删除的废弃项仍是 `tool_clusters.graph_write.layout`；它已不在默认文件和面板中，本轮继续通过测试防回归。

## 实施范围

1. CLI artifact 路径
   - 新增 `cli.artifacts.default_output_dir`。
   - CLI 读取顺序：`--artifact-dir` > `BPH_CLI_ARTIFACT_DIR` > effective Setting > fallback。
   - Setting 相对路径按项目根解析，避免同一项目配置在不同启动目录下落到不同 artifact 根目录。

2. DefaultSetting
   - 增加 `cli.artifacts.default_output_dir`。
   - 增加 `tool_clusters.graph_write.action_resolution.max_candidates/default_search_mode/default_ambiguity_policy`。
   - 移除未被当前实现消费的 `settings_visibility` 目录元数据，避免可视化 row 与文件 catalog 再次漂移。

3. Settings 面板
   - 添加所有已确认 runtime/CLI 消费但当前没有 row 的 setting。
   - 不把 schema、version、Review v2 版本、DebugBundle schema/hash 这类合同元数据做成普通 row。
   - 保持 DryRun 独立分类和现有顺序。
   - Developer 文案保持中文，不恢复英文说明。

4. 验证
   - CLI Node 测试覆盖 artifact 路径优先级和 project/user setting override。
   - UE automation 覆盖 Settings Presenter 的关键 runtime-consumed row。
   - 构建与自动化测试通过后再结束任务。
