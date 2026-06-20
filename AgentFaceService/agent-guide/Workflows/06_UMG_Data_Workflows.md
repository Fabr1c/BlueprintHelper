# 06 - UMG And Data Workflows

UMG, DataAsset, DataTable, and UObject property writes all use the
TaskSpec-first flow. This document records workflow policy only; concrete input
fields belong in CLI-discovered template files.

If source control is enabled for target assets, run the current source-control
status or checkout flow after preview and before execute. Stop on
checked-out-by-other, conflict, unavailable provider, checkout failure, or
not-editable status.

If `write_permission` is disabled, request a write session after preview only.
The Editor prompt is accept/reject; rejection stops the write.

## UMG Capability Boundary Rule

`umg_widget` is an Agent-facing TaskSpec family for Widget Blueprint tree, widget property, named slot, and widget class-setting edits. A failed or skipped HUD implementation is not a capability boundary unless the agent records complete CLI discovery evidence showing the required UMG family / operation / quick-access is unavailable, or preview returns an explicit unsupported / missing capability diagnostic.

## WidgetBlueprint Creation Versus UMG Editing

Use `asset_factory/create_widget_blueprint` to create a WidgetBlueprint asset. Use `umg_widget` to edit an existing WidgetBlueprint tree, widget properties, named slots, or widget class settings.

A skipped HUD implementation is not a capability boundary when `create_widget_blueprint` or `umg_widget` discovery exposes the required operation and quick-access template.

## Classification

Use the shared Report/Debug buckets from the Blueprint edit workflow: `能力边界`, `执行未完成`, `路径误选`, and `preview blocked`. For UMG/HUD work, a skipped implementation is `执行未完成` when `umg_widget` discovery exposes the needed operation family; only complete missing discovery or explicit unsupported diagnostics can become `能力边界`.

## Validation Policy

- WidgetBlueprint and UMG writes may compile when the template and task require
  Blueprint validation.
- DataAsset instances, DataTables, UserDefinedStructs, InputActions,
  InputMappingContexts, and plain UObject property writes do not have a Blueprint
  compile step and should be verified by read-back.
- A Blueprint class used as a DataAsset type must be compiled before creating or
  validating instances from it.
- Do not treat idempotent `no_op` reuse as failure when read-back proves the
  existing asset matches the requested type and content.

## Workflow Notes

- For UMG edits, read the target Widget Blueprint context, use the CLI-discovered
  UMG template, preview, execute, then read back the widget tree or key
  properties.
- For DataTable edits, read table/reference context, use the CLI-discovered row
  edit template, preview, execute, then read back the target rows.
- For DataAsset work, use a concrete project-owned `UDataAsset` subclass. Do not
  instantiate abstract base classes.
- For object property writes, use the CLI-discovered object-property template and
  verify by property read-back.
