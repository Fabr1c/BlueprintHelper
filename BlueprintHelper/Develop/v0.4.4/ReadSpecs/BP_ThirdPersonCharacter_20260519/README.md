# BP_ThirdPersonCharacter ReadContext Inputs

Target file:

`D:\UEProjects\Template\Content\ThirdPerson\Blueprints\BP_ThirdPersonCharacter.uasset`

UE asset path:

`/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`

Run examples from `D:\UEProjects\Template\Plugins\BlueprintHelper`:

```powershell
bh blueprinthelper_read_context_capabilities --json "{}" --fields status,artifacts.full_result
bh blueprinthelper_read_context --file D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519\11_blueprint_logic_flow.json --fields status,summary,artifacts.full_result
bh blueprinthelper_read_context --file D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519\01_asset_context.json --fields status,summary,artifacts.full_result
bh blueprinthelper_read_context --file D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519\02_blueprint_logic_json.json --fields status,summary,artifacts.full_result
bh blueprinthelper_read_context --file D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519\03_blueprint_logic_md.json --fields status,summary,artifacts.full_result
```

Every JSON file in this folder is a `BlueprintHelper.ReadSpec.v1` input for `blueprinthelper_read_context`.

`blueprinthelper_read_context_capabilities` is a separate local discovery command and is not a ReadSpec. Use `--json "{}"` for it instead of passing one of these ReadSpec files.

ReadSpec no longer supports `view.format=summary` or `view.format=schema`. Non-logic ReadSpecs omit `view.format`; logic reads use `logic_flow`, `logic_md`, or `logic_json`. Use `logic_flow` for simple function/event/custom event reads, `logic_md` for larger entry reads, and `logic_json` for whole graph reads or anchor/debug work.
