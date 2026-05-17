---
description: Deprecated compatibility entry; use the repository-root install script for BlueprintHelper setup
allowed-tools: Read
---

# BlueprintHelper Setup

`/blueprint-helper:setup` is retired. The first-run setup path is now the repository-root installer:

```powershell
.\install.ps1 -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

Run that script from the BlueprintHelper repository root. It builds the CLI/runtime packages, links `bh`, registers the Codex marketplace entry, installs Codex subagents and the MCP allowlist entry, writes the project `.blueprinthelper/agent-profile.json` when project and UE root are known, and creates default user preferences only when they are missing.

Use `/blueprint-helper:configure` only after installation when the user wants to change safety profile, save policy, missing-capability policy, or workflow preferences.

Do not recreate the old setup flow in this command, do not request MCP tool permissions here, and do not write project profile or preference files from this compatibility entry.
