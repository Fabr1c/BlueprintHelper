# ReviewPanel UI Test TaskSpecs 2026-05-18

Root asset folder:

`/Game/BlueprintHelperCliSmoke/ReviewPanelManual_20260518_001`

Use these files to regenerate ReviewEvent data for manual ReviewPanel UI testing. Each JSON is a bare `BlueprintHelper.TaskSpec.v1` file and can be executed independently.

Preview one file:

```powershell
cd D:\UEProjects\Template
bh.cmd task preview --file "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Plan\ReviewPanel_UI_Test_TaskSpecs_20260518\01_create_blueprint_actor.json"
```

Execute one file:

```powershell
cd D:\UEProjects\Template
bh.cmd task execute --file "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Plan\ReviewPanel_UI_Test_TaskSpecs_20260518\01_create_blueprint_actor.json"
```

Execute all files in order:

```powershell
cd D:\UEProjects\Template
Get-ChildItem "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.3\ArchivedReference\RetiredReviewDebugDocs_20260518\PlanArtifacts\ReviewPanel_UI_Test_TaskSpecs_20260518" -Filter "*.json" |
  Sort-Object Name |
  ForEach-Object { bh.cmd task execute --file $_.FullName }
```

Suggested order:

1. `01_create_blueprint_actor.json`
2. `02_edit_blueprint_components.json`
3. `03_edit_blueprint_variables.json`
4. `04_edit_blueprint_signatures.json`
5. `04b_write_function_body.json`
6. `05_append_graph_review_body.json`
7. `06_create_structure_row.json`
8. `07_create_data_table.json`
9. `08_edit_data_table_rows.json`
10. `09_create_data_asset_class.json`
11. `10_edit_data_asset_class_variables.json`
12. `11_create_data_asset_instance.json`
13. `12_edit_data_asset_properties.json`
14. `13_create_widget_blueprint.json`
15. `14a_edit_widget_tree_root.json`
16. `14b_edit_widget_tree_child.json`
17. `14c_edit_widget_tree_property.json`

Manual testing focus:

1. Blueprint actor: Components, MyBlueprint variables, function/event/dispatcher signatures, function body diff, graph body diff.
2. DataTable: row add/update, row details, row Accept/Reject isolation.
3. Structure: field row diff and single-field Reject isolation.
4. DataAsset: string/float/bool object property diff, read-only display, Accept/Reject refresh.
5. WidgetBlueprint: root widget and child widget ReviewEvent coverage.

Repeat notes:

1. If assets already exist with the same final values, execution can become no-op and may not generate new ReviewEvents.
2. For a clean manual pass, delete `/Game/BlueprintHelperCliSmoke/ReviewPanelManual_20260518_001` and old Review records first, or copy this folder and replace `20260518_001` with a new suffix in every JSON file.
3. Keep `should_save=true` so later TaskSpecs in the sequence can consume assets from earlier TaskSpecs without dirty-asset blocking.
4. `04b_write_function_body.json` uses `review_baseline_dirty_asset_policy=save_before_archive` because it usually follows signature creation in the same manual pass; this avoids Review baseline blocking when the target Blueprint is still dirty.
