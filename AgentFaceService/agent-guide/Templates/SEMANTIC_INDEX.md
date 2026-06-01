# Root Template Semantic Index

Use root-level templates for CLI support commands around a BlueprintHelper workflow.
They do not read asset graph logic and they do not author TaskSpecs.

## Environment And Guide Discovery

| Intent | Template | Command |
|---|---|---|
| Check installed runtime profile and safety state | `blueprint_get_runtime_profile_template.json` | `blueprint_get_runtime_profile` |
| Run static installation/configuration diagnostics | `blueprinthelper_diagnostics_template.json` | `blueprinthelper_diagnostics` |
| Run Editor/Bridge runtime diagnostics | `blueprinthelper_diagnostics_runtime_template.json` | `blueprinthelper_diagnostics_runtime` |
| Read the AgentGuide onboarding entry | `blueprinthelper_read_agent_guide_template.json` | `blueprinthelper_read_agent_guide` |
| Resolve unknown Unreal asset paths before reads or writes | `blueprinthelper_find_assets_template.json` | `blueprinthelper_find_assets` |
| Capture real editor screenshot evidence after opening an asset | `blueprinthelper_capture_screenshot_template.json` | `blueprinthelper_capture_screenshot` |

## Authorization And Task Lifecycle

| Intent | Template | Command |
|---|---|---|
| Request project-scoped write authorization after preview | `blueprinthelper_request_write_session_project_template.json` | `blueprinthelper_request_write_session` |
| Request asset-list-scoped write authorization after preview | `blueprinthelper_request_write_session_assets_template.json` | `blueprinthelper_request_write_session` |
| Read a completed task result or journal by id | `blueprinthelper_get_task_result_template.json` | `blueprinthelper_get_task_result` |

## Debug And Review Summaries

| Intent | Template | Command |
|---|---|---|
| List available debug cases | `blueprinthelper_list_debug_cases_template.json` | `blueprinthelper_list_debug_cases` |
| Read one summary-only debug case | `blueprinthelper_get_debug_case_template.json` | `blueprinthelper_get_debug_case` |
| Export a debug bundle manifest | `blueprinthelper_export_debug_bundle_template.json` | `blueprinthelper_export_debug_bundle` |
| Query Review record summaries | `blueprinthelper_query_review_records_template.json` | `blueprinthelper_query_review_records` |
