# Worker B Strict Import Diagnostics

## Goal

Make strict import semantics complete: link, default value, pin alias, and pin type failures must surface as structured diagnostics and roll back by default.

## Files

Modify:

```text
Source/BlueprintHelper/Public/TextToBlueprintGenerator.h
Source/BlueprintHelper/Private/TextToBlueprintGenerator.cpp
Source/BlueprintHelper/Private/NodeHandlers/*.cpp
Source/BlueprintHelper/Public/Services/BlueprintHelperServiceTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperValidationService.cpp
Source/BlueprintHelper/Private/Services/BlueprintHelperImportService.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperSafetyTests.cpp
```

Avoid modifying Bridge files except for result serialization fields already owned by Worker A.

## Steps

- [ ] Add a generator diagnostic structure to `TextToBlueprintGenerator.h` with fields:

```text
severity
code
node_id
pin_name
message
```

- [ ] Extend `FBlueprintGenerateResult` with:

```text
RequestedDefaultValueCount
AppliedDefaultValueCount
DefaultValueDiagnostics
RequestedPinTypeCount
ResolvedPinTypeCount
PinTypeDiagnostics
RequestedConnectionCount
CreatedConnectionCount
ConnectionDiagnostics
```

- [ ] Change `ApplyDefaultValues` from `void` to returning diagnostics. Missing pin must produce `default_pin_not_found`. Failed object/class resolution must produce `default_value_object_not_found`. A value rejected by schema must produce `default_value_rejected`.

- [ ] Move default value application after `ReconstructNode` for nodes that currently call `ApplyDefaultValues` before reconstructing.

- [ ] In link creation, emit:

```text
link_node_not_found
link_pin_not_found
link_connection_rejected
```

- [ ] In pin type conversion paths, emit `invalid_pin_type` and stop the node operation instead of silently falling back.

- [ ] In `BlueprintHelperImportService`, treat any generator diagnostic with severity error as strict failure.

- [ ] In strict mode, cancel the transaction and return:

```json
{
  "status": "failed",
  "rolled_back": true,
  "errors": [
    { "code": "strict_import_rolled_back" }
  ]
}
```

- [ ] Add automation cases for:

```text
BlueprintHelper.Safety.ImportStrict.RollsBackOnMissingLinkPin
BlueprintHelper.Safety.ImportStrict.RollsBackOnMissingDefaultPin
BlueprintHelper.Safety.ImportStrict.RollsBackOnInvalidPinType
```

- [ ] Run UE build and safety automation.

## Exit Criteria

- A failed link does not leave newly-created nodes in strict mode.
- A failed default value does not leave newly-created nodes in strict mode.
- An invalid pin type does not create a downgraded or fallback pin.
- Import result includes structured errors suitable for MCP clients.

