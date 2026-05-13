---
description: Run BlueprintHelper initial setup - configure UE paths, verify Bridge connectivity, build CLI packages, collect safety preferences, and generate SetupProfile
allowed-tools: Read, Write, AskUserQuestion
---

# BlueprintHelper Setup

浣犳鍦ㄨ繍琛?BlueprintHelper 鍒濇閰嶇疆銆傛寜浠ヤ笅姝ラ閫愪竴瀹屾垚锛屼笉瑕佽烦杩囥€?

---

## 闃舵 1锛氳矾寰勯厤缃?

### 1.1 鑾峰彇 UE 寮曟搸鐩綍

璇㈤棶鐢ㄦ埛骞剁‘璁ゅ綋鍓嶉」鐩娇鐢ㄧ殑 UE Engine 缁濆璺緞锛屾枃妗ｄ腑鐢?`<UE_ENGINE_ROOT>` 琛ㄧず璇ヨ矾寰勩€傝璺緞鍙啓鍏ラ」鐩笅鐨?`<ProjectDir>/.blueprinthelper/agent-profile.json`锛屽瓧娈典负 `environment.ue_engine_dir`锛屼笉瑕佸啓鍏?C 鐩樺叏灞€ Claude settings 鎴栨彃浠?env銆?

楠岃瘉瑙勫垯锛?
- 蹇呴』鏄粷瀵硅矾寰?
- 璺緞涓嬪繀椤诲瓨鍦?`Engine/Binaries/Win64/UnrealEditor.exe`锛堟垨瀵瑰簲鐨勫钩鍙颁簩杩涘埗鏂囦欢锛?
- 濡傛灉涓嶅瓨鍦紝鎻愮ず鐢ㄦ埛閲嶆柊杈撳叆

### 1.2 鍙戠幇椤圭洰鏂囦欢

Agent 浣跨敤鏅€氫粨搴撳伐鍏峰湪褰撳墠椤圭洰宸ヤ綔鍖哄彂鐜扮洰鏍?`.uproject` 鏂囦欢锛屾枃妗ｄ腑鐢?`<ABSOLUTE_UPROJECT_FILE>` 琛ㄧず璇ヤ竴娆℃€у伐鍏峰弬鏁般€?

涓嶈鎶婇」鐩矾寰勫啓鍏ュ叏灞€ Claude settings銆佹彃浠?env銆丼etupProfile 鎴?RuntimeProfile銆傞」鐩矾寰勫彧鍦ㄨ皟鐢?`blueprint_open_editor`銆乣blueprint_build_project` 绛夊伐鍏锋椂浣滀负鏄惧紡 `project_file` 鍙傛暟浼犲叆銆?

楠岃瘉瑙勫垯锛?
- 蹇呴』鏄粷瀵硅矾寰?
- 蹇呴』浠?`.uproject` 缁撳熬
- 鏂囦欢蹇呴』瀛樺湪
- 濡傛灉褰撳墠宸ヤ綔鍖轰笅鏃犳硶鍞竴纭畾鐩爣 `.uproject`锛屽仠姝㈠苟璇㈤棶鐢ㄦ埛锛屼笉瑕佸洖閫€鍒颁换浣曞叏灞€椤圭洰璺緞鍙橀噺

### 1.3 纭 BlueprintHelper 鎻掍欢宸插畨瑁?

妫€鏌ヤ互涓嬫潯浠讹細
- BlueprintHelper 鎻掍欢鐩綍瀛樺湪浜庨」鐩?`Plugins/` 鎴栧紩鎿?`Engine/Plugins/` 涓?
- `BlueprintHelper.uplugin` 鏂囦欢瀛樺湪涓?`VersionName` 涓?CLI `package.json` 鐗堟湰鍏煎

---

## 闃舵 2锛欳LI 鏋勫缓

### 2.1 瀹夎渚濊禆骞舵瀯寤?

鍦?`AgentFaceService/task-core/` 鍜?`AgentFaceService/cli/` 鐩綍涓嬫墽琛岋細

```powershell
cd <PLUGIN_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <PLUGIN_ROOT>\AgentFaceService\cli
npm install
npm run build
```

### 2.2 楠岃瘉 CLI 鍙繍琛?
鎵ц `bh --help`锛屾垨浠呮鏌?`AgentFaceService/cli/build/cli/index.js` 瀛樺湪銆?
---

## 闃舵 2.5锛氱‘璁?CLI 鍛戒护闈㈠彲鐢?
鍦ㄨ繘鍏?Bridge 杩為€氭€ч獙璇佸墠锛屽繀椤荤敤鍘熺敓浜や簰琛ㄥ崟纭鏈?setup 鍙互浣跨敤 BlueprintHelper CLI 鍛戒护闈€?
鏉冮檺鑼冨洿鍙寘鍚潪鍐荤粨銆侀潪搴熷純銆丄gent-facing 鎴?preflight 宸ュ叿锛?
- `blueprinthelper_read_agent_guide`
- `blueprinthelper_get_debug_case`
- `blueprint_get_runtime_profile`
- `blueprinthelper_diagnostics`
- `blueprinthelper_diagnostics_runtime`
- `blueprinthelper_request_write_session`
- `blueprinthelper_read_context`
- `blueprinthelper_read_task_context`
- `blueprinthelper_read_reference_context`
- `blueprinthelper_preview_task`
- `blueprinthelper_execute_task`
- `blueprinthelper_get_task_result`
- `blueprint_open_editor`

不要走 deprecated compatibility transport 旧入口。setup 只验证 CLI 命令面，不把 frozen / legacy / expert 入口当作普通工作流。

Use AskUserQuestion:
- header: "CLI Commands"
- question: "鏄惁纭 setup 浣跨敤 BlueprintHelper CLI 鍛戒护闈紵"
- multiSelect: false
- options:
  - label: "Allow setup CLI commands (Recommended)"
    description: "鍏佽 setup 鏋勫缓骞堕獙璇?BlueprintHelper CLI TaskSpec銆乨iagnostics銆乧ontext銆乺untime profile銆亀rite session 鍜?open_editor 鍛戒护"
  - label: "Review tool list first"
    description: "鍏堝睍绀轰笂鏂瑰伐鍏锋竻鍗曪紝鍐嶉噸鏂拌姹傚噯璁?
  - label: "Skip CLI validation"
    description: "鍙敓鎴愭湰鍦版枃浠讹紱Bridge銆乺untime_profile 鍜?diagnostics 楠岃瘉灏嗘爣璁颁负 skipped"

濡傛灉鐢ㄦ埛閫夋嫨 `Review tool list first`锛屽睍绀哄伐鍏锋竻鍗曞悗鍐嶆杈撳嚭鍚屼竴涓師鐢熺‘璁よ〃鍗曘€?
濡傛灉鐢ㄦ埛閫夋嫨 `Skip CLI validation` 鎴栨嫆缁?Claude 鏉冮檺寮圭獥锛岀户缁畬鎴愭湰鍦版枃浠剁敓鎴愶紝浣嗕笉瑕佽皟鐢?BlueprintHelper CLI 鍛戒护锛屽苟鍦ㄦ渶缁堟姤鍛婁腑鍐欐槑 `CLI verification skipped by user`銆?
---

## 闃舵 3锛欱ridge 杩為€氭€?

### 3.1 纭 Unreal Editor 鐘舵€?

妫€鏌ヤ互涓嬩箣涓€锛?
1. Unreal Editor 姝ｅ湪杩愯涓斿凡鍔犺浇 BlueprintHelper 鎻掍欢
2. 濡傛灉 Editor 鏈繍琛岋紝纭 `open_editor` 宸ュ叿鍙敤锛堜緷璧栭」鐩?agent-profile 鐨?`environment.ue_engine_dir` 鍜屾樉寮?`project_file` 鍙傛暟锛?

### 3.2 楠岃瘉 Bridge 杩炴帴

Bridge 榛樿鍦板潃 `127.0.0.1:54321`銆?

濡傛灉鍙互璋冪敤 CLI 鍛戒护锛屼娇鐢?`bh blueprinthelper_diagnostics --json "{}"` 妫€鏌ワ細
- `bridge_status` 搴斾负 `connected`
- 濡傛灉涓嶆槸锛屾姤鍛婇樆鏂師鍥犲苟璁╃敤鎴锋帓鏌?

---

## 闃舵 4锛歎ser Preference Wizard

**FIRST**锛氫娇鐢?Read 璇诲彇 `ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md`銆?

濡傛灉鏂囦欢宸插瓨鍦紝涓嶈鍦?setup 涓繍琛屽亸濂芥洿鏂版祦绋嬨€傚悜鐢ㄦ埛灞曠ず褰撳墠鍋忓ソ鎽樿锛屽苟璇存槑鏇存柊宸叉湁鍋忓ソ鍜?safety profile 璇疯繍琛?`/blueprint-helper:configure`銆傚鏋滅敤鎴锋槑纭姹傞噸鏂板垵濮嬪寲鍋忓ソ锛屽彲缁х画鎵ц涓嬮潰鐨勬柊鐢ㄦ埛 Flow A 骞惰鐩栬鏂囦欢銆?

杩欎簺鍋忓ソ鐢熸垚鐙珛鐨勭敤鎴峰亸濂芥枃浠讹紝涓嶅啓鍏?SetupProfile銆丷untimeProfile 鎴栭」鐩?Marker銆係etupProfile 鍙繚瀛樻満鍣ㄥ彲鎵ц閰嶇疆锛涚敤鎴峰亸濂芥枃浠朵繚瀛?Agent 琛屼负銆佸崗浣滃拰鏂囨。璇诲彇绾﹀畾銆?

### 鍘熺敓浜や簰琛ㄥ崟瑙勫垯

鎵€鏈夊亸濂介噰闆嗛棶棰橀兘蹇呴』璋冪敤 `AskUserQuestion` 杈撳嚭鍘熺敓浜や簰琛ㄥ崟銆備笉瑕佹妸涓嬮潰鐨?YAML 鎴栭€夐」鍒楄〃褰撲綔鏅€?Markdown 鎵撳嵃缁欑敤鎴凤紝闄ら潪褰撳墠 Claude 鐜娌℃湁 `AskUserQuestion` 宸ュ叿銆?

姣忎釜 Q 閮芥槸涓€涓畬鏁寸殑 `AskUserQuestion` payload锛?

```yaml
Use AskUserQuestion:
  header: "鐭爣棰?
  question: "缁欑敤鎴风湅鐨勯棶棰?
  multiSelect: false
  options:
    - label: "閫夐」鍚?(Recommended)"
      description: "褰卞搷璇存槑"
```

瑙勫垯锛?
- 鐢熸垚瀹屾暣 `AskUserQuestion` 琛ㄥ崟
- 鎺ㄨ崘椤瑰繀椤绘爣娉?`(Recommended)`銆?
- `multiSelect: true` 鍙敤浜庡彲鍙犲姞鍋忓ソ銆?
- 鐢ㄦ埛閫夋嫨鑷畾涔夋枃鏈椂锛屽啀鐢?AskUserQuestion 鑾峰彇鑷敱鏂囨湰銆?
- 鐢ㄦ埛鍙栨秷鏃讹紝杈撳嚭 `Setup cancelled.`锛屼笉瑕佸啓鏂囦欢銆?
- 濡傛灉 `AskUserQuestion` 涓嶅彲鐢紝鎵嶉€€鍥?Markdown 鏂囨湰琛ㄥ崟锛屽苟鏄庣‘璇存槑褰撳墠鐜涓嶆敮鎸佸師鐢熻〃鍗曘€?

### Flow A锛氭柊鐢ㄦ埛

闂椤哄簭锛歋afety 鈫?Task Flow 鈫?Save/Validation 鈫?Boundary 鈫?Graph/Naming 鈫?Input/Asset 鈫?Review/Debug 鈫?Collaboration銆?

#### Q1锛歋afety

Use AskUserQuestion:
- header: "Safety"
- question: "閫夋嫨 BlueprintHelper 鍐欏叆瀹夊叏妗ｄ綅锛?
- multiSelect: false
- options:
  - label: "Conservative (Recommended)"
    description: "鍏佽鍐欏叆锛屼絾蹇呴』 preview + 鐢ㄦ埛纭锛屼笉鑷姩 save"
  - label: "ReadOnly"
    description: "鍙妯″紡锛屾嫆缁濇墍鏈夊啓鍏?
  - label: "Standard"
    description: "鍏佽鍐欏叆锛屽繀椤?preview锛屽彲鎸夎姹?save"
  - label: "AutoRepair"
    description: "鍏佽鑷姩淇 BlueprintHelper-owned 鍐呭"
  - label: "Expert"
    description: "鍏佽鏇村皯纭鍜屾洿楂橀闄╄兘鍔?

#### Q2锛歍ask Flow

Use AskUserQuestion:
- header: "Task Flow"
- question: "TaskSpec-first 涓嶅彲鐢ㄦ垨鑳藉姏缂哄け鏃舵€庝箞澶勭悊锛?
- multiSelect: false
- options:
  - label: "Stop and report (Recommended)"
    description: "鍋滄骞惰鏄庣己鍙ｏ紝涓嶅洖閫€鍒板簳灞傚喕缁撳叆鍙?
  - label: "Ask user"
    description: "鍏堣闂敤鎴锋槸鍚﹁皟鏁寸洰鏍囨垨鎺堟潈鏇夸唬璺緞"
  - label: "Debug tools only"
    description: "鍏佽浣跨敤璇婃柇/debug 宸ュ叿瀹氫綅闂"
  - label: "Legacy direct allowed"
    description: "鍏佽鐩存帴璋冪敤搴曞眰閬楃暀鍛戒护闈?

#### Q3锛歋ave/Validation

Use AskUserQuestion:
- header: "Save"
- question: "淇濆瓨鍜岄獙璇佺瓥鐣ワ細"
- multiSelect: false
- options:
  - label: "Preview + no auto save (Recommended)"
    description: "preview 鏄啓鍏ラ棬绂侊紝榛樿涓嶈嚜鍔?save"
  - label: "Save when requested"
    description: "鐢ㄦ埛鏄庣‘瑕佹眰鏃跺彲 save"
  - label: "Workflow save"
    description: "閫氳繃楠岃瘉鍚庡彲鐢卞伐浣滄祦 save"

#### Q4锛欱oundary

Use AskUserQuestion:
- header: "Boundary"
- question: "Agent 鍙互瑙︾鍝簺宸ョ▼杈圭晫锛?
- multiSelect: true
- options:
  - label: "UE assets through CLI (Recommended)"
    description: "BlueprintHelper CLI 鍙鐞?UE 缂栬緫鍣ㄨ祫浜?
  - label: "Repo files through normal tools (Recommended)"
    description: "C++銆乀S銆丳ython銆丣SON銆佹枃妗ｇ敤鏅€氫粨搴撳伐鍏?
  - label: "No C++ edits by default (Recommended)"
    description: "榛樿涓嶄慨鏀?C++ 婧愮爜"
  - label: "No reparent by default (Recommended)"
    description: "Parent Class 淇敼涓嶆敮鎸佹椂 stop_and_report"
  - label: "No active tab writes (Recommended)"
    description: "涓嶄緷璧栧綋鍓嶈仛鐒︾紪杈戝櫒鏍囩鎵ц鐮村潖鎬ф搷浣?

#### Q5锛欸raph/Naming

Use AskUserQuestion:
- header: "Graph"
- question: "Graph Write 鍜屽懡鍚嶅亸濂斤細"
- multiSelect: true
- options:
  - label: "EG_{FeatureName} graphs (Recommended)"
    description: "鏂?EventGraph 榛樿浣跨敤 EG_{FeatureName}"
  - label: "Descriptive PascalCase (Recommended)"
    description: "鍑芥暟鍜?Custom Event 浣跨敤鎻忚堪鍨?PascalCase"
  - label: "UE variable style (Recommended)"
    description: "鍙橀噺浣跨敤 bDoorOpen銆丱penImpulse 绛?UE 甯歌椋庢牸"
  - label: "Do not modify user nodes (Recommended)"
    description: "榛樿涓嶄慨鏀圭敤鎴峰凡鏈夎妭鐐?
  - label: "Do not merge existing exec flow (Recommended)"
    description: "榛樿涓嶆帴鍏ョ敤鎴峰凡鏈夋墽琛屾祦"
  - label: "Reject generic names (Recommended)"
    description: "绂佹 NewFunction銆丏oThing銆乀emp銆丮yVar"

#### Q6锛歊eview/Debug

Use AskUserQuestion:
- header: "Review"
- question: "Review銆佸洖婊氬拰 Debug 璇佹嵁鍋忓ソ锛?
- multiSelect: true
- options:
  - label: "Enable Journal (Recommended)"
    description: "鍚敤 Transaction Journal"
  - label: "Enable Review Store (Recommended)"
    description: "鍚敤 Review Store"
  - label: "Keep rollback until accepted (Recommended)"
    description: "Pending 淇濈暀瀹屾暣鍥炴粴鏁版嵁锛屾帴鍙楀悗鍙帇缂?
  - label: "Summary-only CLI debug (Recommended)"
    description: "CLI 鍙煡 DebugCase 鎽樿锛屼笉璇诲彇 DebugBundle artifact 鍐呭"
  - label: "DebugBundle local export shape (Recommended)"
    description: "UE/鏈湴瀵煎嚭淇濇寔 summary.md + artifacts/"

#### Q7锛欳ollaboration

Use AskUserQuestion:
- header: "Workflow"
- question: "Agent 鍗忎綔鍜屽洖澶嶅亸濂斤細"
- multiSelect: true
- options:
  - label: "Read AGENTS first (Recommended)"
    description: "浠诲姟寮€濮嬪厛璇讳粨搴?AGENTS.md 鎴栧綋鍓嶇瓑浠疯鍒?
  - label: "Prefer concise Chinese (Recommended)"
    description: "鐢ㄦ埛鏈姹傛椂鍑忓皯鎷彿璇存槑锛屽洖澶嶇洿鎺?
  - label: "Use parallel workers (Recommended)"
    description: "鏈€澶х▼搴﹀苟鍙戝鐞嗙嫭绔嬭妗ｃ€乨iff銆佹祴璇曞拰瀹炵幇浠诲姟"
  - label: "Precise completion claims (Recommended)"
    description: "宸ヤ綔鍖鸿剰鎴栨湭楠岃瘉鏃朵笉璇村畬鍏ㄥ畬鎴?
  - label: "Write compaction memory (Recommended)"
    description: "涓婁笅鏂囨帴杩戜笂闄愭椂鍐欏叆 .codex/memory 杩涘害鏂囨。"

### 鏇存柊宸叉湁鍋忓ソ

宸叉湁鐢ㄦ埛鍋忓ソ鍜?safety profile 鐨勬洿鏂版祦绋嬪凡杩佺Щ鍒?`ClaudePlugin/commands/configure.md`銆?

浣跨敤鏂瑰紡锛?

```text
/blueprint-helper:configure
```

Setup 鍙礋璐ｉ娆￠厤缃€傚鏋滅敤鎴峰湪 setup 涓姹備慨鏀瑰凡鏈夊亸濂斤紝鍋滄褰撳墠鍋忓ソ閲囬泦骞惰浆浜?configure 鍛戒护锛屼笉瑕佸湪 setup 涓鍒舵洿鏂版祦绋嬨€?

### 澶勭悊閫昏緫

鏂扮敤鎴凤細
1. 浠?Conservative 榛樿闆嗗紑濮嬨€?
2. 搴旂敤姣忎釜琛ㄥ崟閫夋嫨銆?
3. 鐢熸垚鐢ㄦ埛鍋忓ソ鏂囦欢棰勮銆?

### 鍐欏叆鍓嶉瑙?

鍐欏叆鍓嶅繀椤诲睍绀猴細

```text
User Preferences Preview

Safety: Conservative
Task Flow: TaskSpec-first, stop_and_report
Boundary: UE assets via CLI; repo files via normal tools; no C++ by default
Debug: CLI summary-only; local DebugBundle summary.md + artifacts/
Collaboration: AGENTS first; parallel workers; precise completion claims

Write to:
ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md
```

鐒跺悗鐢ㄥ師鐢熺‘璁よ〃鍗曡闂細

Use AskUserQuestion:
- header: "Save"
- question: "Save these preferences?"
- multiSelect: false
- options:
  - label: "Save (Recommended)"
    description: "鍐欏叆鐢ㄦ埛鍋忓ソ鏂囦欢"
  - label: "Cancel"
    description: "涓嶅啓鍏ユ枃浠跺苟閫€鍑?setup"

濡傛灉鐢ㄦ埛纭锛屽啓鍏?`ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md`銆傚鏋滄棤鍙樺寲锛岃緭鍑?`No changes needed - user preferences unchanged.`

### 鐢ㄦ埛鍋忓ソ鏂囦欢杈撳嚭鏍煎紡

蹇呴』鍐欐垚 Markdown锛屼娇鐢ㄥ浐瀹氭爣棰橈細

```markdown
# 08 - BlueprintHelper User Preferences
```

蹇呴』鍖呭惈锛?
- `schema: BlueprintHelper.UserPreferences.v1`
- `generated_by: ClaudePlugin/commands/setup.md`
- `saved_at`
- `source: setup_user_preference_wizard`
- `## Purpose`
- `## Active Preferences`
- `## SetupProfile Separation`
- `## Collaboration Preferences`
- `## Debug And Review Preferences`
- `## Manual Notes`

---

## 闃舵 5锛氱敓鎴?SetupProfile

鍩轰簬鐢ㄦ埛绛旀鏋勯€?`BlueprintHelper.SetupProfile.v1` JSON锛屽啓鍏ラ」鐩厤缃洰褰曘€?

鎺ㄨ崘鐨勪繚瀛樿矾寰勶細`<ProjectDir>/.blueprinthelper/agent-profile.json`

绀轰緥缁撴瀯鍙傝 `Resources/Docs/Setup/SetupProfile_Example.json`銆?

娉ㄦ剰锛氫笉瑕佹妸 `08_User_Preferences.md` 鐨勯暱鏂囨湰鍋忓ソ鍐欏叆 SetupProfile銆係etupProfile 鍙繚瀛樺畨鍏ㄦ。浣嶃€乫allback銆佽嚜鍔ㄤ繚瀛樸€佽竟鐣岀瓑鍙墽琛岄厤缃憳瑕侊紝浠ュ強褰撳墠椤圭洰鐨?`environment.ue_engine_dir` 鍜屽彲閫?`environment.ue_version`銆備笉瑕佹妸椤圭洰 `.uproject` 璺緞銆乣project_file` 鎴栨棫鐨勫叏灞€椤圭洰璺緞瀛楁鍐欏叆 SetupProfile銆?

---

## 闃舵 5.5锛氶」鐩?Agent Profile 棰勬

鍦ㄨ繘鍏ラ獙璇侀樁娈靛墠锛屼娇鐢?Read 璇诲彇 `<ProjectDir>/.blueprinthelper/agent-profile.json`銆?

妫€鏌ヨ鍒欙細
- `environment.ue_engine_dir` 搴斿瓨鍦紝骞舵寚鍚戞湁鏁?UE Engine 鐩綍
- `environment.ue_version` 鍙€夛紝浣嗗鐗堟湰寮€鍙戦」鐩缓璁啓鍏ワ紝渚嬪 `5.6`
- 涓嶈鍐欏叆鎴栨洿鏂板叏灞€ Claude settings 鐨勬棫 UE 寮曟搸璺緞鎴栨棫椤圭洰璺緞瀛楁
- 濡傛灉鍙戠幇鍏ㄥ眬 settings 涓凡鏈夋棫 UE 寮曟搸璺緞鎴栨棫椤圭洰璺緞瀛楁锛屾姤鍛婂畠浠凡琚?BlueprintHelper 椤圭洰 profile 娴佺▼寮冪敤锛涘彲寤鸿鐢ㄦ埛鎵嬪姩娓呯悊锛屼絾 setup 涓嶈嚜鍔ㄤ慨鏀瑰叏灞€ settings
- 椤圭洰 `.uproject` 璺緞缁х画鐢?Agent 浠庡綋鍓嶅伐浣滃尯鍙戠幇锛屽苟鍦ㄥ伐鍏疯皟鐢ㄦ椂鏄惧紡浼犲叆 `project_file`

---

## 闃舵 6锛氶獙璇?

### 6.1 runtime_profile 鍙

璋冪敤 `bh blueprint_get_runtime_profile --json "{}"`锛岄獙璇侊細
- `active_profile.safety_profile` 涓庣敤鎴烽€夋嫨涓€鑷?
- `bridge_status` 涓?`connected`
- `config_status` 涓?`valid`

### 6.2 diagnostics 閫氳繃

璋冪敤 `bh blueprinthelper_diagnostics --json "{}"`锛岀‘璁わ細
- 鏃?Blocking 椤?
- Warning 椤瑰凡鐭ヤ笖鍙帴鍙?
- Info 椤规樉绀洪摼璺畬鏁?

### 6.3 鐢熸垚椤圭洰 Marker锛堝彲閫夛級

濡傛灉椤圭洰鏍圭洰褰曞瓨鍦?`CLAUDE.md` 鎴?`AGENTS.md`锛岃闂槸鍚﹂渶瑕佹坊鍔?BlueprintHelper 寮曠敤鎸囬拡(濡傛灉娣诲姞锛屽厛娓呴櫎鏂囨。鍙鐘舵€?锛?

```markdown
## BlueprintHelper

鏈」鐩娇鐢?BlueprintHelper 杩涜 UE 缂栬緫鍣ㄨ祫浜ф搷浣溿€侫gent 璇烽伒寰?skill `blueprint-helper` 鐨?TaskSpec-first 娴佺▼銆?
SetupProfile: <ProjectDir>/.blueprinthelper/agent-profile.json
UserPreferences: ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md
```

---

## 闃舵 7锛氭姤鍛?

Setup 瀹屾垚鍚庤緭鍑烘憳瑕侊細

```text
BlueprintHelper Setup 瀹屾垚

UE Engine:  <UE_ENGINE_ROOT> (from agent-profile environment.ue_engine_dir)
UE Project: <discovered .uproject used as project_file only>
Bridge:     <host:port> 鈥?<status>
CLI Commands: <validated command surface / skipped by user>
Safety:     <safety_profile>
Entry Mode: task_spec_first
Fallback:   <fallback_policy>

SetupProfile 宸蹭繚瀛樿嚦: <path>
UserPreferences 宸蹭繚瀛樿嚦: ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md
```

濡傛灉浠讳綍闃舵琚樆鏂紝鍋滄骞舵姤鍛婂叿浣撻樆鏂師鍥狅紝涓嶈璺宠繃銆?



