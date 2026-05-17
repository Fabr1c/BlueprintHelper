# BlueprintHelper UE 5.3 Compatibility and AgentFace Audit - 2026-05-18

## 目标

1. 在不破坏 UE 5.6 主路径的前提下，让插件通过 UE 5.3 BuildPlugin。
2. 非 UE 5.6 兼容问题统一收敛到版本能力宏和 fallback 工具类/兼容文件。
3. 派发只读 subagent 检查 Agent-facing 文档是否混入开发侧内容。

## 硬性规则

- UE 5.6 是主基线。
- 不为了旧版本编译直接替换 5.6 API 路径。
- 不改变现有类继承、绘制层、schema、架构语义来规避兼容错误。
- 旧版本差异必须通过 `#if` 能力宏、fallback helper/tool class 或独立 compat `.cpp` 隔离。
- UHT 不支持的条件 UCLASS 形态不能用弱化行为绕过。

## 本轮实现

### 版本兼容 helper

- 扩展 `FBlueprintHelperVersionCompat`：
  - `BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING`
  - `BLUEPRINTHELPER_UE_HAS_CONST_SCRIPTSTRUCT_IMPORTTEXT`
  - `BLUEPRINTHELPER_UE_HAS_AUTOMATION_EXPECTED_ERROR_PLAIN`
  - `LeftInlineNoShrink`
  - `RemoveAtSwapNoShrink`
  - `ImportScriptStructText`
  - `AddExpectedErrorPlainCompat`

### UMG 变量注册 fallback

- 新增 `FBlueprintHelperWidgetVersionCompat`：
  - UE 5.6：保持 `WidgetVariableNameToGuidMap` / `OnVariableAdded` / `OnVariableRemoved` 原路径。
  - UE 5.3：fallback 到 `UWidget::bIsVariable`。

### Comment node 5.3 fallback

- 新增独立 compat `.cpp`：
  - 仅 UE 5.3 编译 `UEdGraphNode_Comment` 缺失导出 fallback。
  - 保持 `UBlueprintHelperReviewDiffBlockNode : UEdGraphNode_Comment` 不变。
  - 不改变 Review diff block 的 comment 绘制层语义。

## 验证

- PASS：UE 5.3 BuildPlugin
  - `E:\UE_5.3\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin -Package=D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BuildPlugin_UE53 -TargetPlatforms=Win64`
- PASS：UE 5.6 BuildPlugin
  - `E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin -Package=D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BuildPlugin_UE56 -TargetPlatforms=Win64`

## AgentFace 检查与清理结果

Subagent 只读检查完成，未检查 C++ 实现。发现 Agent-facing 文档仍有开发侧内容混入：

1. 高优先级：普通 Agent 文档暴露 `developer_exec` / 本地测试编排入口。
2. 高优先级：废弃 MCP 普通工具的测试维护细节混入 Agent-facing 指南。
3. 中优先级：构建/包维护步骤出现在普通 Agent skill / README。
4. 中优先级：内部 TaskPlan contract 被 QuickStart 引给普通 Agent。
5. 低优先级：产品 README 有未完成兼容矩阵口径。

本轮已按用户要求清理普通 Agent-facing 面：

- `AgentFaceService/agent-guide`、Codex/Claude 插件镜像的 onboarding 与 tool selection 不再暴露 developer exec、本地测试编排或废弃 MCP 普通工具测试口径。
- `AgentFaceService/docs/CLI_Tools_API_Reference.md` 的 MCP surface 统一为 editor open / editor close lifecycle 入口，不再把 developer-only exec 写入普通 Agent API 参考。
- `AgentFaceService/README.md` 移除根目录中的 package build 命令块和 debug/recovery MCP 描述。
- `AgentFaceService/docs/Install_CLI_QuickStart.md` 不再复制逐包 Node 构建/测试命令块，安装、重建和诊断统一指向根 `install.ps1` / `install.cmd`。
- 根 `README.md` 同步为 lifecycle-only MCP 口径，不再暴露 developer exec 或废弃 MCP 测试维护描述。

## 遗留说明

- `git status` 可能继续显示若干无内容差异文件为 modified；当前环境创建 `.git/index.lock` 被拒，导致索引刷新/restore 不稳定。以 `git diff --name-only` 和具体 diff 内容为准。
