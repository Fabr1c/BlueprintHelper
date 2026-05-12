# 02 Blueprint Component Tools 璁捐鏂囨。锛堝凡鍚屾纭 Diff锛?

鏃ユ湡锛?026-05-03  
宸ュ叿绨囷細Blueprint Component Tools / 钃濆浘缁勪欢鏍戝伐鍏风皣  
鐘舵€侊細鍚屾纭 Diff 鍚庣殑淇鐗? 
鍚屾鑼冨洿锛歚add_component` 鑱岃矗鏀剁獎銆佺粍浠跺睘鎬ц缃繑鍥炴憳瑕併€乣name_collision` 璇箟銆佹櫘閫氬伐鍏蜂笉榛樿杩斿洖 transaction/review/safety銆?

---

## 0. 鏈鍚屾缁撹

鏈枃浠舵浛鎹㈡棫鐗堜腑浠ヤ笅杩囨湡鍙ｅ緞锛?

```text
1. add_component 涓嶅啀鎵挎媴 transform / mobility / collision / physics / mesh / material / constraint 鍙傛暟璁剧疆銆?
2. add_component 鍙礋璐ｅ垱寤虹粍浠跺拰寤虹珛 attachment銆?
3. 缁勪欢灞炴€у繀椤婚€氳繃 set_component_property / set_component_properties 璁剧疆銆?
4. 缁勪欢灞炴€у啓鍏ユ垚鍔熸椂鍙繑鍥?property_result 鎽樿锛屼笉鍥炴樉 before / after / all_properties銆?
5. name_collision 琛ㄧず缁勪欢鍛藉悕鍐茬獊锛屼笉鏄墿鐞?collision銆?
6. 绗竴鐗?name_collision 鍙敮鎸?fail_if_exists / reuse_if_exists銆?
7. 鏅€?Component 鎴愬姛缁撴灉涓嶉粯璁よ繑鍥?transaction / review / safety銆?
```

---

## 1. 瀹氫綅

Blueprint Component Tools 璐熻矗缂栬緫 Actor Blueprint 鐨勭粍浠舵爲锛屽嵆 UE 鐨?SCS / Component Template 灞傘€?

瀹冧笉灞炰簬 Graph Write锛屼笉閫氳繃 Append / Replace / Patch / Merge 缂栬緫缁勪欢鏍戯紝涔熶笉浣跨敤 `block_id`銆?

缁勪欢鏍戜慨鏀逛粛鏄?UE 鍐欐搷浣滐紝UE 鎻掍欢鍐呴儴蹇呴』鎺ュ叆 Transaction Journal / Review锛屼絾鏅€?Component 宸ュ叿缁撴灉涓嶉粯璁ゆ妸 `transaction / review / safety` 鏆撮湶缁?Agent銆?

---

## 2. 绗竴鐗堝伐鍏峰舰鎬?

绗竴鐗堝伐鍏峰缓璁敹鏁涗负锛?

```text
read_components
add_component
set_component_property
set_component_properties
remove_component
```

鍚庣画鍙墿灞曪細

```text
attach_component
detach_component
set_root_component
rename_component
cleanup_blueprint_helper_component_group
```

楂橀澶嶆潅缁勪欢閰嶇疆宸ュ叿鍙互浣滀负鍚庣画瀹夊叏灏佽锛屼絾绗竴鐗?Agent 瑙勫垯浠嶅簲閬靛畧锛?

```text
鍒涘缓缁勪欢 = add_component
璁剧疆灞炴€?= set_component_property / set_component_properties
```

---

## 3. add_component 鑱岃矗

`add_component` 鍙仛涓や欢浜嬶細

```text
1. 鍒涘缓缁勪欢銆?
2. 寤虹珛缁勪欢鎸傛帴鍏崇郴銆?
```

`add_component` 涓嶈缃細

```text
Transform
RelativeLocation / RelativeRotation / RelativeScale
Mobility
CollisionEnabled / CollisionProfileName / Collision Response
Physics / BodyInstance.bSimulatePhysics
StaticMesh
Material
PhysicsConstraint 鍙傛暟
浠绘剰缁勪欢灞炴€?
```

杩欎簺鍏ㄩ儴灞炰簬缁勪欢灞炴€т慨鏀癸紝蹇呴』浣跨敤锛?

```text
set_component_property
set_component_properties
```

---

## 4. add_component Agent-facing 杩斿洖

鎴愬姛绀轰緥锛?

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_component",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },
  "data": {
    "schema": "BlueprintHelper.BlueprintComponent.v1",
    "component": {
      "component_name": "DoorMesh",
      "component_class": "StaticMeshComponent",
      "created": true,
      "already_existed": false
    },
    "attachment": {
      "parent_component": "DefaultSceneRoot",
      "socket_name": null,
      "attach_rule": "keep_relative"
    },
    "name_collision": {
      "policy": "fail_if_exists",
      "handled": false
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

Agent 搴旂悊瑙ｏ細

```text
缁勪欢宸插垱寤哄苟鎸傛帴銆?
缁勪欢灞炴€у皻鏈厤缃€?
```

榛樿涓嶈繑鍥烇細

```text
transaction
review
safety
transform
properties
collision
physics
mesh
material
```

---

## 5. name_collision 瑙勫垯

瀛楁鍚嶅浐瀹氫负锛?

```text
name_collision
```

涓嶆槸锛?

```text
collision
```

鍘熷洜锛歚collision` 瀹规槗鍜岀墿鐞嗙鎾炶缃贩娣嗐€?

绗竴鐗堢瓥鐣ワ細

```text
fail_if_exists
reuse_if_exists
```

涓嶆敮鎸侊細

```text
auto_rename
replace_existing
```

Agent 涓嶅緱鑷姩鏀瑰悕鎴栨浛鎹㈠凡鏈夌粍浠躲€?

---

## 6. 缁勪欢灞炴€ц缃?

璋冪敤灞傚尯鍒嗭細

```text
set_component_property      鍗曚釜灞炴€т慨鏀?
set_component_properties    澶氫釜灞炴€т慨鏀?
```

杩斿洖灞傜粺涓€浣跨敤锛?

```text
data.property_result
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

鎴愬姛杩斿洖锛?

```json
{
  "property_result": {
    "mode": "batch",
    "requested_count": 4,
    "applied_count": 4,
    "changed_count": 3,
    "no_op_count": 1,
    "invalid_settings": []
  }
}
```

瀛楁瑙ｉ噴锛?

| 瀛楁 | 鍚箟 |
|---|---|
| `mode` | `single` 鎴?`batch`銆?|
| `requested_count` | 璇锋眰璁剧疆鏁伴噺銆?|
| `applied_count` | 瀹為檯搴旂敤鏁伴噺銆?|
| `changed_count` | 瀹為檯浜х敓鍙樺寲鐨勬暟閲忋€?|
| `no_op_count` | 宸插簲鐢ㄤ絾鍊兼湭鍙樺寲鐨勬暟閲忋€?|
| `invalid_settings` | 鏃犳晥璁剧疆鍒楄〃銆?|

---

## 7. 涓嶅洖鏄?property 蹇収

鎴愬姛鏃朵笉杩斿洖锛?

```text
before
after
all_properties
```

鍘熷洜锛?

```text
1. before / after 灞炰簬 UE 鍐呴儴 diff / Review / debug銆?
2. 澶у璞″睘鎬у洖鏄炬氮璐?Token銆?
3. 鎴愬姛缁撴灉鍙渶瑕佹墽琛屾憳瑕併€?
```

濡傛灉 Agent 闇€瑕佺‘璁ゆ渶缁堝睘鎬у€硷紝搴旇皟鐢ㄨ鍙栧伐鍏锋垨鏈潵涓撶敤缁勪欢灞炴€ц鍙栬兘鍔涳紝鑰屼笉鏄緷璧栧啓宸ュ叿鍥炴樉銆?

---

## 8. invalid_settings 瑙勫垯

鏃犳晥璁剧疆鍙嚭鐜板湪锛?

```text
data.property_result.invalid_settings
```

绀轰緥锛?

```json
{
  "property_path": "BodyInstance.bSimulatePhysics",
  "code": "property_not_writable",
  "expected_type": "bool"
}
```

甯歌 code锛?

```text
property_not_found
property_not_writable
type_mismatch
value_out_of_range
object_reference_not_found
enum_value_invalid
struct_field_invalid
component_not_found
component_type_mismatch
unsupported_property_type
```

Agent 搴旀牴鎹?`invalid_settings` 淇璁″垝鎴?stop_and_report銆?

---

## 9. 鎵归噺灞炴€т簨鍔¤鍒?

绗竴鐗堟壒閲忓睘鎬т慨鏀归粯璁や簨鍔″紡锛?

```text
鍙瀛樺湪 invalid_settings锛岄粯璁や笉搴旂敤浠讳綍灞炴€с€?
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

Agent 涓嶅緱鍦ㄦ壒閲忓け璐ュ悗鍋囪閮ㄥ垎灞炴€у凡缁忔垚鍔熷啓鍏ャ€?

---

## 10. 甯歌灞炴€ц矾寰勫綊灞?

浠ヤ笅閮藉睘浜庡睘鎬т慨鏀癸紝涓嶈兘娣峰叆 `add_component`锛?

```text
RelativeLocation
RelativeRotation
RelativeScale
Mobility
CollisionEnabled
CollisionProfileName
BodyInstance.bSimulatePhysics
StaticMesh
Material
PhysicsConstraint 鍙傛暟
```

蹇呴』浣跨敤锛?

```text
set_component_property
set_component_properties
```

---

## 11. Ownership 涓?component_group_id

缁勪欢 ownership 閲囩敤 Metadata + Journal 鍙屽啓銆?

瑙勫垯锛?

```text
涓嶄娇鐢?block_id銆?
缁勪欢 / SCS 鑺傜偣 / Component Template 鍐欏叆鏈€灏?ownership metadata銆?
Journal 璁板綍瀹屾暣 diff銆乺ollback_data銆佺粍浠剁粍鍏崇郴銆佸垱寤烘潵婧愬拰浜嬪姟鍘嗗彶銆?
涓嶄緷璧栫粍浠跺懡鍚嶇害瀹氬垽鏂?ownership銆?
涓嶅彧渚濊禆 Journal銆?
```

`component_group_id` 鍙敤浜庯細

```text
replace_owned
cleanup owned component group
Review 鍒嗙粍灞曠ず
Rollback 鍐茬獊妫€娴?
```

浣嗘櫘閫?Component 宸ュ叿鎴愬姛缁撴灉鏄惁鍚?Agent 鏆撮湶 `component_group_id` 搴旀寜宸ュ叿绨囬渶瑕佸喅瀹氾紝涓嶅簲涓庨€氱敤 `transaction / review / safety` 娣蜂负榛樿杩斿洖 envelope銆?

---

## 12. dry_run

鎵€鏈夌粍浠跺啓鎿嶄綔閮藉繀椤绘敮鎸?dry_run銆?

Conservative 涓嬮珮椋庨櫓缁勪欢鎿嶄綔蹇呴』 dry_run锛?

```text
set_root_component
reattach existing component
attach / modify user-owned component
replace_owned
remove_component
configure_physics_constraint
淇敼 SimulatePhysics / Collision / Mobility
淇敼缁勪欢 parent / root / constraint target
```

鏂板缓绌鸿摑鍥惧唴娣诲姞 BlueprintHelper-owned 缁勪欢锛屽彲浠ヤ笉寮哄埗 dry_run锛屼絾宸ュ叿浠嶅繀椤绘敮鎸?dry_run銆?

dry_run 缁撴灉浣嶇疆锛?

```text
status=dry_run
modified=false
data.dry_run
```

---

## 13. 鍒犻櫎涓庢竻鐞?

鍗曚釜缁勪欢绮剧‘鍒犻櫎锛?

```text
remove_component
```

瑙勫垯锛?

```text
component_name 蹇呴』鏄庣‘銆?
涓嶅厑璁告ā绯婂垹闄ゃ€?
涓嶅厑璁告寜 class 鎵归噺鍒犻櫎銆?
```

鍒犻櫎缁勪欢灞炰簬鍐欐搷浣溿€侫gent 搴斿湪鍒犻櫎鍓嶇‘璁ょ洰鏍囨槑纭紝蹇呰鏃跺厛 `read_components`銆?

owned 缁勪欢缁勬竻鐞嗙敱 Cleanup 宸ュ叿绨囪礋璐ｏ細

```text
cleanup_blueprint_helper_component_group
```

---

## 14. 涓?Graph Write 鐨勫叧绯?

```text
缁勪欢宸ュ叿涓嶅睘浜?Graph Write銆?
缁勪欢宸ュ叿涓嶄娇鐢?block_id銆?
缁勪欢鏍戜慨鏀逛粛鏄啓鎿嶄綔锛屽唴閮ㄥ繀椤绘帴鍏?Journal / Review銆?
Graph Write 涓嶇敤浜庡垱寤烘垨閰嶇疆缁勪欢鏍戙€?
```

---

## 15. 鐗╃悊闂ㄤ换鍔℃媶鍒嗙ず渚?

鐗╃悊闂ㄤ换鍔′腑锛孉gent 搴旀媶涓猴細

```text
1. add_component SceneRoot
2. add_component DoorMesh attach to SceneRoot
3. add_component InteractionBox attach to SceneRoot
4. add_component DoorConstraint attach to SceneRoot
5. set_component_properties DoorMesh: mesh / relative transform / collision / physics
6. set_component_properties DoorConstraint: constraint target / angular limits
7. 鍚庣画 Graph Write 鍐欎氦浜掗€昏緫
```

涓嶈鎶婄 5 姝ユ贩鍏?`add_component`銆?

---

## 16. Agent 绂佹琛屼负

Agent 涓嶅緱锛?

```text
1. 鐢?add_component 璁剧疆灞炴€с€?
2. 渚濊禆 add_component 杩斿洖 transform / properties銆?
3. 鑷姩鏀瑰悕缁勪欢銆?
4. 鑷姩鏇挎崲宸叉湁缁勪欢銆?
5. 鎶?successful property_result 褰撲綔瀹屾暣灞炴€у揩鐓с€?
6. 鍦ㄦ壒閲忓睘鎬уけ璐ユ椂鍋囪閮ㄥ垎灞炴€у凡搴旂敤銆?
7. 璺ㄥ伐鍏风皣鐢?Component 宸ュ叿鍐欏浘琛ㄩ€昏緫銆?
8. 鍦ㄦ渶缁堟姤鍛婁腑榛樿杈撳嚭 transaction_id 鎴?review_status銆?
```

---

## 17. 楠屾敹鏍囧噯

```text
1. Agent 鑳藉尯鍒?add_component 涓?set_component_property / set_component_properties銆?
2. add_component 杩斿洖 data.component / data.attachment / data.name_collision銆?
3. add_component 涓嶈繑鍥?transform / properties銆?
4. name_collision 涓嶈璇В涓虹墿鐞?collision銆?
5. set_component_property / set_component_properties 缁熶竴杩斿洖 data.property_result銆?
6. 鎴愬姛灞炴€у啓鍏ヤ笉鍥炴樉 before / after / all_properties銆?
7. invalid_settings 鏄敮涓€鏃犳晥璁剧疆鍒楄〃銆?
8. 鎵归噺灞炴€уけ璐ユ椂鏁翠綋澶辫触锛屼笉搴旂敤浠讳綍灞炴€с€?
9. 鏅€氭垚鍔熺粨鏋滀笉榛樿杩斿洖 transaction / review / safety銆?
10. Agent 鑳芥牴鎹?validation 缁х画 compile/save銆?
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

