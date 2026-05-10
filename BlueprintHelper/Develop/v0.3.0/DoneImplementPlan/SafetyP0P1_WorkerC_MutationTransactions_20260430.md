# Worker C Mutation Transactions

## Goal

Finish P1 write reliability for non-import write commands using transaction, validation-before-mutation, rollback, diagnostics, and editor-editable property checks.

## Files

Modify:

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperScopedAssetMutation.h
Source/BlueprintHelper/Private/Services/BlueprintHelperBlueprintStructureService.cpp
Source/BlueprintHelper/Private/Services/BlueprintHelperDataTableService.cpp
Source/BlueprintHelper/Private/Services/BlueprintHelperPropertyReflectionService.cpp
Source/BlueprintHelper/Private/Services/BlueprintHelperWidgetService.cpp
Source/BlueprintHelper/Public/Services/BlueprintHelperServiceTypes.h
Source/BlueprintHelper/Private/Tests/BlueprintHelperSafetyTests.cpp
```

Do not modify:

```text
Source/BlueprintHelper/Private/Services/BlueprintHelperImportService.cpp
Source/BlueprintHelper/Private/TextToBlueprintGenerator.cpp
MCPServer/src/tools.ts
```

## Steps

- [ ] Make `FBlueprintHelperScopedAssetMutation` the common helper used by DataTable, UObject property, Widget, and delete-node writes.

- [ ] For `DeleteNodes`, reject `Node_i` as final write targets. Accept stable `node_guid` formats only. Validate all ids before transaction starts.

- [ ] For DataTable update, copy the row buffer, apply every field to the copy, and replace the original row only after all fields succeed.

- [ ] For UObject property set, allow only properties with `CPF_Edit` and without `CPF_BlueprintReadOnly`, `CPF_EditConst`, or `CPF_Transient`.

- [ ] For Widget property set, use the same property flag policy as UObject property set.

- [ ] For UMG Move, snapshot:

```text
old parent
old child index
old slot class
old slot property values
```

- [ ] On UMG Move failure, restore the old parent, old child index, and old slot property values before canceling the transaction.

- [ ] Add automation cases:

```text
BlueprintHelper.Safety.DataTableUpdate.NoHalfWrite
BlueprintHelper.Safety.WidgetMove.RestoresOldSlot
BlueprintHelper.Safety.ObjectProperty.RejectsUnsafeFlags
BlueprintHelper.Safety.DeleteNodes.RejectsNodeIndex
```

- [ ] Run UE build and safety automation.

## Exit Criteria

- DataTable multi-field failure leaves the row unchanged.
- UMG Move failure restores parent, index, and layout slot values.
- Unsafe UObject and Widget properties are rejected and not dirtied.
- DeleteNodes cannot delete through unstable `Node_i` ids.

