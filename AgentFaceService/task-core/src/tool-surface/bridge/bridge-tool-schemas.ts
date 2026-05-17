import { z } from 'zod';
import { ReadFunctionChainContextInputSchema } from './function-chain-context-schema.js';
import { ReadContextInputSchema } from './read-context/read-context-schemas.js';

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

const CloseEditorInputSchema = z.object({
  save_all: z.boolean().optional(),
  wait_timeout_ms: z.number().optional(),
  project_file: z.string().optional(),
  force: z.boolean().optional(),
});

export const bridgeToolSchemas: Record<string, z.ZodTypeAny> = {
  blueprinthelper_read_context: ReadContextInputSchema,
  blueprinthelper_get_debug_case: DebugCaseInputSchema,
  blueprinthelper_list_debug_cases: DebugCaseListInputSchema,
  blueprinthelper_export_debug_bundle: DebugCaseInputSchema,
  blueprinthelper_query_review_records: ReviewRecordQueryInputSchema,
  blueprinthelper_apply_review_action: ReviewActionInputSchema,
  blueprinthelper_read_function_chain_context: ReadFunctionChainContextInputSchema,
  blueprint_close_editor: CloseEditorInputSchema,
};
