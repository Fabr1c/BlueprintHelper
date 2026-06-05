import { z } from 'zod';
import { FindAssetsInputSchema } from './asset-discovery-schema.js';
import { ReadFunctionChainContextInputSchema } from './function-chain-context-schema.js';
import { ReadContextCapabilitiesInputSchema } from './read-context/read-context-capabilities.js';
import { ReadContextInputSchema } from './read-context/read-context-schemas.js';
import { CaptureScreenshotInputSchema } from './screenshot/capture-screenshot-schema.js';

const DebugCaseInputSchema = z.object({
  debug_case_id: z.string().min(1),
});

const DebugCaseListInputSchema = z.object({
  limit: z.number().int().positive().max(200).optional(),
});

const ReviewRecordQueryInputSchema = z.object({
  archive_session_id: z.string().min(1).optional(),
  asset_path: z.string().min(1).optional(),
  task_run_id: z.string().min(1).optional(),
  pending_only: z.boolean().optional(),
});

const ReviewActionInputSchema = z.object({
  review_record_id: z.string().min(1),
  action: z.enum(['accept', 'reject']),
  target_keys: z.array(z.string().min(1)).optional(),
});

const SourceControlInputSchema = z.object({
  asset_paths: z.array(z.string().min(1)).optional(),
  package_names: z.array(z.string().min(1)).optional(),
  file_paths: z.array(z.string().min(1)).optional(),
  update_status: z.boolean().optional(),
}).refine(
  (value) =>
    (value.asset_paths?.length ?? 0) > 0 ||
    (value.package_names?.length ?? 0) > 0 ||
    (value.file_paths?.length ?? 0) > 0,
  {
    message: 'At least one of asset_paths, package_names, or file_paths is required.',
  },
);

const CompileBlueprintInputSchema = z.object({
  target_blueprint: z.string().min(1).optional(),
});

const SaveAssetInputSchema = z.object({
  asset_path: z.string().min(1),
});

export const bridgeToolSchemas: Record<string, z.ZodTypeAny> = {
  blueprinthelper_read_context: ReadContextInputSchema,
  blueprinthelper_read_context_capabilities: ReadContextCapabilitiesInputSchema,
  blueprinthelper_get_debug_case: DebugCaseInputSchema,
  blueprinthelper_list_debug_cases: DebugCaseListInputSchema,
  blueprinthelper_export_debug_bundle: DebugCaseInputSchema,
  blueprinthelper_query_review_records: ReviewRecordQueryInputSchema,
  blueprinthelper_apply_review_action: ReviewActionInputSchema,
  blueprinthelper_read_function_chain_context: ReadFunctionChainContextInputSchema,
  blueprinthelper_find_assets: FindAssetsInputSchema,
  blueprinthelper_capture_screenshot: CaptureScreenshotInputSchema,
  blueprint_compile_blueprint: CompileBlueprintInputSchema,
  blueprint_save_asset: SaveAssetInputSchema,
  blueprinthelper_source_control_status: SourceControlInputSchema,
  blueprinthelper_source_control_checkout: SourceControlInputSchema,
};
