# BlueprintHelper Hybrid Source Docs Sync Package

鏃ユ湡锛?026-05-04  
鐘舵€侊細娣峰悎 TaskSpec / TaskPlan 鏋舵瀯纭鍚庣殑鏉ユ簮鏂囨。鍚屾绋? 
閫傜敤鑼冨洿锛欱lueprintHelper v0.4 / v0.5 璁捐鏂囨。銆丄gent Skill銆丮CP Server銆乁E 鎻掍欢渚?Task Runtime 瑙勫垝銆?

---

## 0. 鍚屾缁撹

鏈鍚屾涓嶆帹缈?2026-05-03 宸茬‘璁ょ殑宸ュ叿绨囧瓧娈靛崗璁紝鑰屾槸璋冩暣瀹冧滑鍦ㄦ柊鏋舵瀯涓殑浣嶇疆銆?

```text
鏃у彛寰勶細Agent 鐩存帴闈㈠澶ч噺 CLI 搴曞眰宸ュ叿銆?
鏂板彛寰勶細Agent 闈㈠灏戦噺浠诲姟绾?CLI 鍛戒护锛涚幇鏈夊伐鍏风皣鍙樻垚 Python / UE Task Runtime 鐨勫唴閮?capability銆乨ebug tool銆佹祴璇曞叆鍙ｃ€?```

鏂颁富閾捐矾锛?

```text
Agent
鈫?BlueprintHelper CLI Task Commands
鈫?task-core / Python Task Compiler
鈫?UE Plugin Task Runtime
鈫?Existing UE Capability Clusters
鈫?Unreal Editor
```

鏂板鏍稿績姒傚康锛?

```text
TaskContextPack / context_id
TaskSpec
TaskPlan
task_run_id
TaskRunJournal
Task Error Layer
Bridge Operation Error Layer
```

淇濈暀鏃㈡湁姒傚康锛?

```text
runtime_profile
Safety Profile
dry_run
validation
transaction_id
Transaction Journal
Review
rollback
block_id
ownership metadata
LogicMD / LogicJson / RawJson / resource_ref
Append / Replace / Patch / Merge
Asset Factory / Component / Class Settings / Enhanced Input Boundary
```

---

## 1. 鏂囨。鍚屾浼樺厛绾?

### P0锛氬繀椤诲悓姝?

| 鏂囨。 | 鍚屾鐩爣 |
|---|---|
| `BlueprintHelper 鎻掍欢鏋舵瀯.txt` | 灏嗏€滃洓灞傛灦鏋勨€濇墿灞曚负鈥淎gent-facing Task Tools + Task Compiler + UE Task Runtime + Capability Clusters鈥濈殑娣峰悎鏋舵瀯銆?|
| `鍐欏伐鍏疯璁?鍚屾绋?synced_20260503.md` | 鏄庣‘ Graph Write 涓嶅啀鏄櫘閫?Agent 榛樿鐩磋繛鍏ュ彛锛岃€屾槸 TaskPlan / UE Task Runtime 鍐呴儴鑳藉姏銆?|
| `06_Transaction_Journal_Review_Design_SyncedDiff_20260503.md` | 鏂板 `task_run_id`銆乣TaskRunJournal`銆乼ask-level Review 鍒嗙粍銆乀ask-level RejectAll銆?|
| `05_Validation_Diagnostics_Tools_Design_SyncedDiff_20260503.md` | 鏂板 `read_task_context`銆乣preview_task`銆乣context_required`銆乣preview_blocked` 鐨?ok/status 璇箟銆?|
| `07_Safety_Profile_DryRun_Design_SyncedDiff_20260503.md` | 鏂板 TaskSpec / TaskPlan 灞傜殑瀹夊叏绛栫暐锛歳untime_profile 浠嶆槸鏉冨▉鏉ユ簮锛岀姝?per-call profile override銆?|
| `鏍稿績涓夌鑳藉姏缂哄彛.txt` | 浠庘€滀笁绔兘鍔涚己鍙ｂ€濇洿鏂颁负鈥滃洓娈典换鍔￠摼璺己鍙ｂ€濓細Task Compiler銆乀ask Runtime銆乀askRunJournal銆丒rror Normalizer銆?|

### P1锛氶渶瑕佸悓姝?

| 鏂囨。 | 鍚屾鐩爣 |
|---|---|
| `01_Asset_Factory_Tools_Design_SyncedDiff_20260503.md` | Asset Factory 缁х画淇濈暀鍘熻竟鐣岋紝浣嗘櫘閫?Agent 涓嶉粯璁ょ洿鎺ヨ皟鐢紱TaskSpec 鐨?resources / asset_policy 缂栬瘧鍒?Asset Factory step銆?|
| `02_Blueprint_Component_Tools_Design_SyncedDiff_20260503.md` | Component 宸ュ叿缁х画浣滀负 TaskPlan step锛沘dd_component 涓?set_component_properties 浠嶅垎绂汇€?|
| `03_Blueprint_Class_Settings_Tools_Design_SyncedDiff_20260503.md` | Class Settings 缁х画鍙礋璐ｅ０鏄庡眰锛汿askSpec 涓?interface implementation body 浠嶈浆 Graph Write銆?|
| `04_Enhanced_Input_Boundary_Design_SyncedDiff_20260503.md` | TaskContextPack 杩斿洖 IA 鍊欓€夛紱TaskSpec 蹇呴』鏄惧紡鍖哄垎 IA 寮曠敤銆両MC 缂栬緫銆佷簨浠跺叆鍙ｆ帴鍏ャ€?|
| `UE宸ュ叿璁捐涓庡瓧娈垫敹鏁?txt` | Append / Replace / Patch / Merge 瀛楁浠嶆湁鏁堬紝浣嗛粯璁ゅ畾浣嶅彉鎴愬唴閮?capability / debug-facing tool銆?|
| `鍏ㄥ姛鑳芥祴璇曠敤渚嬬敓鎴?txt` | 娴嬭瘯璺緞鏀逛负 read_context 鈫?preview_task 鈫?execute_task锛涘簳灞傚伐鍏蜂繚鐣欏崟鍏冩祴璇?/ debug 娴嬭瘯銆?|
| `鐗堟湰瑙勫垝涓庡畾涔?txt` | v0.4/v0.5 鐗堟湰绾挎柊澧?TaskRun / Task Runtime / Task Compiler 涓婚銆?|

---

## 2. 鎬讳綋鍙ｅ緞鏇挎崲

### 2.1 鏃ц〃杩?

```text
Agent 閫氳繃 CLI 鐩存帴璋冪敤 Asset Factory銆丆omponent銆丆lass Settings銆丟raph Write銆乂alidation 绛夊ぇ閲忓伐鍏峰畬鎴愪换鍔°€?
```

### 2.2 鏂拌〃杩?

```text
Agent 榛樿閫氳繃浠诲姟绾?CLI 鍛戒护鎻愪氦 TaskSpec銆倀ask-core / Python 渚у皢 TaskSpec 缂栬瘧涓?TaskPlan锛孶E 鎻掍欢渚?Task Runtime 鎵ц TaskPlan銆傜幇鏈夊伐鍏风皣缁х画浣滀负鍐呴儴 capability銆乨ebug tool 鍜屾祴璇曞叆鍙ｃ€?```

---

## 3. 鏂板鏂囨。寤鸿

寤鸿鍦?`BlueprintHelper/Develop/Plan/` 鏂板浠ヤ笅鏂囨。锛?

```text
00_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md
08_Source_Docs_Hybrid_Sync_Map_20260504.md
09_Agent_Facing_Task_Tools_Design_20260504.md
10_TaskSpec_TaskPlan_Error_Layers_20260504.md
11_TaskRun_Journal_ContextPack_Design_20260504.md
12_Source_Doc_Addendum_Snippets_20260504.md
```

---

## 4. 鏂版灦鏋勪笅鐨勯粯璁?Agent 娴佺▼

```text
1. Agent 璋冪敤 blueprinthelper_read_task_context銆?
2. Python / CLI 杩斿洖 TaskContextPack銆?
3. Agent 鍩轰簬 context_id 鐢熸垚 TaskSpec銆?
4. Agent 璋冪敤 blueprinthelper_preview_task銆?
5. preview_task 杩斿洖 passed / context_required / preview_blocked / structured error銆?
6. Agent 鏍规嵁 suggested_patch 淇 TaskSpec锛屽繀瑕佹椂璇㈤棶鐢ㄦ埛銆?
7. Agent 璋冪敤 blueprinthelper_execute_task銆?
8. UE Task Runtime 鎵ц TaskPlan锛岀敓鎴?task_run_id 鍜屽涓?child transaction_id銆?
9. Review UI 鎸?task_run_id 鍒嗙粍鏄剧ず銆?
10. Agent 鏈€缁堟姤鍛婂彧杈撳嚭浠诲姟鎽樿銆佷慨鏀硅祫浜с€佺紪璇?淇濆瓨/鏈畬鎴愰」銆?
```

---

## 5. 涓嶅彉瑙勫垯

浠ヤ笅瑙勫垯涓嶅洜鏂板 Task Compiler / Task Runtime 鑰屾敼鍙橈細

```text
1. Asset Factory 鍙垱寤鸿祫浜э紝涓嶆坊鍔犳帴鍙ｅ埌钃濆浘锛屼笉鍐欐帴鍙ｅ嚱鏁?body銆?
2. add_component 鍙垱寤虹粍浠跺拰 attachment锛屼笉璁剧疆 mesh / collision / physics / material銆?
3. add_implemented_interface 鍙慨鏀?Implemented Interfaces銆?
4. Append 鍙拷鍔犵嫭绔嬮€昏緫鍧椼€?
5. Merge 鎵嶈礋璐ｆ帴鍏ュ凡鏈夋墽琛屾祦銆?
6. Patch 蹇呴』绮剧‘瀹氫綅 node / pin / link / default value銆?
7. Replace 蹇呴』鏇挎崲鏄庣‘鐩爣銆?
8. Enhanced Input 榛樿涓嶇紪杈?IA / IMC銆?
9. runtime_profile.active_profile 鏄?safety_profile 鍞竴 Agent 鏉ユ簮銆?
10. 鎵€鏈夌湡瀹?UE 鍐欐搷浣滀粛杩涘叆 Transaction Journal / Review銆?
```

---

## 6. 鏂板瑙勫垯

```text
1. TaskSpec 涓嶆槸 transaction 鐨勬浛浠ｅ搧銆?
2. 涓€娆?TaskSpec 鎵ц鐢熸垚涓€涓?task_run_id銆?
3. 涓€涓?task_run_id 鍙寘鍚涓?transaction_id銆?
4. TaskRunJournal 璐熻矗缁勭粐 TaskSpec銆乀askPlan銆乧hild transactions銆乿alidation summary銆?
5. Agent 榛樿涓嶆帴鏀跺畬鏁?child transaction_id 鍒楄〃锛岄櫎闈?debug / rollback / failure 闇€瑕併€?
6. preview_task 姘歌繙涓嶄慨鏀?UE 璧勪骇銆?
7. preview_task 鍙互杩斿洖 context_required锛屾寚瀵?Agent 璇诲彇鏇村涓婁笅鏂囥€?
8. preview_blocked / dry_run blocked 灞炰簬宸ュ叿鎴愬姛鎵ц浣嗕换鍔′笉鍙墽琛岋紝閫氬父 ok=true銆?
9. TaskSpec schema / semantic / suggested_patch 鍦?Python / CLI 灞傚鐞嗐€?
10. Bridge / UE operation error 鐢?Python Error Normalizer 杞瘧涓?Agent-facing Task Error銆?
```

---

## 7. 鐗堟湰绾垮悓姝ュ缓璁?

```text
v0.4.0锛歍askRun / Transaction Review / grouped Review / rollback workflow
v0.5.0锛欰gent-facing Task Tools / TaskContextPack / TaskSpec / Task Compiler / TaskPlan
v0.6.0锛歎E Task Runtime 瀹屾暣鎵ц DAG銆佽法璧勪骇浠诲姟銆佷笂涓嬫枃绱㈠紩涓庢壒閲忛獙璇?
v1.0.0锛氱ǔ瀹?TaskSpec / TaskPlan schema銆佺ǔ瀹?Review / rollback銆佺ǔ瀹?debug tool 鏆撮湶绛栫暐
```

---

## 8. 瀹炵幇鍚屾寤鸿

### Phase 1锛氭枃妗ｄ笌鍗忚

```text
1. 鍥哄畾 TaskContextPack.v1銆?
2. 鍥哄畾 TaskSpec.v1銆?
3. 鍥哄畾 TaskPlan.v1銆?
4. 鍥哄畾 Task Error Layer銆?
5. 鍥哄畾 Bridge Operation Error Layer銆?
```

### Phase 2锛歅ython / CLI Task Compiler

```text
1. blueprinthelper_read_task_context銆?
2. blueprinthelper_preview_task銆?
3. TaskSpec schema / semantic validation銆?
4. suggested_patch銆?
5. TaskSpec 鈫?TaskPlan銆?
```

### Phase 3锛歎E Task Runtime

```text
1. ExecuteTaskPlan Bridge command銆?
2. TaskPlan validator銆?
3. TaskExecutionContext銆?
4. task_run_id銆?
5. TaskRunJournal銆?
6. child transaction grouping銆?
```

### Phase 4锛歊eview UI

```text
1. 鎸?task_run_id 鍒嗙粍銆?
2. 灞曞紑 child transactions銆?
3. Task-level AcceptAll / RejectAll銆?
4. rollback blocked / failed 灞曠ず銆?
```


