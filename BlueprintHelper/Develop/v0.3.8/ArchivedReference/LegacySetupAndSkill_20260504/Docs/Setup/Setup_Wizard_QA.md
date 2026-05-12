# BlueprintHelper Setup Wizard QA

必须采集：

```text
Project agent profile must include `environment.ue_engine_dir` absolute path and optional `environment.ue_version`; project `.uproject` is discovered by the Agent from the current workspace and passed as explicit `project_file` when calling tools
Safety Profile：ReadOnly / Conservative / Standard / AutoRepair / Expert
Agent entry mode：task_spec_first
Fallback policy：stop_and_report / capability_debug_allowed / legacy_direct_allowed
Blueprint / C++ 边界
Graph Write 是否允许修改用户节点 / 现有执行流
命名规则：EG_{FeatureName}、DescriptivePascalCase
Review / Journal / rollback_data retention
CLAUDE.md / AGENTS.md Project Marker 写入确认
```

默认：Conservative、task_spec_first、fallback stop_and_report、不自动 save、不自动编辑 IA/IMC、不支持第一版 Parent Class 修改。
