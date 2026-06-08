import type { ToolInputShapeId } from './tool-command-manifest.js';

export const EMPTY_OBJECT_INPUT_NOTE = 'No parameters. Use the empty-object template as-is. Template content: {}.';
export const MCP_ONLY_INPUT_NOTE = 'Editor lifecycle is global MCP-only. Use mcp__blueprint_helper__blueprint_open_editor or mcp__blueprint_helper__blueprint_close_editor; do not invoke CLI lifecycle aliases.';

const EMPTY_OBJECT_TEMPLATE_IDS = new Set([
  'blueprint_get_runtime_profile',
  'blueprinthelper_diagnostics',
  'blueprinthelper_diagnostics_runtime',
  'blueprinthelper_read_agent_guide',
  'blueprinthelper_read_context_capabilities',
  'blueprinthelper_list_debug_cases',
]);

export type ToolInputShapeSummaryId = ToolInputShapeId | 'multiple' | 'mcp_only';

export interface ToolInputShapeSummary {
	input_shape: ToolInputShapeSummaryId;
	input_shapes: Array<ToolInputShapeId | 'mcp_only'>;
	no_input: boolean;
	input_note?: string;
}

export function inferInputShapesFromTemplateIds(input: {
	templateIds: readonly string[];
	requiresBridge: boolean;
}): ToolInputShapeId[] {
	const shapes = new Set<ToolInputShapeId>();

	for (const templateId of input.templateIds) {
		if (templateId === 'blueprinthelper_preview_task_wrapper') {
			shapes.add('wrapped_taskspec_preview');
			continue;
		}
		if (templateId === 'blueprinthelper_execute_task_wrapper') {
			shapes.add('wrapped_taskspec_execute');
			continue;
		}
		if (
			templateId === 'task_preview_bare_taskspec'
			|| templateId === 'task_execute_bare_taskspec'
		) {
			shapes.add('bare_taskspec');
			continue;
		}
		if (EMPTY_OBJECT_TEMPLATE_IDS.has(templateId)) {
			shapes.add('empty_object');
			continue;
		}
		if (templateId.startsWith('read_context_')) {
			shapes.add('readspec');
			continue;
		}
		if (templateId === 'blueprinthelper_read_reference_context_dependencies') {
			shapes.add('read_reference_context');
			continue;
		}
		shapes.add(input.requiresBridge ? 'bridge_payload' : 'tool_payload');
	}

	if (shapes.size === 0) {
		shapes.add(input.requiresBridge ? 'bridge_payload' : 'tool_payload');
	}
	return [...shapes];
}

export function summarizeToolInputShape(input: {
	templateIds: readonly string[];
	requiresBridge: boolean;
	lifecycleMcpOnly?: boolean;
}): ToolInputShapeSummary {
	if (input.lifecycleMcpOnly === true) {
		return {
			input_shape: 'mcp_only',
			input_shapes: ['mcp_only'],
			no_input: true,
			input_note: MCP_ONLY_INPUT_NOTE,
		};
	}
	const inputShapes = inferInputShapesFromTemplateIds(input);
	const noInput = inputShapes.length === 1 && inputShapes[0] === 'empty_object';
	return {
		input_shape: inputShapes.length === 1 ? inputShapes[0] as ToolInputShapeSummaryId : 'multiple',
		input_shapes: inputShapes,
		no_input: noInput,
		...(noInput ? { input_note: EMPTY_OBJECT_INPUT_NOTE } : {}),
	};
}
