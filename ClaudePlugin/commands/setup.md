---
description: Run BlueprintHelper initial setup — configure UE paths, verify MCP Bridge connectivity, collect safety preferences, and generate SetupProfile
---

# BlueprintHelper Setup

你正在运行 BlueprintHelper 初次配置。按以下步骤逐一完成，不要跳过。

---

## 阶段 1：路径配置

### 1.1 获取 UE 引擎目录

询问用户并确认 `UE_ENGINE_DIR` 的绝对路径，例如 `F:\UE_5.6`。

验证规则：
- 必须是绝对路径
- 路径下必须存在 `Engine/Binaries/Win64/UnrealEditor.exe`（或对应的平台二进制文件）
- 如果不存在，提示用户重新输入

### 1.2 获取项目文件

询问用户并确认 `UE_PROJECT_FILE` 的绝对路径，例如 `G:\UnrealPractise\MrStone\MrStone.uproject`。

验证规则：
- 必须是绝对路径
- 必须以 `.uproject` 结尾
- 文件必须存在

### 1.3 确认 BlueprintHelper 插件已安装

检查以下条件：
- BlueprintHelper 插件目录存在于项目 `Plugins/` 或引擎 `Engine/Plugins/` 下
- `BlueprintHelper.uplugin` 文件存在且 `VersionName` 与 MCP Server `package.json` 版本兼容

---

## 阶段 2：MCP Server 构建

### 2.1 安装依赖并构建

在 `BlueprintHelper_MCP_Server/` 目录下执行：

```powershell
npm install
npm run build
```

### 2.2 验证 MCP Server 可启动

执行 `node build/index.js --help` 或仅检查 `build/index.js` 存在。

---

## 阶段 3：Bridge 连通性

### 3.1 确认 Unreal Editor 状态

检查以下之一：
1. Unreal Editor 正在运行且已加载 BlueprintHelper 插件
2. 如果 Editor 未运行，确认 `open_editor` 工具可用（依赖 UE_ENGINE_DIR 和 UE_PROJECT_FILE）

### 3.2 验证 Bridge 连接

Bridge 默认地址 `127.0.0.1:54321`。

如果可以调用 MCP 工具，使用 `blueprinthelper_diagnostics` 检查：
- `bridge_status` 应为 `connected`
- 如果不是，报告阻断原因并让用户排查

---

## 阶段 4：Setup Wizard 偏好采集

逐项询问用户以下配置，不要一次问所有问题。每项提供默认值。

### 4.1 安全档位 (safety_profile)

选项：
1. **ReadOnly** — 只读模式，拒绝所有写入
2. **Conservative**（推荐默认）— 允许写入，需 preview + 用户确认，不自动 save
3. **Standard** — 允许写入，需 preview，可自动 save
4. **AutoRepair** — 允许写入和自动修复
5. **Expert** — 允许所有操作，最小化确认流程

### 4.2 fallback 策略

当 TaskSpec-first 工具不可用时的行为：
1. **stop_and_report**（推荐默认）— 停止并报告
2. **capability_debug_allowed** — 允许使用调试/诊断工具
3. **legacy_direct_allowed** — 允许直接调用底层 MCP 工具

### 4.3 蓝图/C++ 边界

- Agent 是否可以修改 C++ 源文件？（默认：否）
- Agent 是否可以修改 `.uassets`？（默认：是）
- Parent Class 修改策略？（默认：不支持，停止并报告）

### 4.4 Graph Write 偏好

- 新 EventGraph 命名模式？（默认：`EG_{FeatureName}`）
- 是否允许修改用户已有的节点？（默认：否）
- 是否允许合并现有执行流？（默认：否）

### 4.5 Enhanced Input

- 是否允许自动创建 Input Action？（默认：否）
- 是否允许自动编辑 Input Mapping Context？（默认：否）

### 4.6 Review 与回滚

- 是否启用 Transaction Journal？（默认：是）
- 是否启用 Review Store？（默认：是）
- 回滚数据保留策略？（默认：KeepFullUntilAccepted）

---

## 阶段 5：生成 SetupProfile

基于用户答案构造 `BlueprintHelper.SetupProfile.v1` JSON，写入项目配置目录。

推荐的保存路径：`<ProjectDir>/.blueprinthelper/agent-profile.json`

示例结构参见 `Resources/Docs/Setup/SetupProfile_Example.json`。

---

## 阶段 6：验证

### 6.1 runtime_profile 可读

调用 `blueprinthelper_get_runtime_profile`，验证：
- `active_profile.safety_profile` 与用户选择一致
- `bridge_status` 为 `connected`
- `config_status` 为 `valid`

### 6.2 diagnostics 通过

调用 `blueprinthelper_diagnostics`，确认：
- 无 Blocking 项
- Warning 项已知且可接受
- Info 项显示链路完整

### 6.3 生成项目 Marker（可选）

如果项目根目录存在 `CLAUDE.md` 或 `AGENTS.md`，询问是否需要添加 BlueprintHelper 引用指针：

```markdown
## BlueprintHelper

本项目使用 BlueprintHelper 进行 UE 编辑器资产操作。Agent 请遵循 skill `blueprint-helper` 的 TaskSpec-first 流程。
SetupProfile: <ProjectDir>/.blueprinthelper/agent-profile.json
```

---

## 阶段 7：报告

Setup 完成后输出摘要：

```text
BlueprintHelper Setup 完成

UE Engine:  <UE_ENGINE_DIR>
UE Project: <UE_PROJECT_FILE>
Bridge:     <host:port> — <status>
Safety:     <safety_profile>
Entry Mode: task_spec_first
Fallback:   <fallback_policy>

SetupProfile 已保存至: <path>
```

如果任何阶段被阻断，停止并报告具体阻断原因，不要跳过。
