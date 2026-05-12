# 05 Validation / Diagnostics Tools 璁捐鏂囨。锛堝凡鍚屾纭 Diff锛?

鏃ユ湡锛?026-05-03  
宸ュ叿绨囷細Validation / Diagnostics Tools / 楠岃瘉涓庤瘖鏂伐鍏风皣  
鐘舵€侊細鍚屾纭 Diff 鍚庣殑淇鐗? 
鍚屾鑼冨洿锛欴iagnostics Markdown 杩斿洖銆乀oolResultBase 瑙ｉ噴銆乺untime diagnostics 杈圭晫銆佸啓鍚?validation 瀛楁鏀舵暃銆?

---

## 0. 鏈鍚屾缁撹

鏈枃浠舵浛鎹㈡棫鐗堜腑浠ヤ笅杩囨湡鍙ｅ緞锛?

```text
1. /blueprinthelper-diagnostics 涓?/blueprinthelper-diagnostics --runtime 杩斿洖 ToolResultBase 澶栧３锛屽疄闄呰瘖鏂姤鍛婂湪 data.markdown銆?
2. Diagnostics 涓嶈繑鍥?blocking / warning / info JSON 鏁扮粍銆?
3. Diagnostics Markdown 蹇呴』鍖呭惈 ## Blocking 鍜?## Warning锛?# Info 鍙€夈€?
4. Diagnostics 宸ュ叿鎵ц鎴愬姛鏃讹紝鍗充娇 Markdown 涓湁 Blocking锛屼篃搴?ok=true銆乻tatus=completed銆?
5. Markdown Blocking 琛ㄧず璇婃柇鍙戠幇鐜鎴栬繍琛岄摼璺樆鏂」锛屼笉绛変簬 CLI 鍛戒护璋冪敤澶辫触銆?6. 鍙湁 ok=false / status=failed 琛ㄧず diagnostics 宸ュ叿鑷韩澶辫触銆?
7. 鏅€氬啓宸ュ叿鐨?validation 浠嶅彲杩斿洖 should_compile / should_save锛屼絾涓嶄唬琛ㄦ墍鏈夊伐鍏烽兘榛樿杩斿洖 should_validate / recommended_next_tool銆?
```

---

## 1. 鍥涗釜姒傚康杈圭晫

```text
Graph Diagnostics = 褰撳墠钃濆浘 / 鍥捐〃鐘舵€佷綋妫€銆?
preflight = 鍐欏伐鍏峰唴閮ㄥ己鍒跺啓鍏ュ墠瀹夊叏妫€鏌ャ€?
dry_run = 鍐欏伐鍏烽潪鍐欏叆棰勬紨妯″紡銆?
Review = 鍐欏叆鍚庣殑 transaction 瀹℃煡鍜屽洖婊氬叆鍙ｃ€?
```

鍥涜€呬笉鑳戒簰鐩告浛浠ｃ€?

| 姒傚康 | 鍙戠敓鏃堕棿 | 鏄惁鍐欒祫浜?| 鐢ㄩ€?|
|---|---:|---:|---|
| Diagnostics | 鍐欏叆鍓?/ 鍐欏叆鍚?/ 鍗曠嫭娴嬭瘯 | 鍚?| 妫€鏌ュ畨瑁呫€侀厤缃€丅ridge銆乺untime 鎴栬祫浜?/ 鍥捐〃鐘舵€?|
| preflight | 鍐欏叆鍓?| 鍚?| 鍒ゆ柇鏈鍐欐搷浣滄槸鍚﹀畨鍏?|
| dry_run | 鍐欏叆鍓?| 鍚?| 瀹屾暣棰勬紨宸ュ叿璋冪敤 |
| Review | 鍐欏叆鍚?| 宸插啓鍏?| 瀹℃煡鐪熷疄鏀瑰姩锛孉ccept / Reject / rollback |

---

## 2. Diagnostics 鍒嗗眰

Diagnostics 鍒嗕负涓ょ被锛?

```text
瀹夎 / 閰嶇疆 / Bridge / runtime 閾捐矾璇婃柇
钃濆浘 / 鍥捐〃 / 璧勪骇鐘舵€佽瘖鏂?
```

鍛戒护锛?

```text
/blueprinthelper-diagnostics
/blueprinthelper-diagnostics --runtime
```

宸ュ叿锛?

```text
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
```

钃濆浘璧勪骇绾ц瘖鏂彲缁х画瑙勫垝涓鸿兘鍔涜寖鍥达紝涓嶄綔涓烘櫘閫?Agent 鐩磋皟宸ュ叿娓呭崟锛?

```text
Graph diagnostics
Asset diagnostics
Project diagnostics锛堝悗缃級
```

---

## 3. Diagnostics 杩斿洖浣?

Diagnostics 浣跨敤 ToolResultBase 澶栧３锛?

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "diagnostics_runtime",
  "trace_id": "trace_20260503_0001",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "BlueprintHelper.Diagnostics.v1",
    "mode": "runtime",
    "format": "markdown",
    "markdown": "## Blocking
None

## Warning
None

## Info
- bridge.connected"
  }
}
```

Agent 搴旂悊瑙ｏ細

```text
椤跺眰 schema = 瑙勮寖鍖栧伐鍏风粨鏋滃熀纭€鍗忚鐗堟湰銆?data.schema = Diagnostics 鏁版嵁缁撴瀯鐗堟湰銆?
data.markdown = 瀹為檯璇婃柇鎶ュ憡銆?
```

---

## 4. Markdown 瑙ｉ噴瑙勫垯

Diagnostics 鍙繑鍥?Markdown锛屼笉棰濆杩斿洖 JSON 鏁扮粍銆?

鍥哄畾缁撴瀯锛?

```md
## Blocking
None

## Warning
None

## Info
- code
```

瑙勫垯锛?

```text
1. Blocking 琛ㄧず璇婃柇鍙戠幇闃绘柇椤广€?
2. Warning 琛ㄧず椋庨櫓銆佸彈闄愭垨闈為樆鏂棶棰樸€?
3. Info 琛ㄧず褰撳墠宸茬‘璁ょ殑姝ｅ父鐘舵€併€?
4. Blocking 鍜?Warning 蹇呴』鍑虹幇銆?
5. Info 鍙€夈€?
```

Diagnostics 涓嶈繑鍥烇細

```text
blocking[]
warning[]
info[]
structured_issues
summary_md
```

杩欎簺鍙互浣滀负鍚庣画 graph diagnostics / asset diagnostics 鐨勬墿灞曪紝浣嗕笉閫傜敤浜庡綋鍓嶅畨瑁?/ runtime diagnostics 鍛戒护銆?

---

## 5. ok/status 瑙ｉ噴瑙勫垯

濡傛灉璇婃柇鍛戒护鎵ц鎴愬姛锛屽嵆浣垮彂鐜?Blocking锛屼篃搴旀槸锛?

```json
{
  "ok": true,
  "status": "completed"
}
```

杩欒〃绀猴細

```text
璇婃柇宸ュ叿杩愯鎴愬姛銆?
璇婃柇缁撴灉涓彂鐜扮殑闂鍐欏湪 data.markdown銆?
```

鍙湁 diagnostics 宸ュ叿鑷韩澶辫触鏃讹紝鎵嶆槸锛?

```json
{
  "ok": false,
  "status": "failed",
  "error": {}
}
```

Agent 涓嶅緱鎶?Markdown 涓殑 Blocking 璇垽涓哄伐鍏疯皟鐢ㄥけ璐ャ€?

---

## 6. Static Diagnostics

Static Diagnostics 鐢ㄤ簬瀹夎涓庨厤缃潤鎬佹鏌ワ紝涓嶈姹?UE Editor 姝ｅ湪杩愯銆?

鍏稿瀷 code锛?

```text
version.match
version.mismatch
settings.valid
settings.unavailable
global_guidance.present
global_guidance.missing
skill_entry.valid
skill_entry.invalid
project_marker.present
project_marker.missing
```

澶勭悊瑙勫垯锛?

```text
1. settings.unavailable 鍑虹幇鍦?Blocking 鏃讹紝涓嶈兘缁х画鍐欎换鍔°€?
2. global_guidance.missing 鎴?skill_entry.invalid 鍑虹幇鏃讹紝搴旀彁绀虹敤鎴疯繍琛?/blueprinthelper-setup 鎴栨鏌ュ畨瑁呫€?
3. project_marker.missing 閫氬父鏄?Warning锛屼笉鑷姩鍐欏叆椤圭洰 CLAUDE.md锛涘彧鏈夌敤鎴风‘璁ゅ悗鎵嶈兘鍐?Project Marker銆?
4. version.mismatch 鏄?Blocking锛涗笉鑳界户缁?setup 鎴栧啓鍏ャ€?
```

---

## 7. Runtime Diagnostics

Runtime Diagnostics 鐢ㄤ簬 UE / CLI / Bridge / runtime profile 閾捐矾妫€娴嬨€?

鍏稿瀷 code锛?

```text
ue_editor.running
ue_editor.not_running
mcp_server.available
mcp_server.unavailable
bridge.connected
bridge.disconnected
runtime_profile.available
runtime_profile.unavailable
config_status.valid
config_status.unavailable
write_permission.enabled
write_permission.disabled
risk_command.enabled
risk_command.disabled
```

澶勭悊瑙勫垯锛?

```text
1. bridge.disconnected 鍑虹幇鍦?Blocking 鏃讹紝涓嶅緱璋冪敤 UE 鍐欏伐鍏枫€?
2. runtime_profile.unavailable 鍑虹幇鍦?Blocking 鏃讹紝涓嶅緱杩涘叆鍐欏叆闃舵銆?
3. config_status.unavailable 鍑虹幇鏃讹紝搴?stop_and_report锛屽苟鎻愮ず鍏抽棴 UE 鍚庤繍琛?setup銆?
4. write_permission.disabled 瀵瑰彧璇讳换鍔′笉闃绘柇锛涘鍐欎换鍔￠樆鏂€?
5. risk_command.disabled 鍙樆鏂?close_editor / exec_console_command 绛夐珮椋庨櫓鍛戒护锛屼笉闃绘柇鏅€氳摑鍥捐鍐欍€?
```

---

## 8. Diagnostics 涓?runtime_profile 杈圭晫

runtime_profile锛?

```text
褰撳墠杩愯鏃朵簨瀹炴憳瑕侊紝渚涗换鍔″墠鍒ゆ柇銆?
```

Diagnostics锛?

```text
鍙璇婃柇鎶ュ憡锛岀敤浜庡畾浣嶅畨瑁呫€侀厤缃€丅ridge銆乺untime 閾捐矾闂銆?
```

Diagnostics 涓嶅緱鏇夸唬 runtime_profile銆?

Agent 涓嶅緱杩欐牱鍋氾細

```text
runtime_profile 鏄剧ず write_permission.disabled
鈫?diagnostics 娌℃湁鎶ラ敊
鈫?Agent 缁х画鍐?
```

姝ｇ‘瑙勫垯锛?

```text
runtime_profile 鏄换鍔″墠杩愯鏃朵簨瀹炴潵婧愩€?
diagnostics 鍙敤浜庡畾浣嶉棶棰樸€?
diagnostics 涓嶈兘瑕嗙洊 runtime_profile 鐨勫啓鏉冮檺銆佸畨鍏ㄦ。浣嶆垨鑳藉姏缂哄け鍒ゆ柇銆?
```

---

## 9. Graph / Asset Diagnostics 鍚庣画杈圭晫

钃濆浘鍥捐〃璇婃柇鍙户缁鏌ワ細

```text
duplicate_events
duplicate_custom_events
orphan_nodes
unreachable_nodes
unconnected_exec_flow
empty_function_body
dangling_links
invalid_pin_links
required_pin_missing
owned_block_integrity
event_entry_integrity
```

璧勪骇璇婃柇鍙眹鎬伙細

```text
graph diagnostics
component tree diagnostics
class settings diagnostics
compile status
dirty / save state
ownership metadata integrity
transaction journal consistency
```

浣嗚繖浜涜摑鍥剧骇 diagnostics 浠嶇劧鍙锛屼笉鎻愪緵锛?

```text
auto_fix=true
```

AutoRepair 鍙互璇诲彇 Diagnostics 缁撴灉鍚庡彟琛岃皟鐢ㄥ啓宸ュ叿锛屼絾 Diagnostics 鏈韩涓嶇洿鎺ヤ慨澶嶃€?

---

## 10. compile / save / PIE

compile / save / PIE 涓嶉噸澶嶉€犳柊宸ュ叿銆?

鐜版湁缂栬瘧銆佷繚瀛樸€丳IE 鍚仠銆佺紪杈戝櫒鍛戒护鑳藉姏淇濇寔鍘熸湁瀹炵幇褰掑睘锛屼絾鏅€?Agent 宸ヤ綔娴佸彧閫氳繃 TaskSpec validation銆乸review銆乪xecute銆乼ask result 鍜?read-back 琛ㄨ揪闂幆銆?

Validation Workflow 鍙寘鍚繖浜涜兘鍔涢樁娈碉細

```text
graph diagnostics
asset diagnostics
compile
save
PIE smoke锛堝悗缁彲閫夛級
```

Save 鏄惤鐩樺姩浣滐紝搴旈伒瀹?Safety Profile 涓庣敤鎴锋巿鏉冭鍒欍€?

---

## 11. 鍐欏悗 validation 瀛楁

鏅€氬啓宸ュ叿鍙繑鍥烇細

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

瀛楁瑙勫垯锛?

```text
1. should_compile / should_save 鏄?Agent 鍚庣画闂幆鎻愮ず銆?
2. compiled / saved 琛ㄧず鏈伐鍏疯皟鐢ㄤ腑鏄惁宸茬粡鎵ц銆?
3. no_op 涓?modified=false 鏃讹紝閫氬父 should_compile=false / should_save=false銆?
4. 鑷姩 diagnostics / compile / save 鐢?Safety Profile 鍜?workflow 鍙傛暟鍐冲畾銆?
```

涓嶅啀寮哄埗鎵€鏈夊啓宸ュ叿榛樿杩斿洖锛?

```text
should_validate
recommended_next_tool
recommended_validation_workflow
```

杩欎簺瀛楁鍙互鍦ㄩ渶瑕佸鏉傚伐浣滄祦鐨勫伐鍏蜂腑鎸夐渶杩斿洖锛屼絾涓嶆槸鏅€氬伐鍏风殑缁熶竴瀛楁涔夊姟銆?

---

## 12. 涓ラ噸搴﹀垎绾?

Graph / Asset Diagnostics 缁撴灉鍙噰鐢ㄥ洓妗ｏ細

```text
blocker
error
warning
info
```

璇箟锛?

```text
blocker锛氬綋鍓嶆搷浣滃繀椤诲仠姝€?
error锛氳祫浜ф垨鍥捐〃瀛樺湪鏄庣‘閿欒锛岄€氬父闇€瑕佷慨澶嶏紱鏄惁闃绘柇鍙栧喅浜庢搷浣滃拰 Profile銆?
warning锛氶闄╂垨娼滃湪闂锛屼笉涓€瀹氶樆鏂€?
info锛氳鏄庢€т俊鎭€?
```

瀹夎 / runtime diagnostics 鐨?Markdown 涓垯鐢?`## Blocking / ## Warning / ## Info` 鍒嗗尯琛ㄨ揪銆?

---

## 13. Agent 鎶ュ憡瑙勫垯

姝ｅ父浠诲姟瀹屾垚鏃讹紝涓嶉渶瑕佹姤鍛?diagnostics 鍐呭銆?

鍙湁浠ヤ笅鎯呭喌闇€瑕佹姤鍛婏細

```text
1. 鐢ㄦ埛鏄庣‘瑕佹眰璇婃柇銆?
2. diagnostics 鍙戠幇 Blocking銆?
3. diagnostics 鍙戠幇褰卞搷褰撳墠浠诲姟鐨?Warning銆?
4. runtime_profile 寮傚父鍚庤皟鐢?diagnostics 瀹氫綅鍘熷洜銆?
5. Agent stop_and_report 闇€瑕佽鏄庨樆鏂潵婧愩€?
```

鎶ュ憡鏃跺簲鍙浆杩扮浉鍏?code 鍜屽惈涔夛紝涓嶅睍寮€瀹屾暣 Markdown銆?

---

## 14. 楠屾敹鏍囧噯

```text
1. Agent 鑳藉尯鍒?runtime_profile 涓?diagnostics銆?
2. Agent 涓嶇敤 diagnostics 鏇夸唬 runtime_profile銆?
3. Diagnostics 杩斿洖 data.markdown銆?
4. Diagnostics 涓嶈繑鍥?blocking / warning / info JSON 鏁扮粍銆?
5. Diagnostics Markdown 蹇呴』鍖呭惈 ## Blocking 鍜?## Warning銆?
6. Diagnostics 鎵ц鎴愬姛浣嗗彂鐜?Blocking 鏃朵粛 ok=true/status=completed銆?
7. 鍙湁 ok=false/status=failed 鎵嶆槸 diagnostics 宸ュ叿鑷韩澶辫触銆?
8. Agent 鍙湪蹇呰鏃跺悜鐢ㄦ埛鎶ュ憡 diagnostics銆?
9. 鏅€氬啓宸ュ叿 validation 鍙彧杩斿洖 should_compile / should_save / compiled / saved銆?
10. Diagnostics 姘歌繙鍙锛屼笉鎵ц淇銆?
```
---

# 2026-05-04 Task Context / Preview / Execute 鍚屾

## 鍚屾缁撹

Validation / Diagnostics 涓嶆浛浠?TaskSpec preview銆傛柊澧炰笁涓换鍔＄骇姒傚康锛?

```text
TaskContextPack锛氱粰 Agent 鐢熸垚 TaskSpec 鍓嶄娇鐢ㄣ€?
preview_task锛氭牎楠?TaskSpec / TaskPlan / dry_run锛屼笉鍐欒祫浜с€?
execute_task锛氭墽琛屽凡閫氳繃 preview 鐨?TaskPlan銆?
```

## read_task_context

`blueprinthelper_read_task_context` 杩斿洖鍘嬬缉涓婁笅鏂囷紝涓嶈繑鍥炲畬鏁?RawJson 鎴栧法閲?LogicJson銆?

鏈€灏忚繑鍥烇細

```json
{
  "schema": "BlueprintHelper.TaskContextPack.v1",
  "context_id": "ctx_20260504_0001",
  "runtime": {
    "write_permission": "enabled",
    "safety_profile": "Conservative",
    "missing_capability_policy": "stop_and_report"
  },
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "exists": true,
    "asset_type": "blueprint"
  },
  "blueprint_summary": {
    "components": [],
    "graphs": [],
    "implemented_interfaces": [],
    "variables": []
  },
  "resource_candidates": {},
  "recommended_constraints": {}
}
```

## preview_task

preview_task 涓嶅啓璧勪骇銆傚畠鍙互杩斿洖锛?

```text
status=completed / preview_blocked / context_required / context_stale / failed
```

瑙勫垯锛?

```text
TaskSpec schema/semantic 閿欒锛歰k=false,status=failed,error.issues銆?
TaskSpec 鍚堟硶浣嗛瑙堥樆鏂細ok=true,status=preview_blocked,data.preview銆?
闇€瑕佹洿澶氫笂涓嬫枃锛歰k=true,status=context_required,data.preview.issues[].context_query銆?
```

2026-05-07 鍚屾瑙勫垯锛?

```text
1. preview_task 鏄?S1-S3 鍐欏叆闂ㄧ銆?
2. preview_blocked / context_required / context_stale / failed 鏃朵笉寰?execute銆?
3. GraphWrite branch_fork + owned_block_call 蹇呴』鍦?preview 闃舵瑙ｆ瀽 inserted.block_id銆?
4. inserted.block_id 蹇呴』鎸囧悜宸叉湁 BlueprintHelper-owned CustomEvent block锛涚己澶便€侀潪 owned銆侀潪 CustomEvent 閮芥槸 preview blocker銆?
5. branch_fork 蹇呴』鏄惧紡 sequence_order锛屼笖鍙兘浣跨敤 original_successor / inserted_logic銆?
```

## execute_task

execute_task 鍙墽琛?preview 宸查€氳繃鐨?TaskSpec / TaskPlan銆?

濡傛灉鏈?preview锛?

```text
error.code=preview_required
agent_action=run_preview_task_first
```

鎵ц澶辫触蹇呴』杩斿洖 execution_state锛?

```json
{
  "execution_state": {
    "started": true,
    "write_started": true,
    "modified": false,
    "rollback_result": "rolled_back",
    "safe_to_retry": false
  }
}
```

rollback blocked / failed 鏃讹細

```text
state_trust=unsafe
agent_action=stop_and_report
```

execute 浠嶅彲鑳藉洜涓?UE 褰撳墠鐘舵€併€佽祫浜у彉鍖栥€丒ditor 鍐欏叆鎴栭摼鎺ュけ璐ヨ€屽け璐ャ€傚け璐ヨ繑鍥炲繀椤讳繚鐣欓潪绌?error code/message/stage锛涚┖閿欒瑕佸綊涓€鍖栨垚鍙姤鍛婇敊璇紝Agent 涓嶅簲缁х画鐚滄祴鎴栧洖閫€鍒板師瀛愬啓鍏ュ彛銆?

GraphWrite `branch_fork` execute/read-back 瑙勫垯锛?

```text
1. execute 鎴愬姛鍚庤鍙?LogicJson 鎴?LogicMd銆?
2. 纭 anchor 鍚庢彃鍏?Sequence 鎴栫瓑浠峰垎鍙戣妭鐐广€?
3. 纭 inserted call 宸茬敓鎴愬苟鍙揪銆?
4. 纭 original successor 浠嶄粠 Sequence 鍒嗘敮鍙揪銆?
5. 纭鍙楀奖鍝嶆墽琛屾祦鏃犲绔嬭妭鐐广€?
```

