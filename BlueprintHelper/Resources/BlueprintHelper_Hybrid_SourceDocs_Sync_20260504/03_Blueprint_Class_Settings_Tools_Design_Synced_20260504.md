# 03 Blueprint Class Settings Tools 璁捐鏂囨。锛堝凡鍚屾纭 Diff锛?

鏃ユ湡锛?026-05-03  
宸ュ叿绨囷細Blueprint Class Settings Tools / 钃濆浘绫婚厤缃伐鍏风皣  
鐘舵€侊細鍚屾纭 Diff 鍚庣殑淇鐗? 
鍚屾鑼冨洿锛氱Щ闄?`set_parent_class` / Reparent 瀹炵幇鍙ｅ緞銆佹帴鍙ｆ搷浣滆竟鐣屻€佹壒閲忎簨鍔¤鍒欍€丆lass Settings 瀛楁鏀舵暃銆佹櫘閫氬伐鍏蜂笉榛樿杩斿洖 transaction/review/safety銆?

---

## 0. 鏈鍚屾缁撹

鏈枃浠舵浛鎹㈡棫鐗堜腑浠ヤ笅杩囨湡鍙ｅ緞锛?

```text
1. 绗竴鐗?Blueprint Class Settings 涓嶅寘鍚?set_parent_class銆?
2. parent_class 鍙綔涓?read_class_settings 杩斿洖鐨勫彧璇诲瓧娈点€?
3. 涓嶆毚闇?parent_class_result / requested_parent_class / confirmed_after_dry_run / parent-class dry_run/apply 璇箟銆?
4. add_implemented_interface 鍙慨鏀?Implemented Interfaces锛屼笉鍒涘缓 BPI锛屼笉鍒涘缓鎺ュ彛鍑芥暟瀹炵幇鍥撅紝涓嶅啓鎺ュ彛鍑芥暟 body銆?
5. 鎵归噺 Interface / Class Default 鍐欏叆榛樿浜嬪姟寮忥紝涓嶆敮鎸?partial apply銆?
6. 鏅€?Class Settings 鎴愬姛缁撴灉涓嶉粯璁よ繑鍥?transaction / review / safety銆?
```

---

## 1. 瀹氫綅

Blueprint Class Settings Tools 鐙珛鎴愮皣锛岃礋璐ｈ摑鍥剧被绾ц缃鍙栧拰閮ㄥ垎澹版槑灞備慨鏀广€?

瀹冧笉骞跺叆 Graph Write锛屼篃涓嶅苟鍏ユ櫘閫?Blueprint Structure Tools銆?

绗竴鐗堣寖鍥村簲淇濇寔鏀舵暃锛岄伩鍏嶆妸鈥滅被璁剧疆鈥濃€滄帴鍙ｅ疄鐜扳€濃€滃嚱鏁?Override鈥濃€滅埗绫昏縼绉烩€濇贩鎴愪竴涓繃瀹藉伐鍏风皣銆?

---

## 2. 绗竴鐗堣鐩栬寖鍥?

绗竴鐗堣鐩栵細

```text
read_class_settings
add_implemented_interface
add_implemented_interfaces
remove_implemented_interface
remove_implemented_interfaces
set_class_default_property
set_class_default_properties
```

绗竴鐗堜笉鍖呭惈锛?

```text
set_parent_class
blueprint_reparent
parent_class_result
create interface function implementation body
connect interface function to EventGraph
create Blueprint Interface asset
create function override
create engine event entry
```

`parent_class` 浠嶄繚鐣欏湪璇诲彇缁撴灉涓紝浠呬綔涓哄彧璇讳俊鎭€?

---

## 3. Reparent / Parent Class 杈圭晫

榛樿涓嶆彁渚涳細

```text
set_parent_class
blueprint_reparent
```

瑙勫垯锛?

```text
BlueprintHelper 绗竴鐗堜笉榧撳姳 Agent 淇敼宸插垱寤鸿摑鍥剧殑鐖剁被銆?
宸插垱寤鸿摑鍥惧鏋滅埗绫婚敊璇紝鎺ㄨ崘閲嶆柊鍒涘缓姝ｇ‘鐖剁被鐨勬柊钃濆浘銆?
Reparent 涓嶄綔涓?Agent 榛樿鍙敤鑳藉姏銆?
```

濡傛灉鏈潵纭疄闇€瑕佺埗绫昏縼绉伙紝搴斾綔涓哄崟鐙珮椋庨櫓杩佺Щ宸ュ叿閲嶆柊璁捐锛屼笉鑳藉鍥炵涓€鐗?Class Settings銆?

Agent 瑙勫垯锛?

```text
濡傛灉鐢ㄦ埛浠诲姟闇€瑕佷慨鏀?Parent Class锛孉gent 搴?stop_and_report锛岃鏄庡綋鍓?Blueprint Class Settings 绗竴鐗堜笉鏀寔璇ヨ兘鍔涖€?
```

---

## 4. read_class_settings

`read_class_settings` 杩斿洖锛?

```json
{
  "class_settings": {
    "parent_class": "/Script/Engine.Actor",
    "generated_class": "BP_BH_PhysicsDoor_C",
    "implemented_interfaces": [
      "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
    ],
    "class_default_count": 12
  }
}
```

瀛楁瑙勫垯锛?

| 瀛楁 | 瑙勫垯 |
|---|---|
| `parent_class` | 瀹屾暣绫昏矾寰勶紝渚嬪 `/Script/Engine.Actor`锛屽彧璇汇€?|
| `generated_class` | 鐢熸垚绫荤煭鍚嶏紝渚嬪 `BP_BH_PhysicsDoor_C`锛屼笉鏄畬鏁村璞¤矾寰勩€?|
| `implemented_interfaces` | 鎺ュ彛璧勪骇璺緞鍒楄〃銆?|
| `class_default_count` | 榛樿灞炴€ф憳瑕佹暟閲忥紝涓嶆槸 Class Defaults 蹇収銆?|

`read_class_settings` 涓嶈繑鍥炲畬鏁?Class Defaults 蹇収銆傞渶瑕佽鍙栧叿浣撻粯璁ゅ睘鎬ф椂锛屽簲浣跨敤鍚庣画涓撶敤璇诲彇鑳藉姏鎴栧甫 filter 鐨?Class Defaults 璇诲彇鑳藉姏銆?

Agent 涓嶅緱鎶?`generated_class` 褰撲綔璧勪骇璺緞鎴?object path 浣跨敤銆?

---

## 5. Interface 娣诲姞 / 绉婚櫎

鎺ュ彛宸ュ叿璋冪敤灞傚尯鍒嗭細

```text
add_implemented_interface       鍗曚釜鎺ュ彛
add_implemented_interfaces      澶氫釜鎺ュ彛
remove_implemented_interface    鍗曚釜鎺ュ彛
remove_implemented_interfaces   澶氫釜鎺ュ彛
```

杩斿洖灞傜粺涓€浣跨敤锛?

```text
data.interface_result
```

鍗曚釜鎺ュ彛鍙槸锛?

```text
mode=single
requested_count=1
```

澶氫釜鎺ュ彛鏄細

```text
mode=batch
requested_count>1
```

绀轰緥锛?

```json
{
  "interface_result": {
    "mode": "single",
    "requested_count": 1,
    "applied_count": 1,
    "already_implemented_count": 0,
    "removed_count": 0,
    "invalid_interfaces": []
  }
}
```

---

## 6. Interface 宸ュ叿鑱岃矗杈圭晫

`add_implemented_interface` / `add_implemented_interfaces` 鍙慨鏀圭洰鏍?Blueprint 鐨?Class Settings锛?

```text
Implemented Interfaces
```

瀹冧滑涓嶄細鑷姩锛?

```text
1. 鍒涘缓 Blueprint Interface 璧勪骇銆?
2. 鍒涘缓鎺ュ彛鍑芥暟瀹炵幇鍥俱€?
3. 鍐欐帴鍙ｅ嚱鏁?body銆?
4. 灏嗘帴鍙ｅ嚱鏁版帴鍏?EventGraph銆?
```

濡傛灉浠诲姟瑕佹眰鈥滃畬鏁村疄鐜版帴鍙ｄ氦浜掆€濓紝Agent 搴旀媶涓猴細

```text
1. Asset Factory 鍒涘缓 BPI銆?
2. Blueprint Class Settings 娣诲姞 Implemented Interface銆?
3. Graph Write 鍒涘缓鎴栧疄鐜版帴鍙ｅ嚱鏁伴€昏緫銆?
4. Compile / Save銆?
```

Agent 涓嶅緱鎶娾€滄帴鍙ｅ凡娣诲姞鍒?Blueprint鈥濊鍒や负鈥滄帴鍙ｅ姛鑳藉凡瀹炵幇鈥濄€?

---

## 7. Interface 浜嬪姟瑙勫垯

鎵归噺 Interface 鎿嶄綔榛樿浜嬪姟寮忥細

```text
鍙瀛樺湪 invalid_interfaces锛岄粯璁や笉搴旂敤浠讳綍鎺ュ彛淇敼銆?
```

鍑虹幇鏃犳晥鎺ュ彛鏃讹細

```text
ok=false
status=failed
modified=false
applied_count=0
removed_count=0
```

绗竴鐗堜笉鏀寔锛?

```text
partial apply
allow_partial=true
```

Agent 涓嶅緱鍋囪閮ㄥ垎鎺ュ彛宸茬粡鎴愬姛搴旂敤銆?

---

## 8. remove_implemented_interface 杈圭晫

绉婚櫎鎺ュ彛鍙兘褰卞搷锛?

```text
1. 鎺ュ彛鍑芥暟瀹炵幇鍥俱€?
2. 璋冪敤鏂瑰紩鐢ㄣ€?
3. 钃濆浘缂栬瘧缁撴灉銆?
```

Agent 搴斿湪绉婚櫎鍓嶇‘璁ょ洰鏍囨槑纭€傚繀瑕佹椂鍏?`read_class_settings`锛岀‘璁ゆ帴鍙ｇ‘瀹炲瓨鍦ㄣ€?

鍒犻櫎 Interface 灞炰簬楂橀闄╃被绾т慨鏀癸紝Conservative 涓嬪簲 dry_run銆?

---

## 9. Class Default 灞炴€ц缃?

璋冪敤灞傚尯鍒嗭細

```text
set_class_default_property      鍗曚釜榛樿灞炴€?
set_class_default_properties    澶氫釜榛樿灞炴€?
```

杩斿洖灞傜粺涓€浣跨敤锛?

```text
data.default_property_result
```

鍗曞睘鎬у彧鏄細

```text
mode=single
requested_count=1
```

鎵归噺灞炴€ф槸锛?

```text
mode=batch
requested_count>1
```

绀轰緥锛?

```json
{
  "default_property_result": {
    "mode": "batch",
    "requested_count": 4,
    "applied_count": 4,
    "changed_count": 3,
    "no_op_count": 1,
    "invalid_settings": []
  }
}
```

---

## 10. Class Default 涓嶅洖鏄惧揩鐓?

鎴愬姛鏃朵笉杩斿洖锛?

```text
before
after
all_defaults
```

鍘熷洜锛?

```text
1. before / after 灞炰簬 UE 鍐呴儴 diff / Review / debug銆?
2. 澶у璞″睘鎬у洖鏄炬氮璐?Token銆?
3. 鎴愬姛缁撴灉鍙渶瑕佹墽琛屾憳瑕併€?
```

濡傛灉 Agent 闇€瑕佺‘璁ゆ渶缁堥粯璁ゅ€硷紝搴斾娇鐢ㄥ悗缁笓鐢ㄨ鍙栧伐鍏锋垨甯?filter 鐨勮鍙栬兘鍔涖€?

---

## 11. Class Default 浜嬪姟瑙勫垯

鎵归噺 Class Default 淇敼榛樿浜嬪姟寮忥細

```text
鍙瀛樺湪 invalid_settings锛岄粯璁や笉搴旂敤浠讳綍榛樿灞炴€т慨鏀广€?
```

鍑虹幇鏃犳晥璁剧疆鏃讹細

```text
ok=false
status=failed
modified=false
applied_count=0
changed_count=0
no_op_count=0
```

绗竴鐗堜笉鏀寔锛?

```text
partial apply
allow_partial=true
```

Agent 涓嶅緱鍋囪閮ㄥ垎灞炴€у凡缁忔垚鍔熷啓鍏ャ€?

---

## 12. Override / Interface Implementation 杩滄湡杈圭晫

浠ヤ笅鑳藉姏涓嶅睘浜庣涓€鐗?Class Settings 鐨勫瓧娈靛崗璁細

```text
blueprint_list_overridable_functions
blueprint_create_function_override
create engine event entry
create interface function implementation entry
migrate existing function to interface function
```

濡傛灉鏈潵闇€瑕侊紝搴斾綔涓虹嫭绔嬪伐鍏风皣鎴?Class Settings 鍚庣画闃舵閲嶆柊璁捐锛屽苟鏄庣‘涓?Graph Write 鐨勮竟鐣岋細

```text
鍒涘缓鍏ュ彛涓嶇瓑浜庡啓鍑芥暟浣撻€昏緫銆?
鍑芥暟浣撳唴閮ㄩ€昏緫浠嶄氦缁?Graph Write銆?
```

绗竴鐗堟枃妗ｄ笉鍐嶆妸杩欎簺鑳藉姏鍒椾负鎺ㄨ崘宸ュ叿锛岄伩鍏?Agent 璇垽褰撳墠鍙敤鑳藉姏銆?

---

## 13. dry_run

鎵€鏈?Class Settings 鍐欐搷浣滃繀椤绘敮鎸?dry_run銆?

Conservative 涓嬮珮椋庨櫓绫荤骇淇敼蹇呴』 dry_run锛屼緥濡傦細

```text
鍒犻櫎 Interface
淇敼鐢ㄦ埛宸叉湁绫婚厤缃?
淇敼 Tick / Replication / Spawn / Input 绫昏缃?
Class Default 淇敼褰卞搷杩愯鏃跺疄渚嬭涓?
```

涓嶅啀瀛樺湪锛?

```text
set_parent_class 蹇呴』 dry_run
parent_class dry_run
confirmed_after_dry_run
```

鍥犱负绗竴鐗堜笉鎻愪緵淇敼 Parent Class 鐨勮兘鍔涖€?

---

## 14. Agent-facing 鎴愬姛杩斿洖瀛楁

鏅€?Class Settings 鎴愬姛缁撴灉榛樿涓嶅寘鍚細

```text
transaction
review
safety
```

Agent 涓嶅簲鏈熷緟杩欎簺瀛楁銆?

瑙勫垯锛?

```text
1. safety_profile 鍙粠 runtime_profile.active_profile 璇诲彇銆?
2. dry_run 淇℃伅鍙湪 status=dry_run 鏃朵粠 data.dry_run 璇诲彇銆?
3. transaction_id 鍙互鐢?UE 鎻掍欢鍐呴儴鐢熸垚骞跺啓鍏?Journal / Review锛屼絾鏅€氬伐鍏蜂笉榛樿鏆撮湶缁?Agent銆?
4. Agent 鏈€缁堟姤鍛婇粯璁や笉杈撳嚭 transaction_id 鎴?review_status銆?
```

---

## 15. 鏈€缁堟姤鍛婅鍒?

姝ｅ父瀹屾垚鏃讹紝Agent 鍙姤鍛婏細

```text
1. 娣诲姞鎴栫Щ闄や簡鍝簺鎺ュ彛銆?
2. 淇敼浜嗗灏戜釜榛樿灞炴€с€?
3. 鏄惁瀛樺湪 no_op銆?
4. 鏄惁闇€瑕?compile/save銆?
```

涓嶆姤鍛婏細

```text
transaction_id
review_status
journal_path
rollback_data
before / after 灞炴€у€?
瀹屾暣榛樿灞炴€у垪琛?
鐖剁被鏄惁淇敼
```

鐖剁被淇敼涓嶅湪绗竴鐗堣兘鍔涜寖鍥村唴銆?

---

## 16. Agent 绂佹琛屼负

Agent 涓嶅緱锛?

```text
1. 鐢?add_implemented_interface 鍒涘缓 BPI 璧勪骇銆?
2. 鐢?add_implemented_interface 鑷姩鐢熸垚鎺ュ彛鍑芥暟 body銆?
3. 鐢?Class Settings 宸ュ叿鍐欏浘琛ㄩ€昏緫銆?
4. 璁″垝閫氳繃 Class Settings 淇敼 Parent Class銆?
5. 鍦ㄦ壒閲?Interface 鎴?Class Default 澶辫触鏃跺亣璁鹃儴鍒嗕慨鏀瑰凡搴旂敤銆?
6. 鏈熷緟 before / after銆?
7. 鏈熷緟瀹屾暣 Class Default 蹇収銆?
8. 鍦ㄦ渶缁堟姤鍛婁腑榛樿杈撳嚭 transaction_id 鎴?review_status銆?
```

---

## 17. 楠屾敹鏍囧噯

```text
1. read_class_settings 杩斿洖 parent_class / generated_class / implemented_interfaces / class_default_count銆?
2. parent_class 鏄彧璇诲瓧娈碉紝涓嶆彁渚?set_parent_class銆?
3. generated_class 鍙繑鍥炵煭鍚嶏紝涓嶈繑鍥炲畬鏁磋矾寰勩€?
4. Interface 宸ュ叿璋冪敤灞傚尯鍒嗗崟涓?/ 澶氫釜銆?
5. Interface 杩斿洖缁熶竴 interface_result銆?
6. Interface 鏃犳晥鏃堕粯璁や簨鍔″紡锛屼笉搴旂敤浠讳綍鎺ュ彛銆?
7. add_implemented_interface 涓嶈嚜鍔ㄥ垱寤?BPI銆?
8. add_implemented_interface 涓嶈嚜鍔ㄥ垱寤烘帴鍙ｅ嚱鏁板疄鐜板浘銆?
9. Class Defaults 浣跨敤 default_property_result銆?
10. default_property_result 涓?Component property_result 鍚屾瀯銆?
11. 鎴愬姛鏃朵笉鍥炴樉鎵€鏈夐粯璁ゅ睘鎬с€?
12. 鎴愬姛鏃朵笉杩斿洖 before / after銆?
13. 鏃犳晥椤瑰彧杩斿洖 invalid_interfaces 鎴?invalid_settings銆?
14. 鎵归噺 Class Default 璁剧疆榛樿浜嬪姟寮忋€?
15. 绗竴鐗堜笉鏀寔 partial apply銆?
16. 榛樿涓嶈繑鍥?transaction / review / safety銆?
17. Agent 閬囧埌淇敼 Parent Class 闇€姹傛椂 stop_and_report銆?
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

