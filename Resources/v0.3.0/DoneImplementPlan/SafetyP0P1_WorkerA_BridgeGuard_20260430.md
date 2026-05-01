# Worker A Bridge Guard

## Goal

Close Bridge request guard gaps so invalid payloads, missing tokens, and disabled high-risk commands fail before business logic.

## Files

Modify:

```text
Source/BlueprintHelper/Public/Bridge/BlueprintHelperRequestValidator.h
Source/BlueprintHelper/Private/Bridge/BlueprintHelperRequestValidator.cpp
Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeTypes.h
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeProtocol.cpp
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
MCPServer/src/bridge-client.ts
MCPServer/src/tools.ts
Source/BlueprintHelper/Private/Tests/BlueprintHelperSafetyTests.cpp
```

Do not modify:

```text
Source/BlueprintHelper/Private/Services/BlueprintHelperImportService.cpp
Source/BlueprintHelper/Private/TextToBlueprintGenerator.cpp
Source/BlueprintHelper/Private/Services/BlueprintHelperWidgetService.cpp
```

## Steps

- [ ] Run the strong-read scan:

```powershell
rg "Get(String|Object|Array|Bool|Integer|Number)Field" Source/BlueprintHelper/Private/Bridge Source/BlueprintHelper/Public/Bridge
```

- [ ] Classify every remaining `Get*Field` in Bridge code as protected or unsafe. A read is protected only if `FBlueprintHelperRequestValidator::ValidatePayloadForCommand` proves the field exists and has the expected type for that command.

- [ ] Add missing command payload rules for read commands that still use strong reads, including `list_assets`, `search_assets`, `get_asset_info`, `get_widget_tree`, `get_widget_properties`, `get_datatable_rows`, and editor commands that accept payload fields.

- [ ] Replace unsafe strong reads in `BlueprintHelperBridgeRouter.cpp` with `TryGet*Field` helpers and return `invalid_request` with `field`, `expected_type`, and `actual_type`.

- [ ] Keep `validate_json` readable without token. Keep all write commands token-gated.

- [ ] Keep `exec_console_command` and `close_editor` disabled unless `BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS=1`, `true`, or `yes`.

- [ ] Add automation assertions:

```text
BlueprintHelper.Safety.RequestValidator.RejectsNullTargetGraph
BlueprintHelper.Safety.RequestValidator.RequiresTokenForWrite
BlueprintHelper.Safety.RequestValidator.DisablesHighRiskByDefault
```

- [ ] Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
npm.cmd run build
```

- [ ] Run:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

## Exit Criteria

- Invalid type payloads return `invalid_request` before any service handler runs.
- Untokened write commands return `unauthorized`.
- High-risk commands return `command_disabled` by default.
- MCP sends `auth_token` only from `BLUEPRINTHELPER_BRIDGE_TOKEN`.

