#!/usr/bin/env node

import fs from 'node:fs';
import process from 'node:process';
import readline from 'node:readline';
import { readJsonFile } from './json-input.mjs';

const EXIT_CANCELLED = 20;
const AGENT_NAMES = ['blueprint-explorer', 'sourcecode-explorer', 'sourcecode-worker', 'task-worker'];

const CODEX_MODEL_OPTIONS = [
  { value: 'gpt-5.4-mini', tip: '轻量上下文读取，推荐给 blueprint-explorer。' },
  { value: 'gpt-5.3-codex-spark', tip: '高强度源码和 schema 探索，推荐给 sourcecode-explorer。' },
  { value: 'gpt-5.5', tip: '强力源码开发与架构变更，推荐给 sourcecode-worker。' },
  { value: 'gpt-5.4', tip: '更强的任务执行和 TaskSpec 构造，推荐给 task-worker。' },
];
const CODEX_REASONING_OPTIONS = [
  { value: 'high', tip: '推荐默认思考等级，适合大多数安装。' },
  { value: 'xhigh', tip: '更深推理，推荐给源码探索等复杂上下文任务。' },
];

const CLAUDE_MODEL_OPTIONS = [
  { value: 'haiku', tip: '轻量 sideAgent，推荐给 explorer 类任务。' },
  { value: 'sonnet', tip: '更强 sideAgent，推荐给 task-worker。' },
  { value: 'opus', tip: '强力开发 sideAgent，推荐给 sourcecode-worker。' },
];
const CLAUDE_REASONING_OPTIONS = [
  { value: 'high', tip: '推荐默认 extended thinking 等级。' },
  { value: 'xhigh', tip: '更深 extended thinking，适合复杂读写派发或 TaskSpec 构造。' },
];

const args = parseArgs(process.argv.slice(2));
const defaultsPath = args.defaults;
const outputPath = args.out;

if (!defaultsPath || !outputPath) {
  console.error('Usage: node InstallScripts/install-prompts.mjs --defaults <defaults.json> --out <selection.json>');
  process.exit(2);
}

const defaults = await readJsonFile(defaultsPath);
const options = createInstallOptions(defaults.options ?? {});
const paths = {
  projectFile: normalizeString(defaults.paths?.projectFile),
  engineRoot: normalizeString(defaults.paths?.engineRoot),
  enginePluginDir: normalizeString(defaults.paths?.enginePluginDir),
  projectEditorTarget: normalizeString(defaults.paths?.projectEditorTarget),
};

const profileDefaults = {
  codex: normalizeProfile(defaults.profiles?.codex),
  claude: normalizeProfile(defaults.profiles?.claude),
};

if (args['accept-defaults']) {
  const selection = buildSelection(options, paths, profileDefaults);
  writeJsonFile(outputPath, selection);
  process.exit(0);
}

const forceLineMode = Boolean(args['line-mode'] || args['no-raw']);
const rawOnly = Boolean(args['raw-only']);
const supportsRawTerminal = Boolean(
  !forceLineMode
  && process.stdin.isTTY
  && process.stdout.isTTY
  && typeof process.stdin.setRawMode === 'function',
);
let sharedLineReader = null;
const sharedLineQueue = [];
const sharedLineWaiters = [];
let sharedLineClosed = false;

if (supportsRawTerminal) {
  readline.emitKeypressEvents(process.stdin);
}

try {
  if (supportsRawTerminal) {
    await run();
  } else if (rawOnly) {
    console.error('BlueprintHelper Node install prompts require a raw interactive terminal.');
    process.exit(10);
  } else {
    await runLineMode();
  }
} catch (error) {
  restoreTerminal();
  if (error && error.code === 'BLUEPRINTHELPER_INSTALL_CANCELLED') {
    console.error('Install cancelled by user.');
    process.exit(EXIT_CANCELLED);
  }
  console.error(error?.message ?? String(error));
  process.exit(1);
}

async function run() {
  enableRawMode();
  updateInstallOptionDependencies(options);

  await runInstallOptionMenu(options);
  await collectPathDetails(options, paths);

  const selectedProfiles = {
    codex: optionSelected(options, 'codexAgents')
      ? await runAgentProfileForm({
        title: 'Codex subagent 模型配置',
        profile: cloneProfile(profileDefaults.codex),
        modelOptions: CODEX_MODEL_OPTIONS,
        reasoningOptions: CODEX_REASONING_OPTIONS,
      })
      : null,
    claude: (optionSelected(options, 'claudePlugin') || optionSelected(options, 'claudeAgents'))
      ? await runAgentProfileForm({
        title: 'Claude sideAgent 模型配置',
        profile: cloneProfile(profileDefaults.claude),
        modelOptions: CLAUDE_MODEL_OPTIONS,
        reasoningOptions: CLAUDE_REASONING_OPTIONS,
      })
      : null,
  };

  const selection = buildSelection(options, paths, selectedProfiles);
  await runConfirmation(selection);
  writeJsonFile(outputPath, selection);
  restoreTerminal();
  console.log('BlueprintHelper install selections confirmed.');
}

async function runLineMode() {
  updateInstallOptionDependencies(options);

  await runInstallOptionLinePrompt(options);
  await collectPathDetails(options, paths);

  const selectedProfiles = {
    codex: optionSelected(options, 'codexAgents')
      ? await runAgentProfileLineForm({
        title: 'Codex subagent profiles',
        profile: cloneProfile(profileDefaults.codex),
        modelOptions: CODEX_MODEL_OPTIONS,
        reasoningOptions: CODEX_REASONING_OPTIONS,
      })
      : null,
    claude: (optionSelected(options, 'claudePlugin') || optionSelected(options, 'claudeAgents'))
      ? await runAgentProfileLineForm({
        title: 'Claude sideAgent profiles',
        profile: cloneProfile(profileDefaults.claude),
        modelOptions: CLAUDE_MODEL_OPTIONS,
        reasoningOptions: CLAUDE_REASONING_OPTIONS,
      })
      : null,
  };

  const selection = buildSelection(options, paths, selectedProfiles);
  await runConfirmationLineMode(selection);
  writeJsonFile(outputPath, selection);
  restoreTerminal();
  console.log('BlueprintHelper install selections confirmed.');
}

function createInstallOptions(input) {
  return [
    makeOption('build', '构建 AgentFaceService packages', bool(input.build, true), '安装并构建 task-core、CLI 和 MCP 兼容包。需要 Node.js 和 npm 在 PATH 中。'),
    makeOption('cliLink', '全局链接 bh CLI', bool(input.cliLink, true), '运行 npm link，让 bh / blueprinthelper-cli 成为全局命令，并清理可能被 ExecutionPolicy 拦截的 PowerShell shim。'),
    makeOption('codexSupport', '安装 Codex Desktop 支持', bool(input.codexSupport, true), '启用 Codex 集成。子项控制本地插件配置、subagents 和 lifecycle MCP 配置。'),
    makeOption('codexMarketplace', '注册 Codex 本地插件配置', bool(input.codexMarketplace, true), '直接写入 Codex config.toml：本地 marketplace、启用插件和当前源码目录。', 1, 'codexSupport'),
    makeOption('codexAgents', '安装 Codex subagents', bool(input.codexAgents, true), '把 BlueprintHelper Codex subagent 定义安装到用户 .codex/agents 目录。', 1, 'codexSupport'),
    makeOption('lifecycleMcp', '安装 lifecycle MCP 配置', bool(input.lifecycleMcp, true), '安装只用于打开/关闭 Unreal Editor 的全局 MCP allowlist 配置。', 1, 'codexSupport'),
    makeOption('claudePlugin', '安装 Claude Code 插件支持', bool(input.claudePlugin, false), '直接写入 Claude settings.json：本地 marketplace 和启用插件。选择此项会同步安装 Claude sideAgents。'),
    makeOption('claudeAgents', '安装 Claude sideAgent 定义', bool(input.claudeAgents, false), '只复制 Claude sideAgent 定义；如果选择 Claude 插件支持，此项会自动启用。'),
    makeOption('projectProfile', '写入项目 project-profile.json', bool(input.projectProfile, true), '创建或更新 .blueprinthelper/project-profile.json，并刷新 .blueprinthelper/AgentWorkFlow.md 与项目根 AGENTS.md / CLAUDE.md marker。'),
    makeOption('projectUbtCompile', '运行一次项目 UBT 编译', bool(input.projectUbtCompile, true), '安装完成后使用 .uproject 和 UE root 运行 Build.bat <ProjectName>Editor Win64 Development，验证 UE 侧插件可被目标项目编译。'),
    makeOption('defaultPreferences', '创建默认用户偏好文件', bool(input.defaultPreferences, true), '只在缺失时创建 Claude/Codex BlueprintHelper 用户偏好文件，不覆盖已有偏好。'),
    makeOption('diagnostics', '安装后运行 diagnostics', bool(input.diagnostics, false), '安装完成后运行 BlueprintHelper 静态诊断，用于验证 CLI、profile、Bridge 和运行时配置。'),
    makeOption('ueEnginePlugin', '复制 UE 插件到 Engine', bool(input.ueEnginePlugin, false), '把 UE 侧 BlueprintHelper 插件复制到 Engine/Plugins/Marketplace。确认选单后会询问目标路径。'),
    makeOption('force', '允许替换已有目标', bool(input.force, false), '允许安装器在需要时替换已有本地链接或 Engine 插件目标。'),
  ];
}

function makeOption(key, label, selected, tip, indent = 0, parent = '') {
  return {
    key,
    label,
    selected,
    tip,
    indent,
    parent,
    enabled: true,
  };
}

async function runInstallOptionMenu(items) {
  let index = 0;

  while (true) {
    renderInstallOptionMenu(items, index);
    const key = await readKey();

    if (isCancelKey(key)) {
      throwCancelled();
    }
    if (key.name === 'up') {
      index = previousEnabledIndex(items, index);
    } else if (key.name === 'down') {
      index = nextEnabledIndex(items, index);
    } else if (key.name === 'space') {
      toggleInstallOption(items, items[index]);
    } else if (key.name === 'return') {
      return;
    }
  }
}

function renderInstallOptionMenu(items, index) {
  clearScreen();
  writeLine('BlueprintHelper 交互式安装');
  writeLine(`Source root: ${defaults.root ?? process.cwd()}`);
  writeLine('');
  writeLine('↑/↓ 选择，Space 勾选，Enter 继续，Esc 取消');
  writeLine('');

  for (let itemIndex = 0; itemIndex < items.length; itemIndex += 1) {
    const item = items[itemIndex];
    const cursor = itemIndex === index ? '>' : ' ';
    const check = item.enabled ? (item.selected ? '[x]' : '[ ]') : '[-]';
    const indent = '  '.repeat(item.indent);
    const text = `${cursor} ${indent}${check} ${item.label}`;
    writeLine(item.enabled ? text : dim(text));
  }

  const current = items[index];
  writeLine('');
  writeLine('Tip:');
  writeWrapped(current.enabled ? current.tip : `${current.tip} 请先启用父选项。`);
}

function updateInstallOptionDependencies(items) {
  const codexSupport = optionByKey(items, 'codexSupport');
  for (const key of ['codexMarketplace', 'codexAgents', 'lifecycleMcp']) {
    const child = optionByKey(items, key);
    child.enabled = Boolean(codexSupport.selected);
    if (!codexSupport.selected) {
      child.selected = false;
    }
  }

  const claudePlugin = optionByKey(items, 'claudePlugin');
  const claudeAgents = optionByKey(items, 'claudeAgents');
  if (claudePlugin.selected) {
    claudeAgents.selected = true;
    claudeAgents.enabled = false;
  } else {
    claudeAgents.enabled = true;
  }
}

function toggleInstallOption(items, item) {
  if (!item.enabled) {
    return;
  }

  item.selected = !item.selected;

  if (item.key === 'codexSupport') {
    for (const key of ['codexMarketplace', 'codexAgents', 'lifecycleMcp']) {
      optionByKey(items, key).selected = item.selected;
    }
  } else if (item.parent === 'codexSupport') {
    optionByKey(items, 'codexSupport').selected = ['codexMarketplace', 'codexAgents', 'lifecycleMcp']
      .some((key) => optionByKey(items, key).selected);
  }

  updateInstallOptionDependencies(items);
}

async function runInstallOptionLinePrompt(items) {
  while (true) {
    renderInstallOptionLinePrompt(items);
    const answer = normalizeString(await question('Toggle number(s), ?number for tip, Enter to continue, Q to cancel: '));
    const command = answer.toLowerCase();

    if (!command) {
      return;
    }
    if (command === 'q' || command === 'quit' || command === 'cancel') {
      throwCancelled();
    }
    if (command.startsWith('?')) {
      const item = optionFromLineInput(items, command.slice(1));
      if (item) {
        writeLine('');
        writeLine(`${item.label}:`);
        writeWrapped(item.tip);
        await question('Press Enter to continue...');
      } else {
        writeLine('Unknown option. Use ?number, for example ?3.');
        await question('Press Enter to continue...');
      }
      continue;
    }

    const tokens = command.split(/[,\s]+/).filter(Boolean);
    let changed = false;
    for (const token of tokens) {
      const item = optionFromLineInput(items, token);
      if (!item) {
        writeLine(`Unknown option: ${token}`);
        continue;
      }
      if (!item.enabled) {
        writeLine(`Option is disabled by its parent: ${item.label}`);
        continue;
      }
      toggleInstallOption(items, item);
      changed = true;
    }

    if (!changed) {
      await question('No option changed. Press Enter to continue...');
    }
  }
}

function renderInstallOptionLinePrompt(items) {
  clearScreen();
  writeLine('BlueprintHelper Node install prompts');
  writeLine(`Source root: ${defaults.root ?? process.cwd()}`);
  writeLine('');
  writeLine('Use the numbered Node prompt below.');
  writeLine('Enter one or more numbers to toggle options, ?number to show a tip, or press Enter to continue.');
  writeLine('');

  for (let itemIndex = 0; itemIndex < items.length; itemIndex += 1) {
    const item = items[itemIndex];
    const number = String(itemIndex + 1).padStart(2, ' ');
    const check = item.enabled ? (item.selected ? '[x]' : '[ ]') : '[-]';
    const indent = '  '.repeat(item.indent);
    writeLine(`${number}. ${indent}${check} ${item.label}`);
  }
  writeLine('');
}

function optionFromLineInput(items, input) {
  const text = normalizeString(input).toLowerCase();
  if (!text) {
    return null;
  }

  const number = Number.parseInt(text, 10);
  if (Number.isInteger(number) && number >= 1 && number <= items.length) {
    return items[number - 1];
  }

  return items.find((item) => item.key.toLowerCase() === text) ?? null;
}

async function collectPathDetails(items, targetPaths) {
  const projectProfile = optionSelected(items, 'projectProfile');
  const projectUbtCompile = optionSelected(items, 'projectUbtCompile');
  const ueEnginePlugin = optionSelected(items, 'ueEnginePlugin');

  if (!projectProfile && !projectUbtCompile && !ueEnginePlugin) {
    return;
  }

  clearScreen();
  writeLine('BlueprintHelper 路径设置');
  writeLine('');
  restoreTerminalForPrompt();

  if (projectProfile || projectUbtCompile) {
    targetPaths.projectFile = await promptText('Project .uproject path，留空自动检测', targetPaths.projectFile);
    targetPaths.engineRoot = await promptText('UE root，例如 E:\\UE_5.6 或 E:\\UE_5.6\\Engine', targetPaths.engineRoot);
  }

  if (projectUbtCompile) {
    targetPaths.projectEditorTarget = await promptText('Project Editor target，留空自动检测', targetPaths.projectEditorTarget);
  }

  if (ueEnginePlugin) {
    targetPaths.enginePluginDir = await promptText('Engine plugin target directory，留空从 UE root 推导', targetPaths.enginePluginDir);
    if (!targetPaths.engineRoot && !targetPaths.enginePluginDir) {
      targetPaths.engineRoot = await promptText('UE root required for engine plugin install', targetPaths.engineRoot);
    }
    if (!targetPaths.engineRoot && !targetPaths.enginePluginDir) {
      optionByKey(items, 'ueEnginePlugin').selected = false;
      writeLine('');
      writeLine('未提供 UE root 或 Engine plugin target directory，本次跳过 Engine 插件复制。');
      await promptText('按 Enter 继续', '');
    }
  }

  enableRawMode();
}

async function runAgentProfileForm({ title, profile, modelOptions, reasoningOptions }) {
  let index = 0;
  const fields = AGENT_NAMES.flatMap((agentName) => ([
    { agentName, field: 'model' },
    { agentName, field: 'reasoning' },
  ]));

  for (const agentName of AGENT_NAMES) {
    profile.agents[agentName] = normalizeAgentProfile(profile.agents[agentName], modelOptions, reasoningOptions);
  }

  while (true) {
    renderAgentProfileForm(title, profile, fields, index, modelOptions, reasoningOptions);
    const key = await readKey();

    if (isCancelKey(key)) {
      throwCancelled();
    }
    if (key.name === 'up') {
      index = (index + fields.length - 1) % fields.length;
    } else if (key.name === 'down') {
      index = (index + 1) % fields.length;
    } else if (key.name === 'left') {
      cycleProfileValue(profile, fields[index], modelOptions, reasoningOptions, -1);
    } else if (key.name === 'right' || key.name === 'space') {
      cycleProfileValue(profile, fields[index], modelOptions, reasoningOptions, 1);
    } else if ((key.sequence ?? '').toLowerCase() === 'r') {
      profile = normalizeProfile(title.startsWith('Codex') ? defaults.profiles?.codex : defaults.profiles?.claude);
    } else if (key.name === 'return') {
      return profile;
    }
  }
}

function renderAgentProfileForm(title, profile, fields, index, modelOptions, reasoningOptions) {
  clearScreen();
  writeLine(title);
  writeLine('');
  writeLine('↑/↓ 选择字段，←/→ 或 Space 切换值，R 重置推荐值，Enter 继续，Esc 取消');
  writeLine('');
  writeLine('Agent                 Model                         Thinking');
  writeLine('--------------------------------------------------------------');

  for (const agentName of AGENT_NAMES) {
    const modelFieldIndex = fields.findIndex((field) => field.agentName === agentName && field.field === 'model');
    const reasoningFieldIndex = fields.findIndex((field) => field.agentName === agentName && field.field === 'reasoning');
    const selectedModel = modelFieldIndex === index;
    const selectedReasoning = reasoningFieldIndex === index;
    const agentProfile = profile.agents[agentName];
    const cursor = selectedModel || selectedReasoning ? '>' : ' ';
    const modelText = selectedModel ? `[${agentProfile.model}]` : ` ${agentProfile.model} `;
    const reasoningText = selectedReasoning ? `[${agentProfile.reasoning}]` : ` ${agentProfile.reasoning} `;
    writeLine(`${cursor} ${pad(agentName, 20)} ${pad(modelText, 29)} ${reasoningText}`);
  }

  const current = fields[index];
  const value = profile.agents[current.agentName][current.field];
  const optionList = current.field === 'model' ? modelOptions : reasoningOptions;
  const currentOption = optionList.find((item) => item.value === value);
  writeLine('');
  writeLine('Tip:');
  writeWrapped(`${current.agentName} ${current.field === 'model' ? '模型' : '思考等级'}：${currentOption?.tip ?? '当前安装选项。'}`);
}

function cycleProfileValue(profile, field, modelOptions, reasoningOptions, direction) {
  const agentProfile = profile.agents[field.agentName];
  const optionList = field.field === 'model' ? modelOptions : reasoningOptions;
  const values = optionList.map((item) => item.value);
  const currentValue = agentProfile[field.field];
  const currentIndex = Math.max(0, values.indexOf(currentValue));
  const nextIndex = (currentIndex + direction + values.length) % values.length;
  agentProfile[field.field] = values[nextIndex];
  agentProfile.reasoning_effort = agentProfile.reasoning;
  agentProfile.model_reasoning_effort = agentProfile.reasoning;
}

async function runAgentProfileLineForm({ title, profile, modelOptions, reasoningOptions }) {
  for (const agentName of AGENT_NAMES) {
    profile.agents[agentName] = normalizeAgentProfile(profile.agents[agentName], modelOptions, reasoningOptions);
  }

  clearScreen();
  writeLine(title);
  writeLine('');
  writeLine('Choose model and thinking level separately. Press Enter to keep the recommended value.');
  writeLine('');

  for (const agentName of AGENT_NAMES) {
    const agentProfile = profile.agents[agentName];
    agentProfile.model = await promptChoice(`${agentName} model`, modelOptions, agentProfile.model);
    agentProfile.reasoning = await promptChoice(`${agentName} thinking`, reasoningOptions, agentProfile.reasoning);
    agentProfile.reasoning_effort = agentProfile.reasoning;
    agentProfile.model_reasoning_effort = agentProfile.reasoning;
    writeLine('');
  }

  return profile;
}

async function promptChoice(label, choices, defaultValue) {
  while (true) {
    writeLine(label);
    for (let index = 0; index < choices.length; index += 1) {
      const choice = choices[index];
      const marker = choice.value === defaultValue ? '*' : ' ';
      writeLine(`  ${index + 1}. ${marker} ${choice.value}`);
      writeWrapped(`     ${choice.tip}`);
    }

    const answer = normalizeString(await question(`${label} [${defaultValue}]: `));
    if (!answer) {
      return defaultValue;
    }
    if (answer.toLowerCase() === 'q' || answer.toLowerCase() === 'cancel') {
      throwCancelled();
    }

    const selected = choiceFromInput(choices, answer);
    if (selected) {
      return selected.value;
    }

    writeLine('Invalid choice. Enter a number or value, or press Enter for the default.');
  }
}

function choiceFromInput(choices, input) {
  const text = normalizeString(input).toLowerCase();
  const number = Number.parseInt(text, 10);
  if (Number.isInteger(number) && number >= 1 && number <= choices.length) {
    return choices[number - 1];
  }

  return choices.find((choice) => choice.value.toLowerCase() === text) ?? null;
}

async function runConfirmation(selection) {
  while (true) {
    clearScreen();
    writeLine('BlueprintHelper 安装确认');
    writeLine('');
    writeLine(formatOptionSummary(selection.options));
    writeLine('');
    if (selection.paths.projectFile || selection.paths.engineRoot || selection.paths.enginePluginDir || selection.paths.projectEditorTarget) {
      writeLine('Paths:');
      writeLine(`  ProjectFile: ${selection.paths.projectFile || '(auto-detect)'}`);
      writeLine(`  EngineRoot: ${selection.paths.engineRoot || '(not set)'}`);
      writeLine(`  EnginePluginDir: ${selection.paths.enginePluginDir || '(derive or skip)'}`);
      writeLine(`  ProjectEditorTarget: ${selection.paths.projectEditorTarget || '(auto-detect)'}`);
      writeLine('');
    }
    if (selection.profiles.codex) {
      writeLine(`Codex subagents: ${formatProfileSummary(selection.profiles.codex)}`);
    }
    if (selection.profiles.claude) {
      writeLine(`Claude sideAgents: ${formatProfileSummary(selection.profiles.claude)}`);
    }
    writeLine('');
    writeLine('Enter/Y 开始安装，N/Esc 取消');

    const key = await readKey();
    const sequence = (key.sequence ?? '').toLowerCase();
    if (key.name === 'return' || sequence === 'y') {
      return;
    }
    if (isCancelKey(key) || sequence === 'n') {
      throwCancelled();
    }
  }
}

async function runConfirmationLineMode(selection) {
  while (true) {
    clearScreen();
    writeLine('BlueprintHelper install confirmation');
    writeLine('');
    writeLine(formatOptionSummary(selection.options));
    writeLine('');
    if (selection.paths.projectFile || selection.paths.engineRoot || selection.paths.enginePluginDir || selection.paths.projectEditorTarget) {
      writeLine('Paths:');
      writeLine(`  ProjectFile: ${selection.paths.projectFile || '(auto-detect)'}`);
      writeLine(`  EngineRoot: ${selection.paths.engineRoot || '(not set)'}`);
      writeLine(`  EnginePluginDir: ${selection.paths.enginePluginDir || '(derive or skip)'}`);
      writeLine(`  ProjectEditorTarget: ${selection.paths.projectEditorTarget || '(auto-detect)'}`);
      writeLine('');
    }
    if (selection.profiles.codex) {
      writeLine(`Codex subagents: ${formatProfileSummary(selection.profiles.codex)}`);
    }
    if (selection.profiles.claude) {
      writeLine(`Claude sideAgents: ${formatProfileSummary(selection.profiles.claude)}`);
    }
    writeLine('');

    const answer = normalizeString(await question('Start install? [Y/n]: ')).toLowerCase();
    if (!answer || answer === 'y' || answer === 'yes') {
      return;
    }
    if (answer === 'n' || answer === 'no' || answer === 'q' || answer === 'cancel') {
      throwCancelled();
    }

    writeLine('Enter Y to start or N to cancel.');
  }
}

function buildSelection(items, selectedPaths, selectedProfiles) {
  const outputOptions = {
    build: optionSelected(items, 'build'),
    cliLink: optionSelected(items, 'cliLink'),
    codexSupport: optionSelected(items, 'codexSupport'),
    codexMarketplace: optionSelected(items, 'codexSupport') && optionSelected(items, 'codexMarketplace'),
    codexAgents: optionSelected(items, 'codexSupport') && optionSelected(items, 'codexAgents'),
    lifecycleMcp: optionSelected(items, 'codexSupport') && optionSelected(items, 'lifecycleMcp'),
    claudePlugin: optionSelected(items, 'claudePlugin'),
    claudeAgents: optionSelected(items, 'claudePlugin') || optionSelected(items, 'claudeAgents'),
    projectProfile: optionSelected(items, 'projectProfile'),
    projectUbtCompile: optionSelected(items, 'projectUbtCompile'),
    defaultPreferences: optionSelected(items, 'defaultPreferences'),
    diagnostics: optionSelected(items, 'diagnostics'),
    ueEnginePlugin: optionSelected(items, 'ueEnginePlugin'),
    force: optionSelected(items, 'force'),
  };

  return {
    schema: 'BlueprintHelper.InstallSelection.v1',
    options: outputOptions,
    paths: {
      projectFile: normalizeString(selectedPaths.projectFile),
      engineRoot: normalizeString(selectedPaths.engineRoot),
      enginePluginDir: normalizeString(selectedPaths.enginePluginDir),
      projectEditorTarget: normalizeString(selectedPaths.projectEditorTarget),
    },
    profiles: {
      codex: outputOptions.codexAgents ? normalizeProfile(selectedProfiles.codex) : null,
      claude: outputOptions.claudeAgents ? normalizeProfile(selectedProfiles.claude) : null,
    },
  };
}

function normalizeProfile(profile) {
  const agents = {};
  for (const agentName of AGENT_NAMES) {
    agents[agentName] = normalizeAgentProfile(profile?.agents?.[agentName], [], []);
  }
  return { agents };
}

function normalizeAgentProfile(profile, modelOptions, reasoningOptions) {
  const firstModel = modelOptions[0]?.value ?? 'default';
  const firstReasoning = reasoningOptions[0]?.value ?? 'high';
  const model = normalizeString(profile?.model) || firstModel;
  const reasoning = normalizeString(profile?.reasoning)
    || normalizeString(profile?.model_reasoning_effort)
    || normalizeString(profile?.reasoning_effort)
    || firstReasoning;
  return {
    model,
    reasoning,
    reasoning_effort: reasoning,
    model_reasoning_effort: reasoning,
  };
}

function cloneProfile(profile) {
  return JSON.parse(JSON.stringify(normalizeProfile(profile)));
}

function formatOptionSummary(selectedOptions) {
  const enabled = [];
  if (selectedOptions.build) enabled.push('build');
  if (selectedOptions.cliLink) enabled.push('cli-link');
  if (selectedOptions.codexMarketplace) enabled.push('codex-plugin');
  if (selectedOptions.codexAgents) enabled.push('codex-agents');
  if (selectedOptions.lifecycleMcp) enabled.push('lifecycle-mcp');
  if (selectedOptions.claudePlugin) enabled.push('claude-plugin');
  if (selectedOptions.claudeAgents && !selectedOptions.claudePlugin) enabled.push('claude-agents');
  if (selectedOptions.projectProfile) enabled.push('project-profile');
  if (selectedOptions.projectUbtCompile) enabled.push('project-ubt-compile');
  if (selectedOptions.defaultPreferences) enabled.push('default-preferences');
  if (selectedOptions.diagnostics) enabled.push('diagnostics');
  if (selectedOptions.ueEnginePlugin) enabled.push('ue-engine-plugin');
  if (selectedOptions.force) enabled.push('force');
  return `Selected: ${enabled.length ? enabled.join(', ') : '(none)'}`;
}

function formatProfileSummary(profile) {
  return AGENT_NAMES
    .map((agentName) => {
      const agentProfile = profile.agents[agentName];
      return `${agentName}=${agentProfile.model}/${agentProfile.reasoning}`;
    })
    .join('; ');
}

function optionSelected(items, key) {
  return Boolean(optionByKey(items, key).selected);
}

function optionByKey(items, key) {
  const item = items.find((candidate) => candidate.key === key);
  if (!item) {
    throw new Error(`Unknown install option: ${key}`);
  }
  return item;
}

function nextEnabledIndex(items, index) {
  for (let offset = 1; offset <= items.length; offset += 1) {
    const candidate = (index + offset) % items.length;
    if (items[candidate].enabled) {
      return candidate;
    }
  }
  return index;
}

function previousEnabledIndex(items, index) {
  for (let offset = 1; offset <= items.length; offset += 1) {
    const candidate = (index - offset + items.length) % items.length;
    if (items[candidate].enabled) {
      return candidate;
    }
  }
  return index;
}

function readKey() {
  return new Promise((resolve) => {
    process.stdin.once('keypress', (_str, key) => {
      resolve(key ?? {});
    });
  });
}

function isCancelKey(key) {
  return key.name === 'escape' || (key.ctrl && key.name === 'c');
}

function throwCancelled() {
  const error = new Error('Install cancelled by user.');
  error.code = 'BLUEPRINTHELPER_INSTALL_CANCELLED';
  throw error;
}

async function promptText(label, defaultValue) {
  const suffix = defaultValue ? ` [${defaultValue}]` : '';
  const answer = await question(`${label}${suffix}: `);
  const value = normalizeString(answer);
  return value || normalizeString(defaultValue);
}

function question(prompt) {
  if (!supportsRawTerminal) {
    return questionWithSharedReader(prompt);
  }

  return new Promise((resolve, reject) => {
    const rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout,
      terminal: Boolean(process.stdout.isTTY),
    });
    let settled = false;
    rl.question(prompt, (answer) => {
      settled = true;
      rl.close();
      resolve(answer);
    });
    rl.once('close', () => {
      if (!settled) {
        reject(new Error('Input stream closed before an answer was provided.'));
      }
    });
  });
}

function questionWithSharedReader(prompt) {
  if (!sharedLineReader) {
    sharedLineClosed = false;
    sharedLineReader = readline.createInterface({
      input: process.stdin,
      terminal: Boolean(process.stdout.isTTY),
    });
    sharedLineReader.on('line', (line) => {
      const waiter = sharedLineWaiters.shift();
      if (waiter) {
        waiter.resolve(line);
      } else {
        sharedLineQueue.push(line);
      }
    });
    sharedLineReader.once('close', () => {
      sharedLineClosed = true;
      while (sharedLineWaiters.length) {
        sharedLineWaiters.shift().reject(new Error('Input stream closed before an answer was provided.'));
      }
    });
  }

  return new Promise((resolve, reject) => {
    process.stdout.write(prompt);
    if (sharedLineQueue.length) {
      resolve(sharedLineQueue.shift());
      return;
    }
    if (sharedLineClosed) {
      reject(new Error('Input stream closed before an answer was provided.'));
      return;
    }
    sharedLineWaiters.push({ resolve, reject });
  });
}

function clearScreen() {
  if (supportsTerminalControl()) {
    process.stdout.write('\x1b[2J\x1b[H');
  } else {
    process.stdout.write('\n');
  }
}

function writeLine(value = '') {
  process.stdout.write(`${value}\n`);
}

function writeWrapped(text) {
  const width = Math.max(60, (process.stdout.columns || 100) - 2);
  let remaining = text;
  while (remaining.length > width) {
    let cut = remaining.lastIndexOf(' ', width);
    if (cut < 20) {
      cut = width;
    }
    writeLine(remaining.slice(0, cut));
    remaining = remaining.slice(cut).trimStart();
  }
  writeLine(remaining);
}

function pad(value, width) {
  const text = String(value);
  return text.length >= width ? text : `${text}${' '.repeat(width - text.length)}`;
}

function dim(value) {
  return supportsTerminalControl() ? `\x1b[2m${value}\x1b[0m` : value;
}

function supportsTerminalControl() {
  return Boolean(supportsRawTerminal && process.stdout.isTTY);
}

function enableRawMode() {
  if (process.stdin.isTTY) {
    process.stdin.setRawMode(true);
    process.stdin.resume();
  }
}

function restoreTerminalForPrompt() {
  if (process.stdin.isTTY) {
    process.stdin.setRawMode(false);
  }
}

function restoreTerminal() {
  if (process.stdin.isTTY) {
    process.stdin.setRawMode(false);
    process.stdin.pause();
  }
  if (sharedLineReader) {
    sharedLineReader.close();
    sharedLineReader = null;
  }
  sharedLineQueue.length = 0;
  if (supportsTerminalControl()) {
    process.stdout.write('\x1b[0m');
  }
}

function writeJsonFile(filePath, value) {
  fs.writeFileSync(filePath, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
}

function normalizeString(value) {
  return typeof value === 'string' && value.trim() ? stripOuterQuotes(value.trim()) : '';
}

function stripOuterQuotes(value) {
  let current = value;
  while (
    current.length >= 2
    && ((current.startsWith('"') && current.endsWith('"')) || (current.startsWith("'") && current.endsWith("'")))
  ) {
    current = current.slice(1, -1).trim();
  }
  return current;
}

function bool(value, fallback) {
  return typeof value === 'boolean' ? value : fallback;
}

function parseArgs(argv) {
  const result = {};
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (!arg.startsWith('--')) {
      continue;
    }
    const key = arg.slice(2);
    const next = argv[index + 1];
    if (next && !next.startsWith('--')) {
      result[key] = next;
      index += 1;
    } else {
      result[key] = true;
    }
  }
  return result;
}
