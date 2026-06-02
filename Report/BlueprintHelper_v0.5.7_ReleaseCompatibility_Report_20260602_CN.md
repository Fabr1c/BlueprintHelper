# BlueprintHelper v0.5.7 Release Compatibility Report - 2026-06-02

## 改动原因

本次会话需要按既有方式完成 UE 5.3、5.4、5.5、5.7 版本适配，将当前版本同步到 `v0.5.7`，归档已经闭环的 plan/design 文档，并输出相较 `v0.5.4` 的 release log。

## 改动过程

1. 先运行 UE 5.3 RED BuildPlugin，确认低版本失败点集中在 `SGraphEditor` view location 类型、AssetRegistry overload 和 GraphWrite direct no-shrink API。
2. 新增 `FBlueprintHelperGraphEditorVersionCompat` 兼容 seam，并用独立 `.h/.cpp` 承载高低版本 `SGraphEditor` view location 差异。
3. 将 AssetRegistry 遍历改回低版本可用 overload，并保留 `bIncludeOnlyOnDiskAssets = true` 语义。
4. 将 GraphWrite 两处 direct `Pop(EAllowShrinking::No)` 改为复用 `FBlueprintHelperVersionCompat::PopNoShrink`。
5. 将 Unreal descriptor、默认设置、AgentFaceService package、MCP server metadata、Codex/Claude plugin manifest 和用户文档中的当前版本元数据同步到 `0.5.7`。
6. 将已闭环 plan/design 文档移动到 `BlueprintHelper/Develop/v0.5.7/ArchivedReference/CompletedDevelopDocs_20260602/`，并创建 v0.5.7 README、structured AGENT/rules/compat snapshot 与 release log。
7. 根据最终只读审计结果，将 CLI/MCP tracked package-lock 中 `../task-core` 的残留 `0.5.4` 元数据同步为 `0.5.7`，并重跑 CLI/MCP node tests。
8. 根据最终只读审计结果，修复 Claude README 和 active Debug 证据里指向已归档 plan 文档的旧 `Develop/Plan/...` 链接。

## 改动结果

1. 当前 release metadata 已同步为 `v0.5.7` / descriptor `Version: 507`。
2. UE 5.3、5.4、5.6、5.7 BuildPlugin 均通过。
3. UE 5.5 当前被本机 UE engine plugin intermediate 写入/rename 权限问题阻断，失败发生在 BlueprintHelper 编译前。
4. CLI/TaskSpec/MCP TypeScript build 和 node tests 均通过。
5. 真实 E2E smoke 通过：UE 5.6 editor 成功启动，Bridge 可用，CLI runtime profile 和 runtime diagnostics 均完成。

## 代码改动范围

1. C++ compatibility seam: `BlueprintHelperGraphEditorVersionCompat.h/.cpp`。
2. C++ call sites: screenshot capture, asset discovery, GraphWrite append service, GraphWrite connectivity validator。
3. Release metadata: `.uplugin`、DefaultSetting、AgentFaceService packages、MCP server metadata、Codex/Claude plugin manifests、README/API docs、C++ runtime/config fallback version strings。
4. Release documents: `Debug/`、`Report/`、`BlueprintHelper/Develop/v0.5.7/`、`BlueprintHelper/Develop/v0.5.7/AGENT/rules/compat/`、`BlueprintHelper/Develop/Plan/README.md`。
5. Lockfile metadata: `AgentFaceService/cli/package-lock.json`、`AgentFaceService/mcp/package-lock.json`。
6. Handoff links: `ClaudePlugin/README.md`、`Debug/*GraphWriteConnectivityValidation*`、`Debug/*GraphLayout*`、`Debug/BlueprintHelper_EditorScreenshotEvidence_Brainstorm_20260601.md`。

## 验证结果

| 验证项 | 结果 | 证据 |
| --- | --- | --- |
| task-core build | PASS | `@blueprinthelper/task-core@0.5.7 build` exit 0 |
| task-core test:node | PASS | 331 passed / 0 failed |
| cli build | PASS | `blueprint-helper-cli@0.5.7 build` exit 0 |
| cli test:node | PASS | 54 passed / 0 failed |
| mcp build | PASS | `blueprint-helper-mcp-server@0.5.7 build` exit 0 |
| mcp test:node | PASS | 12 passed / 0 failed |
| tracked package-lock scan | PASS | no `0.5.4` remains in tracked task-core/cli/mcp package-lock files |
| archived-doc link scan | PASS | no active references remain to the moved `Develop/Plan/<archived-doc>.md` paths |
| post-audit cli test:node | PASS | 54 passed / 0 failed |
| post-audit mcp test:node | PASS | 12 passed / 0 failed |
| UE 5.3 BuildPlugin | PASS | `Saved/B53_v057_final`, ExitCode 0 |
| UE 5.4 BuildPlugin | PASS | `Saved/B54_v057_final`, ExitCode 0 |
| UE 5.5 BuildPlugin | BLOCKED | UHT failed to save/rename `CaptureDataUtils` temp files under UE engine Intermediate before BlueprintHelper compile |
| UE 5.6 BuildPlugin | PASS | `Saved/B56_v057_final`, ExitCode 0 |
| UE 5.7 BuildPlugin | PASS | `Saved/B57_v057_final`, ExitCode 0 |
| Real E2E | PASS | lifecycle MCP launched UE 5.6 editor; CLI `bridge ping`, `blueprint_get_runtime_profile`, and `blueprinthelper_diagnostics_runtime` completed with zero warnings/errors |

## Git 状态

本次没有执行 `git add`、`git commit` 或 `git push`。
