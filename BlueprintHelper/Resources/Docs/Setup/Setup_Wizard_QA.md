# BlueprintHelper Setup Wizard QA

必须采集：

```text
UE_ENGINE_DIR 绝对路径；项目 .uproject 由 Agent 在当前工作区发现，并在调用工具时作为 project_file 显式传入
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
