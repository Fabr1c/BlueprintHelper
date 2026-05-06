import {
  TASK_RUN_JOURNAL_SCHEMA,
  type TaskPlan,
} from './task-schemas.js';

let taskCounter = 0;
let previewCounter = 0;

const taskResults = new Map<string, Record<string, unknown>>();

export function nextPreviewId() {
  return `preview_${Date.now()}_${String(++previewCounter).padStart(4, '0')}`;
}

export function nextTaskRunId() {
  return `task_${Date.now()}_${String(++taskCounter).padStart(4, '0')}`;
}

export function storeTaskResult(input: {
  taskRunId: string;
  previewId: string;
  taskPlan: TaskPlan;
  status: 'completed' | 'failed';
  bridgeResult?: Record<string, unknown>;
}) {
  const firstStep = input.taskPlan.steps[0];
  const bridgeStep = extractBridgeStep(input.bridgeResult, firstStep?.step_id);
  const transactionId = extractTransactionId(input.bridgeResult);
  const stepCapability = stringField(bridgeStep, 'capability') ?? stringField(firstStep, 'capability');
  const stepOperation = stringField(bridgeStep, 'operation')
    ?? stringField(firstStep, 'operation')
    ?? stepCapability
    ?? 'unknown';
  const adapterOperation = stringField(bridgeStep, 'adapter_operation');
  const generatedIntent = input.status === 'completed'
    ? generateTaskIntent(input.taskPlan, firstStep, stepCapability)
    : undefined;
  const journal = {
    schema: TASK_RUN_JOURNAL_SCHEMA,
    task_run_id: input.taskRunId,
    preview_id: input.previewId,
    task_type: input.taskPlan.task_type,
    feature_name: input.taskPlan.task_name,
    ...(generatedIntent ? { generated_intent: generatedIntent } : {}),
    status: input.status,
    target_assets: input.taskPlan.target_assets,
    steps: firstStep
      ? [
          {
            step_id: firstStep.step_id,
            ...(stepCapability ? { capability: stepCapability } : {}),
            operation: stepOperation,
            ...(adapterOperation ? { adapter_operation: adapterOperation } : {}),
            status: input.status === 'completed' ? 'completed' : 'failed',
            ...(transactionId ? { transaction_id: transactionId } : {}),
          },
        ]
      : [],
    bridge_result: input.bridgeResult,
  };
  taskResults.set(input.taskRunId, journal);
  return journal;
}

export function storeTaskRunJournal(taskRunId: string, journal: Record<string, unknown>) {
  const normalizedJournal = normalizeTaskRunJournal(taskRunId, journal);
  taskResults.set(taskRunId, normalizedJournal);
  return normalizedJournal;
}

export function getTaskResult(taskRunId: string) {
  return taskResults.get(taskRunId);
}

function extractBridgeStep(result: Record<string, unknown> | undefined, stepId: string | undefined) {
  const data = getRecord(result, 'data');
  const steps = Array.isArray(data?.['steps']) ? data['steps'] : [];
  return steps.find((step): step is Record<string, unknown> => {
    if (step === null || typeof step !== 'object' || Array.isArray(step)) {
      return false;
    }
    if (!stepId) {
      return true;
    }
    return step['step_id'] === stepId;
  });
}

function extractTransactionId(result: Record<string, unknown> | undefined) {
  if (!result) {
    return undefined;
  }

  const data = getRecord(result, 'data');
  if (!data) {
    return undefined;
  }

  const writeRef = getRecord(data, 'write_ref');
  const transactionId = writeRef?.['transaction_id'];
  if (typeof transactionId === 'string') {
    return transactionId;
  }

  const rawSteps = data?.['steps'];
  const steps = Array.isArray(rawSteps) ? rawSteps : [];
  const firstStep = steps.find((step): step is Record<string, unknown> =>
    step !== null && typeof step === 'object' && !Array.isArray(step),
  );
  const stepResult = getRecord(firstStep, 'result');
  if (!stepResult) {
    return undefined;
  }

  return extractTransactionId(stepResult);
}

function stringField(record: unknown, field: string) {
  const value = record !== null && typeof record === 'object' && !Array.isArray(record)
    ? (record as Record<string, unknown>)[field]
    : undefined;
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

function getRecord(record: Record<string, unknown> | undefined, field: string) {
  const value = record?.[field];
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

const intentToolShortNames: Record<string, string> = {
  graph_write: 'GraphWrite',
  blueprint_variable: 'BlueprintVariable',
  asset_factory: 'AssetFactory',
  blueprint_component: 'BlueprintComponent',
  blueprint_class_settings: 'ClassSettings',
  blueprint_signature: 'BlueprintSignature',
  object_property: 'ObjectProperty',
  graph_cleanup_ownership: 'CleanupOwnership',
  umg_widget: 'UMGWidget',
  data_table: 'DataTable',
};

const intentActionByCapability: Record<string, string> = {
  graph_write: '写入蓝图逻辑',
  blueprint_variable: '编辑蓝图变量',
  asset_factory: '创建资产',
  blueprint_component: '编辑组件',
  blueprint_class_settings: '编辑类设置',
  blueprint_signature: '编辑蓝图签名',
  object_property: '设置对象属性',
  graph_cleanup_ownership: '管理 BlueprintHelper 所有权',
  umg_widget: '编辑 UMG',
  data_table: '编辑 DataTable',
};

const intentCapabilityByTaskType: Record<string, string> = {
  edit_blueprint_graph: 'graph_write',
  edit_blueprint_variables: 'blueprint_variable',
  create_asset: 'asset_factory',
  edit_blueprint_components: 'blueprint_component',
  edit_blueprint_class_settings: 'blueprint_class_settings',
  edit_umg_widget: 'umg_widget',
  edit_data_table: 'data_table',
  edit_object_properties: 'object_property',
  manage_blueprinthelper_ownership: 'graph_cleanup_ownership',
};

const intentCapabilityByOperation: Record<string, string> = {
  append_blueprint_graph: 'graph_write',
  replace_blueprint_graph: 'graph_write',
  patch_blueprint_graph: 'graph_write',
  merge_blueprint_graph: 'graph_write',
  graph_write: 'graph_write',
  blueprint_variable_batch: 'blueprint_variable',
  create_asset: 'asset_factory',
  blueprint_component_batch: 'blueprint_component',
  blueprint_class_settings_batch: 'blueprint_class_settings',
  blueprint_signature_batch: 'blueprint_signature',
  object_property_batch: 'object_property',
  graph_cleanup_ownership_batch: 'graph_cleanup_ownership',
  umg_widget_batch: 'umg_widget',
  data_table_batch: 'data_table',
};

function normalizeTaskRunJournal(taskRunId: string, journal: Record<string, unknown>) {
  const normalizedJournal = { ...journal };
  if (
    normalizedJournal['schema'] !== TASK_RUN_JOURNAL_SCHEMA ||
    normalizedJournal['task_run_id'] !== taskRunId ||
    normalizedJournal['status'] !== 'completed' ||
    stringField(normalizedJournal, 'generated_intent')
  ) {
    return normalizedJournal;
  }

  const generatedIntent = generateTaskIntentFromJournal(normalizedJournal);
  if (generatedIntent) {
    normalizedJournal['generated_intent'] = generatedIntent;
  }
  return normalizedJournal;
}

function generateTaskIntent(
  taskPlan: TaskPlan,
  firstStep: TaskPlan['steps'][number] | undefined,
  stepCapability: string | undefined,
) {
  const intentStep = selectTaskPlanIntentStep(taskPlan, firstStep);
  const capability = inferStepCapability(intentStep)
    ?? stepCapability
    ?? stringField(firstStep, 'capability')
    ?? taskPlan.task_type;
  const toolShortName = intentToolShortNames[capability] ?? capability;
  const action = intentActionByCapability[capability] ?? '执行任务';
  return `使用 ${toolShortName} ${action}了 ${formatIntentTarget(taskPlan, intentStep)}`;
}

function generateTaskIntentFromJournal(journal: Record<string, unknown>) {
  const intentStep = selectJournalIntentStep(journal);
  const capability = inferStepCapability(intentStep)
    ?? intentCapabilityByTaskType[stringField(journal, 'task_type') ?? '']
    ?? stringField(journal, 'task_type');
  if (!capability) {
    return undefined;
  }

  const toolShortName = intentToolShortNames[capability] ?? capability;
  const action = intentActionByCapability[capability] ?? '执行任务';
  return `使用 ${toolShortName} ${action}了 ${formatJournalIntentTarget(journal, intentStep)}`;
}

function formatIntentTarget(taskPlan: TaskPlan, firstStep: TaskPlan['steps'][number] | undefined) {
  const target = getRecord(firstStep as Record<string, unknown> | undefined, 'target');
  const write = getRecord(firstStep as Record<string, unknown> | undefined, 'write');
  const ops = Array.isArray(write?.['ops']) ? write['ops'] : [];
  const firstOp = ops.find((op): op is Record<string, unknown> =>
    op !== null && typeof op === 'object' && !Array.isArray(op),
  );

  const assetPath = stringField(target, 'asset_path') ?? taskPlan.target_assets[0] ?? taskPlan.task_name ?? taskPlan.task_type;
  const assetName = shortTargetName(assetPath);
  const childName = stringField(target, 'graph')
    ?? stringField(target, 'function_name')
    ?? stringField(target, 'function')
    ?? stringField(target, 'row_name')
    ?? stringField(target, 'component_name')
    ?? stringField(firstOp, 'graph_name')
    ?? stringField(firstOp, 'function_name')
    ?? stringField(firstOp, 'event_name')
    ?? stringField(firstOp, 'dispatcher_name')
    ?? stringField(firstOp, 'signature_name')
    ?? stringField(firstOp, 'variable_name')
    ?? stringField(firstOp, 'name')
    ?? stringField(firstOp, 'property_path')
    ?? stringField(firstOp, 'row_name')
    ?? stringField(firstOp, 'block_id');

  return childName ? `${assetName}.${childName}` : assetName;
}

function formatJournalIntentTarget(
  journal: Record<string, unknown>,
  firstStep: Record<string, unknown> | undefined,
) {
  const stepTarget = getRecord(firstStep, 'target');
  const result = getRecord(firstStep, 'result');
  const resultTarget = getRecord(result, 'target');
  const data = getRecord(result, 'data');
  const appendResult = getRecord(data, 'append_result');
  const appendGraph = getRecord(appendResult, 'graph');
  const writeRef = getRecord(data, 'write_ref');
  const targetAssets = Array.isArray(journal['target_assets']) ? journal['target_assets'] : [];
  const firstTargetAsset = targetAssets.find((asset): asset is string => typeof asset === 'string');

  const assetPath = stringField(stepTarget, 'asset_path')
    ?? stringField(resultTarget, 'asset_path')
    ?? firstTargetAsset
    ?? stringField(journal, 'feature_name')
    ?? stringField(journal, 'task_type')
    ?? 'unknown';
  const assetName = shortTargetName(assetPath);
  const childName = stringField(stepTarget, 'graph')
    ?? stringField(stepTarget, 'graph_name')
    ?? stringField(stepTarget, 'function_name')
    ?? stringField(stepTarget, 'member_name')
    ?? stringField(stepTarget, 'row_name')
    ?? stringField(stepTarget, 'component_name')
    ?? stringField(resultTarget, 'graph')
    ?? stringField(resultTarget, 'graph_name')
    ?? stringField(resultTarget, 'function_name')
    ?? stringField(resultTarget, 'member_name')
    ?? stringField(resultTarget, 'row_name')
    ?? stringField(resultTarget, 'component_name')
    ?? stringField(appendGraph, 'graph_name')
    ?? stringField(data, 'graph_name')
    ?? stringField(data, 'function_name')
    ?? stringField(data, 'member_name')
    ?? stringField(data, 'row_name')
    ?? stringField(data, 'component_name')
    ?? stringField(writeRef, 'block_id');

  return childName ? `${assetName}.${childName}` : assetName;
}

function selectTaskPlanIntentStep(taskPlan: TaskPlan, fallbackStep: TaskPlan['steps'][number] | undefined) {
  const preferredCapability = intentCapabilityByTaskType[taskPlan.task_type];
  if (preferredCapability) {
    const preferredStep = taskPlan.steps.find((step) => inferStepCapability(step) === preferredCapability);
    if (preferredStep) {
      return preferredStep;
    }
  }

  const nonDependencyStep = taskPlan.steps.find((step) => inferStepCapability(step) !== 'blueprint_signature');
  return nonDependencyStep ?? fallbackStep;
}

function selectJournalIntentStep(journal: Record<string, unknown>) {
  const steps = Array.isArray(journal['steps'])
    ? journal['steps'].filter((step): step is Record<string, unknown> =>
        step !== null && typeof step === 'object' && !Array.isArray(step),
      )
    : [];

  const preferredCapability = intentCapabilityByTaskType[stringField(journal, 'task_type') ?? ''];
  if (preferredCapability) {
    const preferredStep = steps.find((step) => inferStepCapability(step) === preferredCapability);
    if (preferredStep) {
      return preferredStep;
    }
  }

  return steps.find((step) => inferStepCapability(step) !== 'blueprint_signature') ?? steps[0];
}

function inferStepCapability(step: unknown) {
  const explicitCapability = stringField(step, 'capability');
  if (explicitCapability) {
    return explicitCapability;
  }

  const adapterOperation = stringField(step, 'adapter_operation');
  if (adapterOperation && intentCapabilityByOperation[adapterOperation]) {
    return intentCapabilityByOperation[adapterOperation];
  }

  const operation = stringField(step, 'operation');
  return operation ? intentCapabilityByOperation[operation] : undefined;
}

function shortTargetName(path: string) {
  const lastSlash = path.split('/').filter(Boolean).pop() ?? path;
  return lastSlash.split('.').filter(Boolean).pop() ?? lastSlash;
}
