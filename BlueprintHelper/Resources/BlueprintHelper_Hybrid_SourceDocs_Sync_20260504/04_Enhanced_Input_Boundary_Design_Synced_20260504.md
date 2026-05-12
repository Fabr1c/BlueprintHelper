# 04 Enhanced Input / Input Reference Boundary 璁捐鏂囨。锛堝凡鍚屾纭 Diff锛?

鏃ユ湡锛?026-05-03  
宸ュ叿绨囷細Enhanced Input / Input Reference Boundary  
鐘舵€侊細鍚屾纭 Diff 鍚庣殑淇鐗? 
鍚屾鑼冨洿锛氫笌 Asset Factory / Graph Write / runtime profile / stop_and_report 瑙勫垯瀵归綈銆?

---

## 0. 鏈鍚屾缁撹

鏈枃浠剁淮鎸佹棫鐗堟牳蹇冭竟鐣岋紝浣嗗悓姝ヤ互涓嬭鍒欙細

```text
1. 褰撳墠闃舵涓嶅皢 Enhanced Input 缂栬緫鑳藉姏浣滀负 BlueprintHelper 榛樿鏍稿績鑳藉姏銆?
2. Agent 涓嶅簲榛樿鑷姩鍒涘缓鎴栦慨鏀?InputAction / InputMappingContext銆?
3. 濡傛灉 Asset Factory 褰撳墠鐗堟湰鏀寔鍒涘缓 InputAction锛屼篃蹇呴』浠ョ敤鎴锋槑纭洰鏍囧拰 runtime profile 鑳藉姏涓哄墠鎻愩€?
4. IA 浜嬩欢鍏ュ彛灞炰簬 Graph Write / 浜嬩欢鍏ュ彛鑳藉姏锛屼笉淇敼 IA / IMC 璧勪骇銆?
5. 鎺ュ叆宸叉湁鎵ц娴佸繀椤讳娇鐢?MergeBlueprintGraph锛屽苟 dry_run銆?
6. Enhanced Input 鑳藉姏缂哄け鏃讹紝Agent 搴旀寜 runtime_profile unavailable_only 鍜?missing_capability_policy 鍒ゆ柇 stop_and_report銆?
```

---

## 1. 瀹氫綅

褰撳墠闃舵涓嶅皢 Enhanced Input 缂栬緫鑳藉姏浣滀负 BlueprintHelper 榛樿鏍稿績鑳藉姏銆?

IA / IMC 灞炰簬椤圭洰杈撳叆鏋舵瀯灞傦紝閫氬父闇€瑕佺敤鎴蜂笌 Agent 鍗忎綔纭銆?

BlueprintHelper 涓嶅簲榛樿璁?Agent 鑷姩鍒涘缓鎴栦慨鏀?InputAction / InputMappingContext銆?

---

## 2. 涓嶆彁渚涚殑榛樿鑳藉姏

褰撳墠闃舵涓嶆彁渚涙垨涓嶄紭鍏堟彁渚?Enhanced Input 涓撶敤鍐欏伐鍏凤細

```text
input_create_action
input_create_mapping_context
input_add_mapping
input_remove_mapping
input_update_mapping
```

涔熶笉鎻愪緵 Enhanced Input 涓撶敤瑙ｆ瀽宸ュ叿锛?

```text
input_list_actions
input_get_action
input_list_mapping_contexts
input_get_mapping_context
input_find_key_binding
```

濡傛灉鏈潵 Asset Factory 鏀寔鍒涘缓 InputAction锛屼篃涓嶇瓑浜?Enhanced Input 缂栬緫鑳藉姏宸茬粡瀹屾暣鍙敤銆傚垱寤?IA 璧勪骇銆佺紪杈?IMC 鏄犲皠銆佽杩愯鏃舵帴鏀惰緭鍏ユ槸涓変欢涓嶅悓鐨勪簨銆?

---

## 3. 杈撳叆绯荤粺杈圭晫

鎻掍欢涓嶈В鏋?IMC 鍐呴儴鎸夐敭鏄犲皠銆?

鎻掍欢涓嶅垽鏂細

```text
F 閿槸鍚﹀凡缁戝畾鍒?IA_Interact
鏌愪釜 IMC 鏄惁宸插姞鍏?Player Controller / Character
鏌愪釜 IA 鏄惁鍦ㄨ繍琛屾椂鍙Е鍙?
```

杈撳叆缁戝畾鏄惁姝ｇ‘锛屽睘浜庣敤鎴峰崗浣滈厤缃寖鍥淬€?

濡傛灉鎵句笉鍒版寚瀹?IA 璧勪骇锛孉gent 搴斿仠姝㈠苟鎶ュ憡闇€瑕佺敤鎴峰垱寤?/ 鎸囧畾 InputAction锛岃€屼笉鏄嚜鍔ㄥ垱寤恒€?

渚嬪锛?

```text
鐢ㄦ埛鏄庣‘瑕佹眰鍒涘缓 IA
+
runtime profile 鏈姤鍛?Asset Factory 瀵瑰簲鑳藉姏涓嶅彲鐢?
+
褰撳墠 Safety Profile 鍏佽鍒涘缓璧勪骇
```

杩欑鎯呭喌涓嬪彲璧?Asset Factory 鍒涘缓 IA 璧勪骇锛屼絾浠嶄笉浠ｈ〃 IMC 鏄犲皠宸插畬鎴愩€?

---

## 4. 鍏佽鐨勮兘鍔涳細寮曠敤宸叉湁 IA

Agent 鍙€氳繃閫氱敤璧勪骇鎼滅储 / 璧勪骇淇℃伅宸ュ叿鏌ユ壘宸叉湁 InputAction / InputMappingContext 璧勪骇銆?

濡傛灉鐢ㄦ埛鏄庣‘鎻愪緵 IA `asset_path`锛屽彲鐩存帴寮曠敤璇ヨ祫浜э紝浣嗕粛搴旀牎楠岋細

```text
璧勪骇瀛樺湪
璧勪骇绫诲瀷姝ｇ‘
```

濡傛灉 IA 璺緞涓嶆槸鐢ㄦ埛鏄庣‘缁欏嚭锛孉gent 蹇呴』閫氳繃璧勪骇鎼滅储纭鍞竴鍖归厤銆?

澶氫釜鍚屽悕鎴栫浉浼?IA 璧勪骇瀛樺湪鏃讹紝Agent 蹇呴』鍋滄骞惰姹傜敤鎴锋寚瀹氭槑纭祫浜ц矾寰勩€?

---

## 5. IA 浜嬩欢鍏ュ彛

钃濆浘鍥捐〃鍐欏叆鏃讹紝鍙互寮曠敤鐢ㄦ埛宸叉湁 IA 璧勪骇鍒涘缓鎴栬繛鎺ュ搴?InputAction 浜嬩欢鑺傜偣銆?

杩欏睘浜庯細

```text
Graph Write / 浜嬩欢鍏ュ彛鑳藉姏
```

涓嶆槸 Enhanced Input 璧勪骇缂栬緫鑳藉姏銆?

鍒涘缓 IA 浜嬩欢鍏ュ彛涓嶄慨鏀?IA / IMC 璧勪骇銆?

IA 浜嬩欢鍏ュ彛搴旇繑鍥烇細

```text
entry_ref
entry_node_guid
```

IA 浜嬩欢鍏ュ彛涓嶄娇鐢?`block_id`銆?

IA 浜嬩欢鍏ュ彛 ownership 涓庡叾浠栦簨浠跺叆鍙ｄ竴鑷达細

```text
entry metadata + Journal 鍙屽啓
涓嶅啓 BlueprintHelperBlockId
浜嬩欢鍚庢柟涓氬姟閫昏緫鐢?Graph Write 鍐欏叆鏃舵墠鐢熸垚 block_id
```

---

## 6. 鎺ュ叆宸叉湁鎵ц娴?

濡傛灉 IA 浜嬩欢鍏ュ彛鍚庡凡鏈夋墽琛屾祦锛屾帴鍏ユ柊閫昏緫蹇呴』浣跨敤锛?

```text
MergeBlueprintGraph
```

骞朵笖蹇呴』 dry_run銆?

Agent 涓嶅緱鐢?Append 鏇夸唬 Merge 鎺ュ叆宸叉湁鎵ц娴併€?

濡傛灉 IA 浜嬩欢鍏ュ彛涓嶅瓨鍦紝鍙敱 Graph Write / 浜嬩欢鍏ュ彛鑳藉姏鍒涘缓锛涘叿浣撹兘鍔涙槸鍚﹀彲鐢ㄤ互 runtime profile 鍜?CLI command contract 涓哄噯銆?

---

## 7. runtime_profile 涓?stop_and_report

Enhanced Input 鐩稿叧鑳藉姏鏄惁鍙敤锛屼笉鐢辨枃妗ｉ潤鎬佸亣璁惧喅瀹氥€?

Agent 搴旀寜锛?

```text
runtime_profile.tool_capabilities.mode = unavailable_only
```

鐞嗚В鑳藉姏缂哄け銆?

濡傛灉褰撳墠浠诲姟鏄庣‘闇€瑕侊細

```text
edit_mapping_context
input_add_mapping
input_find_key_binding
```

涓旇鑳藉姏鍦?runtime_profile unavailable 鍒楄〃涓紝涓旀棤瀹夊叏鏇夸唬璺緞锛孉gent 搴?stop_and_report銆?

濡傛灉鐢ㄦ埛鐩爣鍙互鏀逛负鈥滃紩鐢ㄧ敤鎴峰凡閰嶇疆濂界殑 IA 骞跺啓钃濆浘浜嬩欢閫昏緫鈥濓紝鍒?Agent 鍙互鍙畬鎴?Graph Write 閮ㄥ垎锛屽苟鏄庣‘璇存槑杈撳叆璧勪骇閰嶇疆涓嶅睘浜庢湰娆?Agent 鐙珛瀹屾垚鑼冨洿銆?

---

## 8. 娴嬭瘯楠屾敹鍙ｅ緞淇

鏃ч獙鏀讹細

```text
Agent 蹇呴』鐙珛鍒涘缓 IA_Interact 骞剁紪杈?IMC_Default銆?
```

鏂伴獙鏀讹細

```text
Agent 搴旇兘璇嗗埆闇€瑕佽緭鍏ョ粦瀹氥€?
Agent 搴旀姤鍛婇渶瑕佺敤鎴烽厤缃?IA / IMC锛屾垨璇锋眰鐢ㄦ埛鎸囧畾 IA銆?
鐢ㄦ埛瀹屾垚杈撳叆璧勪骇閰嶇疆鍚庯紝Agent 鍐嶇户缁畬鎴愯摑鍥句簨浠舵帴鍏ヤ笌浜や簰閫昏緫銆?
```

濡傛灉鐢ㄦ埛鏄庣‘瑕佹眰骞朵笖鑳藉姏鍙敤锛孉gent 鍙互鍒涘缓 IA 璧勪骇锛屼絾涓嶈兘鎶娾€淚A 宸插垱寤衡€濇姤鍛婃垚鈥滄寜閿槧灏勫凡瀹屾垚鈥濄€?

---

## 9. Agent 绂佹琛屼负

Agent 涓嶅緱锛?

```text
1. 榛樿鑷姩鍒涘缓 InputAction / InputMappingContext銆?
2. 榛樿鑷姩缂栬緫 IMC 鎸夐敭鏄犲皠銆?
3. 鎶?IA 浜嬩欢鍏ュ彛鍒涘缓璇涓?IMC 宸查厤缃€?
4. 鍦ㄥ涓?IA 鍖归厤鏃剁寽娴嬩娇鐢ㄥ叾涓竴涓€?
5. 鐢?Append 鎺ュ叆宸叉湁 IA 浜嬩欢鎵ц娴併€?
6. 缁曡繃 runtime_profile 鐨?Enhanced Input 鑳藉姏缂哄け銆?
7. 鎶婄敤鎴锋墜鍔ㄩ厤缃?IMC 璁″叆 Agent 鐙珛瀹屾垚鑳藉姏銆?
```

---

## 10. 楠屾敹鏍囧噯

```text
1. Agent 鑳藉尯鍒?IA 璧勪骇銆両MC 鏄犲皠銆佽摑鍥?IA 浜嬩欢鍏ュ彛銆?
2. Agent 涓嶉粯璁よ嚜鍔ㄥ垱寤烘垨淇敼杈撳叆璧勪骇銆?
3. Agent 寮曠敤 IA 鍓嶄細纭璧勪骇瀛樺湪涓旂被鍨嬫纭€?
4. 澶氫釜鍖归厤 IA 鏃跺仠姝㈠苟瑕佹眰鏄庣‘璺緞銆?
5. IA 浜嬩欢鍏ュ彛涓嶄娇鐢?block_id銆?
6. IA 浜嬩欢鍚庢柟涓氬姟閫昏緫鐢?Graph Write 鍒涘缓 block_id銆?
7. 鎺ュ叆宸叉湁鎵ц娴佸繀椤荤敤 MergeBlueprintGraph 骞?dry_run銆?
8. Enhanced Input 鑳藉姏涓嶅彲鐢ㄤ笖浠诲姟蹇呴』渚濊禆鏃讹紝Agent stop_and_report銆?
```
---

# 2026-05-04 娣峰悎鏋舵瀯鍚屾锛氬伐鍏风皣鏆撮湶灞傜骇

## 鍚屾缁撹

鏈枃妗ｄ腑鐨勫伐鍏风皣杈圭晫涓嶆帹缈伙紝浣?Agent-facing 鏆撮湶鏂瑰紡璋冩暣銆?

搴曞眰鑳藉姏绨囩户缁綔涓猴細

```text
1. UE Task Runtime step operation銆?
2. task-core / Python Task Compiler 鐨?capability 妯″瀷銆?
3. Debug / Expert / 娴嬭瘯鍏ュ彛銆?
```

鏅€?Agent 涓嶅簲榛樿鐩存帴鎵嬪姩鎷艰鏈伐鍏风皣璋冪敤閾俱€傛櫘閫氭祦绋嬫敼涓猴細

```text
read_task_context 鈫?preview_task 鈫?execute_task
```

## 杈圭晫浠嶇劧鏈夋晥

鏈伐鍏风皣鍘熸湁鑱岃矗杈圭晫浠嶅繀椤昏 Task Compiler / Task Runtime 閬靛畧銆?

渚嬪锛?

```text
Asset Factory 鍙垱寤鸿祫浜э紝涓嶆坊鍔犳帴鍙ｃ€佷笉鍐欐帴鍙ｅ嚱鏁?body銆?
Component add_component 鍙垱寤虹粍浠跺拰 attachment锛屼笉璁剧疆灞炴€с€?
Class Settings add_implemented_interface 鍙慨鏀?Implemented Interfaces銆?
Enhanced Input 褰撳墠涓嶉粯璁よ嚜鍔ㄧ紪杈?IA / IMC銆?
```

涔熷氨鏄锛屾贩鍚堟灦鏋勫彧鏀瑰彉鈥滆皝鏉ヨ皟鐢ㄥ伐鍏封€濓紝涓嶆敼鍙樷€滃伐鍏疯兘鍋氫粈涔堚€濄€?

## Agent-facing 杩斿洖璋冩暣

鏅€?execute_task 鎴愬姛缁撴灉榛樿涓嶅睍寮€鏈伐鍏风皣鐨勫簳灞傝繑鍥炪€?

搴曞眰 transaction / review / safety 浠嶈繘鍏?UE Journal / Review锛屼絾鏅€氫换鍔℃垚鍔熸憳瑕佸彧鎶ュ憡锛?

```text
浠诲姟鏄惁瀹屾垚
淇敼浜嗗摢浜涜祫浜?
鎵ц浜嗗灏戞楠?
鏄惁 compile/save
寮傚父鎴栨湭瀹屾垚椤?
```

