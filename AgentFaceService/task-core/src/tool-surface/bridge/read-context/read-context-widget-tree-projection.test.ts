import assert from 'node:assert/strict';
import test from 'node:test';

import {
  buildWidgetTreeJsonPayload,
  buildWidgetTreeLogicFlowPayload,
  WIDGET_TREE_JSON_SCHEMA,
} from './read-context-widget-tree-projection.js';

test('widget_tree tree_json omits empty slot fields and builds virtual_index', () => {
  const payload = buildWidgetTreeJsonPayload({
    asset_path: '/Game/UI/WBP_Menu',
    root_widget: 'Canvas_Root',
    widgets: [
      { name: 'Canvas_Root', class: 'CanvasPanel', child_count: 1, depth: 0 },
      { name: 'Button_Start', class: 'Button', parent: 'Canvas_Root', slot_class: 'CanvasPanelSlot', child_count: 0, depth: 1 },
    ],
  });
  assert.equal(payload.schema, WIDGET_TREE_JSON_SCHEMA);
  assert.equal(payload.format, 'tree_json');
  const root = payload.root as Record<string, unknown>;
  assert.equal(root.slot_class_path, undefined);
  assert.equal(root.slot_name, undefined);
  const child = (root.children as Record<string, unknown>[])[0];
  assert.equal(child.virtual_index, 0);
});

test('widget_tree tree_json preserves slot property facts for read-write closure', () => {
  const payload = buildWidgetTreeJsonPayload({
    asset_path: '/Game/UI/WBP_Menu',
    root: {
      widget_name: 'Canvas_Root',
      widget_class_path: '/Script/UMG.CanvasPanel',
      virtual_index: 0,
      children: [
        {
          widget_name: 'Button_Start',
          widget_class_path: '/Script/UMG.Button',
          parent_name: 'Canvas_Root',
          slot_class_path: '/Script/UMG.CanvasPanelSlot',
          slot_properties: {
            'LayoutData.Offsets.Left': '24.000000',
          },
          virtual_index: 0,
          children: [],
        },
      ],
    },
  });

  const root = payload.root as Record<string, unknown>;
  const child = (root.children as Record<string, unknown>[])[0];
  assert.deepEqual(child.slot_properties, {
    'LayoutData.Offsets.Left': '24.000000',
  });
  const index = payload.index as Record<string, Record<string, unknown>>;
  assert.deepEqual(index.Button_Start.slot_properties, {
    'LayoutData.Offsets.Left': '24.000000',
  });
});

test('widget_tree logic_flow renders hierarchy and named slot content', () => {
  const payload = buildWidgetTreeLogicFlowPayload({
    asset_path: '/Game/UI/WBP_Menu',
    root: {
      widget_name: 'Canvas_Root',
      widget_class_path: '/Script/UMG.CanvasPanel',
      virtual_index: 0,
      children: [
        { widget_name: 'Dialog_Shell', widget_class_path: '/Script/UMG.ExpandableArea', virtual_index: 0, children: [] },
      ],
    },
    index: {
      Canvas_Root: { widget_class_path: '/Script/UMG.CanvasPanel', virtual_index: 0 },
      Dialog_Shell: {
        widget_class_path: '/Script/UMG.ExpandableArea',
        parent_name: 'Canvas_Root',
        virtual_index: 0,
      },
      BodyText: {
        widget_class_path: '/Script/UMG.TextBlock',
        parent_name: 'Dialog_Shell',
        slot_name: 'Body',
        virtual_index: 0,
      },
    },
    named_slots: [
      {
        host_widget_name: 'Dialog_Shell',
        slot_name: 'Body',
        content_widget_name: 'BodyText',
        virtual_index: 0,
      },
    ],
  });
  assert.equal(payload.flow, 'widgetroot[CanvasPanel] -> (Dialog_Shell[ExpandableArea](Body[NamedSlot](BodyText[TextBlock])))');
  assert.deepEqual(payload.warnings, []);
});

test('widget_tree logic_flow warns when compact flow drops authoritative index facts', () => {
  const payload = buildWidgetTreeLogicFlowPayload({
    asset_path: '/Game/UI/WBP_Menu',
    root: {
      widget_name: 'Canvas_Root',
      widget_class_path: '/Script/UMG.CanvasPanel',
      virtual_index: 0,
      children: [],
    },
    index: {
      Canvas_Root: { widget_class_path: '/Script/UMG.CanvasPanel', virtual_index: 0 },
      InheritedButton: {
        widget_class_path: '/Script/UMG.Button',
        parent_name: 'Canvas_Root',
        virtual_index: 0,
        is_inherited: true,
      },
      MissingClassWidget: {
        parent_name: 'Canvas_Root',
        virtual_index: 1,
      },
    },
  });

  assert.deepEqual(payload.warnings, [
    'widget_tree_logic_flow_degraded_unrepresented_index_entry',
    'widget_tree_logic_flow_degraded_inherited_widget',
    'widget_tree_logic_flow_degraded_missing_widget_identity',
  ]);
});
