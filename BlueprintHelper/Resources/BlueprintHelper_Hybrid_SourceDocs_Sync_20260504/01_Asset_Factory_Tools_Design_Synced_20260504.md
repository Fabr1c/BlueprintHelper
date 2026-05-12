# 01 Asset Factory Tools 璁捐鏂囨。锛堝凡鍚屾纭 Diff锛?

鏃ユ湡锛?026-05-03  
宸ュ叿绨囷細Asset Factory Tools / 璧勪骇鍒涘缓宸ュ叿绨? 
鐘舵€侊細鍚屾纭 Diff 鍚庣殑淇鐗? 
鍚屾鑼冨洿锛氬瓧娈靛崗璁敹鏁涖€佹櫘閫氬伐鍏蜂笉榛樿杩斿洖 transaction/review/safety銆丄sset Factory 鑱岃矗杈圭晫銆丄sset 瀛楁鍘诲啑浣欍€乿alidation 杩斿洖瑙勫垯銆?

---

## 0. 鏈鍚屾缁撹

鏈枃浠舵浛鎹㈡棫鐗堜腑浠ヤ笅杩囨湡鍙ｅ緞锛?

```text
1. 涓嶅啀瑕佹眰鏅€?Asset Factory 鎴愬姛缁撴灉榛樿杩斿洖 transaction_id / review / safety銆?
2. 涓嶅啀榛樿杩斿洖 asset_name銆?
3. 涓嶅啀榛樿杩斿洖 package_path銆?
4. Asset Factory 鍙垱寤鸿祫浜э紝涓嶆妸鎺ュ彛娣诲姞鍒拌摑鍥撅紝涔熶笉鍐欐帴鍙ｅ嚱鏁伴€昏緫銆?
5. Agent-facing 鎴愬姛缁撴灉浠?data.asset / data.factory / validation 涓轰富銆?
6. transaction / review / rollback_data 浠嶅彲鐢?UE 鎻掍欢鍐呴儴鍐欏叆 Journal / Review锛屼絾涓嶆槸鏅€?Asset Factory 宸ュ叿榛樿鏆撮湶缁?Agent 鐨勫瓧娈点€?
```

---

## 1. 瀹氫綅

Asset Factory Tools 璐熻矗閫氳繃 Unreal Editor 鐨?AssetTools / Factory 绯荤粺鍒涘缓 UE 璧勪骇銆?

瀹冧笉鏄櫘閫氭枃浠跺垱寤哄伐鍏凤紝涓嶅簲鍛藉悕涓?`create_file`銆傛墍鏈夊垱寤鸿涓洪兘搴旂悊瑙ｄ负 UE Editor 鍐呯殑 `.uasset` 璧勪骇鍒涘缓銆?

Asset Factory 鍙互璐熻矗鍒涘缓濡備笅璧勪骇绫诲瀷锛?

```text
Blueprint Class
Blueprint Interface
UserDefinedStruct
UserDefinedEnum
DataAsset
DataTable
WidgetBlueprint
InputAction锛堜粎鍦ㄥ綋鍓嶇増鏈?/ runtime profile 鏄庣‘鏀寔涓旂敤鎴风洰鏍囨槑纭椂锛?
```

Asset Factory 涓嶈礋璐ｏ細

```text
1. 灏?Blueprint Interface 娣诲姞鍒版煇涓?Blueprint 鐨?Implemented Interfaces銆?
2. 鍒涘缓鎺ュ彛鍑芥暟瀹炵幇鍥俱€?
3. 鍐欐帴鍙ｅ嚱鏁?body銆?
4. 灏嗘帴鍙ｅ嚱鏁版垨浜嬩欢鎺ュ叆 EventGraph銆?
5. 淇敼 InputMappingContext 涓殑鎸夐敭鏄犲皠銆?
6. 淇敼 C++ 婧愮爜鎴栭」鐩厤缃枃浠躲€?
```

瀹屾暣鎺ュ彛浜や簰宸ヤ綔娴佸繀椤绘媶鍒嗭細

```text
Asset Factory 鍒涘缓 BPI 璧勪骇
鈫?Blueprint Class Settings 娣诲姞 Implemented Interface
鈫?Graph Write 鍒涘缓鎴栧疄鐜版帴鍙ｅ嚱鏁伴€昏緫
鈫?Compile / Save
```

---

## 2. 宸ュ叿褰㈡€?

閲囩敤娣峰悎鏂规锛?

```text
asset_create = 缁熶竴 Asset Factory 鍏ュ彛
涓撶敤宸ュ叿 = 楂橀澶嶆潅璧勪骇鐨勫畨鍏ㄥ皝瑁?
```

鎺ㄨ崘涓撶敤宸ュ叿锛?

```text
blueprint_create_interface
widget_create_blueprint
asset_create_struct
asset_create_enum
asset_create_data_asset
asset_create_data_table
asset_create_blueprint_class
```

`input_create_action` / `input_create_mapping_context` 鏄惁鍙敤锛屽繀椤讳互 runtime profile 鍜?Enhanced Input 杈圭晫鏂囨。涓哄噯銆傚綋鍓嶉樁娈?Agent 涓嶅簲榛樿鑷姩鍒涘缓鎴栦慨鏀硅緭鍏ヨ祫浜э紝闄ら潪鐢ㄦ埛鏄庣‘瑕佹眰涓旇兘鍔涘彲鐢ㄣ€?

涓撶敤宸ュ叿涓嶆槸鍙︿竴濂楀疄鐜帮紝鍐呴儴浠嶈蛋 `asset_create` 鍙?UE 鎻掍欢渚у悓涓€濂?AssetTools / Factory 鍚庣銆?

杩斿洖涓彲浠ユ爣鏄庯細

```json
{
  "underlying_operation": "asset_create"
}
```

Agent 璋冪敤浼樺厛绾э細

```text
鏈変笓鐢ㄥ伐鍏锋椂锛屼紭鍏堜娇鐢ㄤ笓鐢ㄥ伐鍏枫€?
asset_create 浣滀负缁熶竴搴曞眰鍏ュ彛鍜屽悗澶囧伐鍏枫€?
```

---

## 3. asset_type 寮€鏀剧瓥鐣?

閲囩敤鍙屽眰妯″紡锛?

```text
榛樿锛氱櫧鍚嶅崟 asset_type銆?
楂樼骇锛欵xpert / 鍙楁帶 Profile 涓嬫墠鍏佽浣庡眰 asset_class / factory_class銆?
```

榛樿鐧藉悕鍗曡鐩栧凡楠岃瘉銆佸彲娴嬭瘯銆丄gent 楂橀浣跨敤鐨勮祫浜х被鍨嬶紝渚嬪锛?

```text
BlueprintClass
BlueprintInterface
UserDefinedStruct
UserDefinedEnum
DataAsset
DataTable
WidgetBlueprint
```

鏅€?Agent 涓嶇洿鎺ヤ娇鐢?`factory_class`锛岄伩鍏嶈浼?UE 绫诲悕鎴栫粫杩囧畨鍏ㄨ竟鐣屻€?

---

## 4. 宸插瓨鍦ㄨ祫浜у鐞?

榛樿锛?

```text
if_exists = error
```

鐩爣璺緞宸插瓨鍦ㄦ椂锛?

```text
涓嶈嚜鍔ㄨ鐩栥€?
涓嶈嚜鍔ㄩ噸鍛藉悕銆?
涓嶈嚜鍔?create_unique銆?
```

鍙湁鏄惧紡浼犲叆锛?

```text
reuse_if_type_matches
```

鏃讹紝鎵嶅厑璁稿鐢ㄥ凡鏈夊悓绫诲瀷璧勪骇銆?

瑙勫垯锛?

```text
鍚岀被鍨嬶細ok=true, status=no_op 鎴?reused, modified=false銆?
绫诲瀷涓嶄竴鑷达細error銆?
```

Conservative 涓嬪鐢ㄥ凡鏈夎祫浜у睘浜庨珮椋庨櫓鎴栬嚦灏戦渶 dry_run 鐨勮矾寰勶紝鍥犱负瀹冨彲鑳芥敼鍙樺悗缁?Agent 璁″垝鐨勭洰鏍囧綊灞炪€?

---

## 5. Agent-facing 鎴愬姛杩斿洖瀛楁

鏅€?Asset Factory 鎴愬姛缁撴灉榛樿浣跨敤绮剧畝 ToolResultBase锛?

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "asset_create",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/Input/IA_Interact",
    "target_type": "asset"
  },
  "data": {
    "schema": "BlueprintHelper.AssetFactory.v1",
    "asset": {
      "asset_path": "/Game/Input/IA_Interact",
      "asset_class": "InputAction",
      "asset_type": "input_action"
    },
    "factory": {
      "factory_type": "input_action"
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

榛樿涓嶈繑鍥烇細

```text
transaction
review
safety
asset_name
package_path
```

瀛楁瑙ｉ噴锛?

| 瀛楁 | 瑙勫垯 |
|---|---|
| `data.asset.asset_path` | Agent 渚т富瀹氫綅瀛楁銆?|
| `data.asset.asset_class` | 鍒涘缓鍑虹殑璧勪骇绫绘垨璧勬簮绫诲埆銆?|
| `data.asset.asset_type` | 鐧藉悕鍗曡涔夌被鍨嬨€?|
| `data.factory.factory_type` | 瀹為檯浣跨敤鐨?Factory 绫诲瀷鎽樿銆?|
| `data.factory.parent_class` | 浠?Blueprint Class 鍒涘缓绛夐渶瑕佺埗绫绘椂杩斿洖銆?|
| `validation.should_save` | 鍒涘缓璧勪骇鍚庨€氬父涓?true銆?|
| `validation.should_compile` | 鍙栧喅浜庤祫浜х被鍨嬨€?|

`asset_name` 涓嶉粯璁よ繑鍥烇紝鍥犱负鍙敱 `asset_path` 鎺ㄥ銆? 
`package_path` 涓嶉粯璁よ繑鍥烇紝鍥犱负浼氫笌 `asset_path` / object path 浜х敓鍐椾綑鎴栨涔夈€?

---

## 6. Blueprint Interface 鍒涘缓绀轰緥

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "blueprint_create_interface",
  "status": "applied",
  "modified": true,
  "data": {
    "schema": "BlueprintHelper.AssetFactory.v1",
    "asset": {
      "asset_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
      "asset_class": "BlueprintInterface",
      "asset_type": "blueprint_interface"
    },
    "factory": {
      "factory_type": "blueprint_interface"
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

鍒涘缓 BPI 璧勪骇鍚庯紝涓嶄唬琛ㄤ换浣?Blueprint 宸茬粡瀹炵幇璇ユ帴鍙ｃ€侫gent 鍚庣画蹇呴』鏄惧紡璋冪敤 Blueprint Class Settings 宸ュ叿娣诲姞 Implemented Interface銆?

---

## 7. 鍒涘缓鍚庤涓?

`asset_create` 榛樿鍙垱寤鸿祫浜э細

```text
涓嶈嚜鍔ㄦ墦寮€銆?
涓嶈嚜鍔ㄧ紪璇戯紝闄ら潪 workflow / profile 鏄庣‘鍏佽銆?
涓嶈嚜鍔ㄤ繚瀛橈紝闄ら潪 workflow / profile 鏄庣‘鍏佽銆?
```

Agent-facing 杩斿洖閫氳繃 `validation` 鍛婅瘔 Agent 鍚庣画闂幆锛?

```text
should_compile
should_save
compiled
saved
```

鏃犻渶缂栬瘧璧勪骇杩斿洖锛?

```text
should_compile=false
```

渚嬪 DataTable銆丏ataAsset 绛夈€?

Blueprint Class銆丅lueprint Interface銆乄idget Blueprint 绛夊彲杩斿洖锛?

```text
should_compile=true
```

鏄惁鑷姩 compile / save 鐢?Safety Profile銆亀orkflow 鍙傛暟鍜屽綋鍓?runtime profile 鍐冲畾銆?

---

## 8. dry_run

鎵€鏈?Asset Factory 鍐欐搷浣滃繀椤绘敮鎸?`dry_run`銆?

dry_run 涓嶅垱寤虹湡瀹炶祫浜э紝鍙繑鍥炲垱寤鸿鍒掑拰棰勬缁撴灉銆?

杩斿洖浣嶇疆锛?

```text
status=dry_run
modified=false
data.dry_run
```

dry_run 绀轰緥锛?

```json
{
  "ok": true,
  "operation": "asset_create",
  "status": "dry_run",
  "modified": false,
  "data": {
    "schema": "BlueprintHelper.AssetFactoryDryRun.v1",
    "dry_run": {
      "would_create_asset": true,
      "asset_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
      "asset_type": "blueprint_interface",
      "resolved_factory": "BlueprintFactory",
      "resolved_asset_class": "BlueprintInterface",
      "parent_class": null,
      "name_conflict": false,
      "required_modules": [],
      "can_execute": true,
      "warnings": [],
      "errors": []
    }
  }
}
```

涓嶉粯璁よ繑鍥?`package_path`銆?

Conservative 涓嬶紝鍒涘缓鍏ㄦ柊涓嶅瓨鍦ㄨ矾寰勭殑鐧藉悕鍗曡祫浜у彲浠ヤ笉寮哄埗 dry_run锛屼絾宸ュ叿蹇呴』鏀寔 dry_run銆?

浠ヤ笅鎯呭喌蹇呴』 dry_run锛?

```text
鐩爣璺緞宸插瓨鍦ㄥ苟璇锋眰 reuse_if_type_matches
浣跨敤楂樼骇 asset_class / factory_class
鍒涘缓楂橀闄╄祫浜х被鍨?
璺緞鍐茬獊
parent_class 瑙ｆ瀽涓嶆槑纭?
factory_options 鍖呭惈澶嶆潅閰嶇疆
```

dry_run 鎴愬姛涓嶇瓑浜庢寮忓垱寤恒€傛寮忓垱寤烘椂蹇呴』閲嶆柊妫€鏌ュ綋鍓嶈祫浜х姸鎬侊紝閬垮厤 TOCTOU 闂銆?

---

## 9. Journal / Review 杈圭晫

鎵€鏈夎祫浜у垱寤洪兘灞炰簬 UE 鍐欐搷浣滐紝UE 鎻掍欢鍐呴儴蹇呴』鎺ュ叆锛?

```text
Transaction Journal
Review Store
rollback_data
```

浣嗘櫘閫?Asset Factory Agent-facing 鎴愬姛缁撴灉涓嶉粯璁よ繑鍥烇細

```text
transaction
review
safety
```

杩欎簺灞炰簬鍐呴儴瀹¤ / Review UI / rollback 宸ヤ綔娴併€傚彧鏈夎皟璇曘€佸け璐ュ畾浣嶃€乺ollback 鎴栧悗缁繀椤诲紩鐢ㄦ椂锛岀浉鍏冲伐鍏锋墠鎸夐渶鏆撮湶蹇呰鎽樿銆?

璧勪骇鍒涘缓涓嶄娇鐢?`block_id`銆?

璧勪骇鍒涘缓 rollback 閲囩敤鏉′欢鍒犻櫎锛屽叿浣撹鍒欒 Transaction / Journal / Review 鏂囨。銆?

---

## 10. 鏈€灏忓伐鍏?Schema 鑽夋

```ts
asset_create({
  asset_path: string,
  asset_type: string,
  parent_class?: string,
  factory_options?: object,
  if_exists?: "error" | "reuse_if_type_matches",
  dry_run?: boolean
})
```

楂樼骇瀛楁浠呭湪 Expert / 鍙楁帶 Profile 涓嬪厑璁革細

```ts
asset_create({
  asset_path: string,
  asset_class?: string,
  factory_class?: string,
  factory_options?: object
})
```

濡傛灉 Profile 涓嶅厑璁镐綆灞傚瓧娈碉紝杩斿洖锛?

```text
ProfilePolicyViolation
```

---

## 11. Agent 绂佹琛屼负

Agent 涓嶅緱锛?

```text
1. 鎶?Asset Factory 鍒涘缓 BPI 璇涓虹洰鏍?Blueprint 宸插疄鐜拌鎺ュ彛銆?
2. 鏈熷緟 asset_name / package_path 榛樿杩斿洖銆?
3. 鏈熷緟鏅€?Asset Factory 缁撴灉榛樿杩斿洖 transaction_id / review_status銆?
4. 鍦ㄦ渶缁堟姤鍛婁腑榛樿杈撳嚭 transaction_id 鎴?review_status銆?
5. 鐢?Asset Factory 淇敼钃濆浘 Class Settings銆?
6. 鐢?Asset Factory 鍐欐帴鍙ｅ嚱鏁?body 鎴?EventGraph 閫昏緫銆?
7. 鏈粡 runtime profile / user target 纭鑷姩鍒涘缓鎴栦慨鏀硅緭鍏ユ槧灏勮祫浜с€?
```

---

## 12. 楠屾敹鏍囧噯

```text
1. 鎴愬姛鍒涘缓璧勪骇鏃惰繑鍥?data.asset.asset_path / asset_class / asset_type銆?
2. 鎴愬姛鍒涘缓璧勪骇鏃惰繑鍥?data.factory.factory_type銆?
3. asset_name 涓嶉粯璁よ繑鍥炪€?
4. package_path 涓嶉粯璁よ繑鍥炪€?
5. 鏅€氭垚鍔熺粨鏋滀笉榛樿杩斿洖 transaction / review / safety銆?
6. dry_run 缁撴灉鍙湪 status=dry_run 鏃惰繑鍥?data.dry_run銆?
7. validation 鑳芥纭弽鏄?should_compile / should_save銆?
8. Blueprint Interface 鍒涘缓涓嶈嚜鍔ㄦ坊鍔犲埌 Blueprint銆?
9. Asset Factory 涓嶅啓鎺ュ彛鍑芥暟閫昏緫銆?
10. Asset Factory 鍐呴儴鍐?Journal / Review锛屼絾 Agent 榛樿涓嶆秷璐硅繖浜涘唴閮ㄥ瓧娈点€?
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

