# 07 Safety Profile / dry_run 璁捐鏂囨。锛堝凡鍚屾纭 Diff锛?

鏃ユ湡锛?026-05-03  
宸ュ叿绨囷細Safety Profile / dry_run 瑙勫垯宸ュ叿绨? 
鐘舵€侊細鍚屾纭 Diff 鍚庣殑淇鐗? 
鍚屾鑼冨洿锛歳untime_profile 涓?safety_profile 鍞竴 Agent 鏉ユ簮銆佹櫘閫氬伐鍏蜂笉杩斿洖 safety銆佺Щ闄?parent_class 淇敼鑳藉姏銆乨ry_run 瀛楁浣嶇疆鏀舵暃銆丼etup / runtime profile 鏉冮檺杈圭晫銆?

---

## 0. 鏈鍚屾缁撹

鏈枃浠舵浛鎹㈡棫鐗堜腑浠ヤ笅杩囨湡鍙ｅ緞锛?

```text
1. Agent 涓嶄粠鍗曟宸ュ叿缁撴灉璇诲彇 safety_profile銆?
2. safety_profile 鍙粠 runtime_profile.active_profile 璇诲彇銆?
3. 鏅€氭垚鍔熷伐鍏风粨鏋滀笉榛樿杩斿洖 safety銆?
4. dry_run 鏁版嵁鍙湪 status=dry_run 鏃朵綅浜?data.dry_run銆?
5. Blueprint Class Settings 绗竴鐗堜笉鏀寔淇敼 Parent Class锛屽洜姝や笉鍐嶆妸 parent_class 瑙ｆ瀽涓嶆槑纭垪涓虹涓€鐗堥珮椋庨櫓鍐欏叆椤广€?
6. runtime_profile.tool_capabilities 鏄?unavailable_only 璐熷悜绋€鐤忓垪琛紝涓嶆槸瀹屾暣宸ュ叿绱㈠紩銆?
7. SetupProfile / Safety Profile 涓嶇敱 Agent 涓存椂瑕嗙洊銆?
```

---

## 1. Profile 妗ｄ綅

Safety Profile 浣跨敤浜旀。锛?

```text
ReadOnly
Conservative
Standard
AutoRepair
Expert
```

Profile 鏄繍琛屾椂瀹夊叏绛栫暐锛屼笉鏄?Agent 鍙殢鎰忔寚瀹氱殑宸ュ叿鍙傛暟銆?

Agent 蹇呴』閫氳繃锛?

```text
runtime_profile.active_profile.safety_profile
```

璇诲彇褰撳墠鐢熸晥妗ｄ綅銆?

---

## 2. ReadOnly

鍙厑璁革細

```text
璇诲彇
鎼滅储
璇婃柇
瀵煎嚭
棰勮
dry_run
Review 鍘嗗彶鏌ョ湅
```

绂佹锛?

```text
鍒涘缓璧勪骇
淇敼缁勪欢
淇敼绫昏缃?
Graph Write
Cleanup
Rollback
Save
```

ReadOnly 鍏佽 dry_run锛屼絾蹇呴』锛?

```text
execution_allowed=false
profile="ReadOnly"
reason="profile_is_read_only"
```

ReadOnly 涓?dry_run 涓嶅厑璁歌浆涓烘寮忓啓鍏ワ紝涓嶇敓鎴愬闃呯敤 transaction_id銆?

---

## 3. Conservative

榛樿瀹夊叏鍐欏叆妗ｃ€?

瑙勫垯锛?

```text
鍏佽鍐欐搷浣溿€?
楂橀闄╂搷浣滃繀椤?dry_run銆?
dry_run 鏃?error / conflict / blocker 鍚庢墠鍙墽琛屻€?
warning / info 涓嶉樆鏂寮忓啓鍏ャ€?
error / conflict / blocker 蹇呴』闃绘柇姝ｅ紡鍐欏叆銆?
姘镐笉鑷姩 save銆?
涓嶈嚜鍔?cleanup 鏃?owned block銆?
涓嶈嚜鍔ㄤ慨鏀圭敤鎴疯妭鐐?/ 鐢ㄦ埛缁勪欢 / 鐢ㄦ埛绫昏缃€?
```

Conservative 涓嬫櫘閫氫綆椋庨櫓鏂板缓鍙洿鎺ュ啓锛屼絾宸ュ叿浠嶅繀椤绘敮鎸?dry_run銆?

---

## 4. Standard

闈㈠悜甯歌 Agent 鑷姩鍖栧紑鍙戙€?

瑙勫垯锛?

```text
鍏佽鏇村浣庨闄╁啓鎿嶄綔鐩存帴鎵ц銆?
鍏佽鑷姩 diagnostics銆?
榛樿涓嶈嚜鍔?save銆?
鍙湪鏄惧紡鍙傛暟鎴?workflow 涓嬭嚜鍔?save銆?
瀵?owned 鍐呭鍙仛鏇翠富鍔ㄧ殑 replace_owned / cleanup銆?
楂橀闄╂搷浣滀粛寤鸿鎴栬姹?dry_run銆?
```

---

## 5. AutoRepair

闈㈠悜鑷姩淇 BlueprintHelper-owned 闂銆?

AutoRepair 涓嶇瓑浜庢棤闄愬埗鑷姩淇敼銆?

榛樿鍙嚜鍔ㄤ慨澶?BlueprintHelper-owned 鍐呭锛屼緥濡傦細

```text
owned block
owned component_group
owned event_entry
owned metadata inconsistency
owned Journal / Review 鐘舵€佷笉涓€鑷?
owned block 娈嬬暀
owned component group 娈嬬暀
owned entry 鍚庢柟 orphan owned nodes
```

鍙鍙?Diagnostics 缁撴灉鍚庤嚜鍔ㄨ皟鐢細

```text
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
cleanup_blueprint_helper_component_group
cleanup_blueprint_helper_override_entry
PatchBlueprintGraph
ReplaceBlueprintGraph
MergeBlueprintGraph
```

淇鐢ㄦ埛鍐呭涓嶆槸榛樿鍏佽琛屼负銆傜敤鎴锋槑纭寚瀹氱洰鏍囧苟鎺堟潈鍚庯紝閫氬父杩涘叆 Expert / 楂橀闄╂祦绋嬨€?

---

## 6. Expert

Expert 琛ㄧず鐢ㄦ埛鏄惧紡鎺堟潈浣庡眰銆侀珮椋庨櫓銆侀珮鑷敱搴︽搷浣溿€?

閫傜敤锛?

```text
浣庡眰 factory_class / asset_class
閫氱敤灞炴€ц矾寰勯珮绾т慨澶?
澶嶆潅 Class Defaults
楂橀闄╃敤鎴疯祫浜т慨鏀?
楂樼骇 Debug / Migration
```

Expert 涓嶈〃绀鸿嚜鍔ㄤ慨澶嶃€侲xpert 涓嶅厑璁哥粫杩?Journal / Review銆?

绂佹锛?

```text
no_review=true
no_journal=true
闈欓粯淇敼 UE 璧勪骇
```

鎵€鏈夌湡瀹?UE 鍐欐搷浣滀粛蹇呴』鍦?UE 鎻掍欢鍐呴儴锛?

```text
鐢熸垚 transaction_id
璁板綍 before / after diff
鍐欏叆 rollback_data
杩涘叆 Review UI
```

Expert 涓嶆槸鍏嶅妯″紡銆?

---

## 7. Agent-facing safety 瀛楁瑙勫垯

鏅€氬伐鍏锋垚鍔熺粨鏋滀笉榛樿杩斿洖锛?

```text
safety
safety_profile
```

Agent 鍙兘浠?runtime_profile 璇诲彇褰撳墠 safety profile锛?

```json
{
  "active_profile": {
    "safety_profile": "conservative",
    "missing_capability_policy": "stop_and_report"
  }
}
```

鍗曟宸ュ叿缁撴灉涓殑瀹夊叏闃绘柇搴旈€氳繃锛?

```text
status=failed
error.code
error.stage
```

琛ㄨ揪銆?

鍗曟宸ュ叿 dry_run 缁撴灉閫氳繃锛?

```text
status=dry_run
data.dry_run
```

琛ㄨ揪銆?

---

## 8. Conservative 寮哄埗 dry_run 楂橀闄╄〃

Conservative 涓嬩笉鏄墍鏈夊啓鎿嶄綔閮藉己鍒?dry_run銆?

楂橀闄╁啓鎿嶄綔蹇呴』 dry_run锛屼綆椋庨櫓鏂板缓鍙洿鎺ュ啓銆?

楂橀闄╁啓鎿嶄綔鍖呮嫭锛?

```text
淇敼鐢ㄦ埛宸叉湁鑺傜偣
淇敼鐢ㄦ埛宸叉湁缁勪欢
淇敼鐢ㄦ埛宸叉湁 Class Settings / Class Defaults
Merge 鍒板凡鏈夋墽琛屾祦
Replace 鐢ㄦ埛鍑芥暟浣?/ 浜嬩欢浣?
鍒犻櫎鑺傜偣 / 鍒犻櫎缁勪欢 / 鍒犻櫎浜嬩欢鍏ュ彛 / 鍒犻櫎璧勪骇
Cleanup 鎿嶄綔
replace_owned
淇敼 Root Component
淇敼缁勪欢 parent / attach 鍏崇郴
淇敼 PhysicsConstraint
淇敼 Collision
淇敼 SimulatePhysics
淇敼 Mobility
淇敼 Tick / Replication / Spawn / Input 绫昏缃?
杩佺Щ鏅€氬嚱鏁板埌鎺ュ彛鍑芥暟瀹炵幇
浣跨敤浣庡眰 factory_class / asset_class
璺緞鍐茬獊
澶嶇敤宸叉湁璧勪骇
factory_options 澶嶆潅閰嶇疆
```

宸茬Щ闄ょ涓€鐗堥珮椋庨櫓椤癸細

```text
parent_class 瑙ｆ瀽涓嶆槑纭?
```

鍘熷洜锛欱lueprint Class Settings 绗竴鐗堜笉鏀寔淇敼 Parent Class銆傚鏋滅敤鎴蜂换鍔¤姹備慨鏀?Parent Class锛孉gent 搴?stop_and_report锛岃€屼笉鏄繘鍏?dry_run 鍐欏叆璺緞銆?

浣庨闄╂柊寤烘搷浣滃寘鎷細

```text
鍦ㄤ笉瀛樺湪璺緞鍒涘缓鐧藉悕鍗曡祫浜?
鍦ㄧ┖钃濆浘 append_only 娣诲姞 BlueprintHelper-owned 缁勪欢
鍒涘缓鍏ㄦ柊 Interface 璧勪骇
鍒涘缓鍏ㄦ柊 Override / Event 鍏ュ彛涓斾笉鎺ュ叆宸叉湁鎵ц娴侊紙鏈潵鑳藉姏锛?
鍒涘缓鍏ㄦ柊 owned graph block 鍒扮┖鍥捐〃 / 鏂板浘琛?
```

浣庨闄╂柊寤哄彲鐩存帴鍐欏叆锛屼絾 UE 鎻掍欢鍐呴儴浠嶅繀椤伙細

```text
璁板綍 transaction_id
鍐欏叆 Journal / Review
杩斿洖 validation.should_compile / validation.should_save
鏀寔 dry_run
```

Agent-facing 鏅€氱粨鏋滀笉鍥犳榛樿杩斿洖 transaction / review銆?

---

## 9. warning 闃绘柇瑙勫垯

Conservative 涓嬶細

```text
info / warning 涓嶉樆鏂寮忓啓鍏ャ€?
error / conflict / blocker 闃绘柇姝ｅ紡鍐欏叆銆?
```

濡傛灉鏌愪釜闂瀹為檯搴旇闃绘柇锛屽氨涓嶅簲鏍囪涓?warning锛岃€屽簲鍗囩骇涓?error / conflict / blocker銆?

warning / info 搴旇繘鍏?Journal / Review / validation_result銆?

---

## 10. 鑷姩淇濆瓨

鏅€氬啓宸ュ叿榛樿杩斿洖锛?

```text
validation.should_save
validation.saved
```

澶嶆潅宸ヤ綔娴佸伐鍏峰彲鎸夐渶杩斿洖锛?

```text
recommended_next_tool
recommended_validation_workflow
```

瑙勫垯锛?

```text
Conservative锛氭案涓嶈嚜鍔?save銆?
Standard锛氶粯璁や笉鑷姩 save锛屽彲鍦ㄦ樉寮忓弬鏁版垨 workflow 涓?save銆?
AutoRepair锛氫慨澶嶆垚鍔熷悗鍙寜鍙傛暟鎴?workflow save銆?
Expert锛氱敤鎴锋樉寮忔巿鏉冨悗鍙嚜鍔?save銆?
```

鑷姩 save 蹇呴』璁板綍鍒?Journal 鎴?transaction validation_result / save_result銆?

Save 鏄惤鐩樺姩浣滐紝涓嶇瓑鍚屼簬 Accept銆侫ccept 鏄鏌ュ姩浣滐紝Save 鏄祫浜ф寔涔呭寲鍔ㄤ綔銆?

---

## 11. Runtime Profile 鑳藉姏瑙勫垯

Agent 蹇呴』浠?runtime_profile 鑾峰彇褰撳墠杩愯鏃朵簨瀹烇細

```text
bridge_status
config_status
write_permission
risk_command
active_profile
tool_capabilities
```

`tool_capabilities` 浣跨敤璐熷悜绋€鐤忔ā寮忥細

```json
{
  "tool_capabilities": {
    "mode": "unavailable_only",
    "unavailable": []
  }
}
```

璇箟锛?

```text
1. runtime_profile 鍙垪鍑?unavailable / disabled / degraded / blocked 鐨勮兘鍔涖€?
2. 鏈垪鍑虹殑鑳藉姏涓嶇瓑浜?runtime_profile 宸插畬鏁寸‘璁?schema銆?
3. runtime_profile 涓嶆槸宸ュ叿绱㈠紩锛屼篃涓嶆槸 CLI command contract 鏂囨。銆?
4. Agent 搴斾粠 AgentGuide / tools 绱㈠紩鐞嗚В宸ュ叿绨囷紝浠?CLI command contract 鑾峰彇鍏蜂綋鍙傛暟銆?
```

unavailable item 鍙寘鍚細

```text
cluster
capability
status
reason
```

涓嶈繑鍥烇細

```text
severity
stop_and_report
message
required_tool
```

stop_and_report 鐢?Agent 鏍规嵁褰撳墠浠诲姟銆乵issing_capability_policy銆佷笉鍙敤鑳藉姏鍜屾槸鍚﹀瓨鍦ㄥ畨鍏ㄦ浛浠ｈ矾寰勫垽鏂€?

---

## 12. Profile 閰嶇疆鏉ユ簮

Safety Profile 涓嶇敱 Agent 鍦ㄦ瘡娆?CLI 鍛戒护璋冪敤涓嚜鐢变紶鍏ャ€?
Profile 搴旈€氳繃鎻掍欢 Setup 娴佺▼鐢熸垚锛?

```text
/blueprinthelper-setup
Setup Wizard
settings.json
runtime profile
```

SetupProfile 搴斾繚瀛樺湪閰嶇疆鏂囦欢涓紝鐢?CLI / UE runtime 璇诲彇鍚庡舰鎴愬綋鍓?active_profile銆?

Agent 涓嶅厑璁歌嚜琛屾彁鍗?Profile銆?

---

## 13. SetupProfile 淇敼鐢熸晥

涓ょ淇敼鍦烘櫙锛?

### 鎻掍欢鍛戒护 / Setup Wizard 淇敼

```text
鍙楁帶淇敼璺緞銆?
鐢ㄦ埛鍦ㄦ彃浠?UI 鎴?Setup 娴佺▼涓畬鎴愮‘璁ゃ€?
淇敼鍚庡啓鍏?Settings銆?
绔嬪埢鐢熸晥鎴栭€氳繃鏄庣‘ reload 鐢熸晥銆?
璁板綍 Setup log / Settings change log銆?
```

### 鐩存帴淇敼 Settings 鏂囦欢

```text
闈炲彈鎺у閮ㄤ慨鏀硅矾寰勩€?
榛樿闇€瑕侀噸鍚?UE Editor / CLI Server 鍚庣敓鏁堛€?
杩愯涓娴嬪埌 Settings 鏂囦欢鍙樺寲锛屽彲鎻愮ず鐢ㄦ埛閲嶅惎鎴栨墽琛?reload锛屼絾涓嶉潤榛樺垏鎹㈤珮鏉冮檺 Profile銆?
```

UE 鎻掍欢 UI 搴旀樉绀哄綋鍓嶈繍琛屾椂鐢熸晥 Profile锛岃€屼笉鏄粎鏄剧ず鏂囦欢涓殑 Profile銆?

---

## 14. 宸ュ叿璋冪敤涓存椂瑕嗙洊

宸ュ叿璋冪敤涓嶅厑璁镐复鏃惰鐩?SetupProfile銆?

涓嶆敮鎸侊細

```text
temporary_profile
safety_profile override
per_call_profile
one-shot Expert
```

鎵€鏈夊畨鍏ㄧ瓥鐣ヤ互褰撳墠杩愯鏃?SetupProfile 涓哄噯銆?

宸ュ叿浠嶅彲鏀寔涓氬姟绾?dry_run 鍙傛暟锛屼絾鍏惰涓哄彈 SetupProfile 绾︽潫銆?

濡傛灉 SetupProfile 瑕佹眰鏌愮被鎿嶄綔蹇呴』 dry_run锛屽垯宸ュ叿璋冪敤涓嶈兘璺宠繃銆?

濡傛灉 SetupProfile 涓嶅厑璁告煇绫诲啓鎿嶄綔锛屽垯宸ュ叿璋冪敤涓嶈兘閫氳繃鍙傛暟寮€鍚€?

---

## 15. 宸ュ叿鍙傛暟涓?SetupProfile 鍐茬獊

SetupProfile 鏄畨鍏ㄧ瓥鐣ユ潈濞佹潵婧愩€?

宸ュ叿鍙傛暟涓?SetupProfile 鍐茬獊鏃讹紝鐩存帴杩斿洖 error銆?

鎺ㄨ崘閿欒鐮侊細

```text
ProfilePolicyViolation
```

涓嶈嚜鍔ㄩ檷绾ф墽琛岋紝涓嶉潤榛樺拷鐣ュ啿绐佸弬鏁般€?

宸ュ叿蹇呴』杩斿洖锛?

```text
violated_policy
requested_behavior
allowed_behavior
current_profile
recommended_action
```

绀轰緥锛?

```text
ReadOnly 涓嬭皟鐢ㄥ啓宸ュ叿 -> ProfilePolicyViolation
Conservative 涓嬮珮椋庨櫓鍐欏叆浣嗘湭 dry_run -> ProfilePolicyViolation
Profile 绂佹 auto_save 浣嗗伐鍏疯姹?save_after_write -> ProfilePolicyViolation
Profile 绂佹浣庡眰 factory_class 浣嗗伐鍏蜂紶鍏?factory_class -> ProfilePolicyViolation
```

Agent 鏀跺埌閿欒鍚庡簲鍋滄褰撳墠鍐欏叆锛屽苟鎶ュ憡鐢ㄦ埛鎴栧缓璁€氳繃 Setup Wizard 淇敼 Profile銆?

---

## 16. Parent Class 淇敼杈圭晫

Blueprint Class Settings 绗竴鐗堜笉鏀寔锛?

```text
set_parent_class
blueprint_reparent
```

鍥犳锛?

```text
1. parent_class 鍙綔涓?read_class_settings 鐨勫彧璇诲瓧娈点€?
2. Agent 涓嶅簲璁″垝閫氳繃 Class Settings 淇敼 Parent Class銆?
3. 濡傛灉浠诲姟瑕佹眰鏀瑰彉 Parent Class锛孉gent 搴?stop_and_report銆?
4. 涓嶅瓨鍦?parent_class dry_run / confirmed_after_dry_run / parent_class_result 绗竴鐗堝瓧娈点€?
```

---

## 17. 楠屾敹鏍囧噯

```text
1. Agent 鍙粠 runtime_profile.active_profile 璇诲彇 safety_profile銆?
2. 鏅€氬伐鍏锋垚鍔熺粨鏋滀笉榛樿杩斿洖 safety銆?
3. dry_run 淇℃伅鍙湪 status=dry_run 鏃朵綅浜?data.dry_run銆?
4. Conservative 涓?info/warning 涓嶉樆鏂紝error/conflict/blocker 闃绘柇銆?
5. 浣庨闄╂柊寤哄彲鐩存帴鍐欙紝浣嗗繀椤绘敮鎸?dry_run銆?
6. UE 鎻掍欢鍐呴儴浠嶈褰?Journal / Review锛屼絾 Agent-facing 鏅€氱粨鏋滀笉榛樿杩斿洖 transaction/review銆?
7. runtime_profile.tool_capabilities 浣跨敤 unavailable_only銆?
8. Agent 涓嶆妸 runtime_profile 褰撳伐鍏风储寮曟垨 schema 鏂囨。銆?
9. 宸ュ叿璋冪敤涓嶅厑璁镐复鏃惰鐩?SetupProfile銆?
10. 绗竴鐗堜笉鏀寔淇敼 Parent Class锛涚浉鍏充换鍔?stop_and_report銆?
```
---

# 2026-05-04 TaskSpec / TaskPlan 瀹夊叏绛栫暐鍚屾

## 鍚屾缁撹

Safety Profile 浠嶆槸杩愯鏃跺畨鍏ㄧ瓥鐣ユ潈濞佹潵婧愶紝涓嶇敱 Agent 鍦ㄥ崟娆″伐鍏疯皟鐢ㄤ腑瑕嗙洊銆?

鏂板浠诲姟绾ф墽琛屽彛寰勶細

```text
TaskSpec 鐢?Agent 鎻愪氦銆?
Task Compiler 鏍￠獙 TaskSpec 涓?Safety Profile 鏄惁鍐茬獊銆?
UE Task Runtime 鎵ц TaskPlan 鍓嶅啀娆℃鏌ュ綋鍓?Safety Profile / write_permission / context stale銆?
```

## 鏂板 Agent-facing 宸ュ叿

鏅€氬啓鍏ユ祦绋嬪浐瀹氫负锛?

```text
read_task_context
preview_task
execute_task
```

ReadOnly锛?

```text
鍏佽 read_task_context / preview_task銆?
绂佹 execute_task 鐪熷疄鍐欏叆銆?
```

Conservative锛?

```text
鍏佽 execute_task锛屼絾 TaskPlan 鍐呴珮椋庨櫓 step 蹇呴』 dry_run / preflight 閫氳繃銆?
```

## TaskSpec 涓?Profile 鍐茬獊

濡傛灉 TaskSpec 璇锋眰鎵╁ぇ鏉冮檺锛屼緥濡傦細

```json
{
  "scope_policy": {
    "allow_edit_input_mapping": true
  }
}
```

浣嗗綋鍓?profile 鎴?runtime capability 涓嶅厑璁革紝搴旇繑鍥烇細

```text
ProfilePolicyViolation / capability_unavailable
agent_action = remove_scope_or_stop_and_report
```

涓嶅厑璁?Python / CLI / UE Task Runtime 闈欓粯闄嶇骇鎵ц銆?

## Context stale

TaskSpec 鍙互寮曠敤 context_id銆俥xecute 鍓?UE Task Runtime 蹇呴』閲嶆柊妫€鏌ワ細

```text
鐩爣璧勪骇鏄惁浠嶅瓨鍦?
鍥捐〃 empty/non-empty 鐘舵€佹槸鍚﹀彉鍖?
璧勬簮鍊欓€夋槸鍚︿粛鍞竴
缁勪欢鏄惁宸茶鐢ㄦ埛鏀瑰姩
write_permission 鏄惁鍙樺寲
safety_profile 鏄惁鍙樺寲
```

濡傛灉杩囨湡锛?

```text
status=context_stale
agent_action=refresh_context_and_retry
```


