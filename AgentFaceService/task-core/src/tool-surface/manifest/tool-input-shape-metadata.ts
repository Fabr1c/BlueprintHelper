import type { ToolInputShapeId } from './tool-command-manifest.js';
import { getReadContextRouteDescriptor } from '../templates/read-context-template-registry.js';

export const EMPTY_OBJECT_INPUT_NOTE = 'No parameters. Use the empty-object template as-is. Template content: {}.';

const EMPTY_OBJECT_TEMPLATE_IDS = new Set([
  'blueprint_get_runtime_profile',
  'blueprinthelper_diagnostics',
  'blueprinthelper_diagnostics_runtime',
  'blueprinthelper_read_agent_guide',
  'blueprinthelper_read_context_capabilities',
  'blueprinthelper_list_debug_cases',
]);

export type ToolInputShapeSummaryId = ToolInputShapeId | 'multiple';

export interface ToolInputShapeSummary {
	input_shape: ToolInputShapeSummaryId;
	input_shapes: ToolInputShapeId[];
	no_input: boolean;
	input_note?: string;
}

export function inferInputShapesFromTemplateIds(input: {
	templateIds: readonly string[];
	requiresBridge: boolean;
	emptyTemplateInputShape?: ToolInputShapeId;
}): ToolInputShapeId[] {
	const shapes = new Set<ToolInputShapeId>();

	for (const templateId of input.templateIds) {
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
		if (templateId.startsWith('read_context_') || getReadContextRouteDescriptor(templateId)?.status === 'active') {
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
		shapes.add(input.emptyTemplateInputShape ?? (input.requiresBridge ? 'bridge_payload' : 'tool_payload'));
	}
	return [...shapes];
}

export function summarizeToolInputShape(input: {
	templateIds: readonly string[];
	requiresBridge: boolean;
	emptyTemplateInputShape?: ToolInputShapeId;
}): ToolInputShapeSummary {
	const inputShapes = inferInputShapesFromTemplateIds(input);
	const noInput = inputShapes.length === 1 && inputShapes[0] === 'empty_object';
	return {
		input_shape: inputShapes.length === 1 ? inputShapes[0] as ToolInputShapeSummaryId : 'multiple',
		input_shapes: inputShapes,
		no_input: noInput,
		...(noInput ? { input_note: EMPTY_OBJECT_INPUT_NOTE } : {}),
	};
}
