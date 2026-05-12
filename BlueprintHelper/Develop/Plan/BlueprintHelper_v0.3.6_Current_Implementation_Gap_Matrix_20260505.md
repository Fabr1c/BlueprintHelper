# BlueprintHelper v0.3.6 鑳藉姏绨囦笌褰撳墠瀹炵幇宸窛鐭╅樀

鏃ユ湡锛?026-05-05

2026-05-09 娓呯悊: 鏈枃浠朵繚鐣?capability gap 鐭╅樀鍜岃璁″緟鍔炪€傚綋鍓?grouped Automation銆丳1/P2 fixture銆丷eviewPanel銆丏ebugBundle 鍏ㄧ嚎楠岃瘉鍏ュ彛宸茬粺涓€鍒?`Develop/Plan/BlueprintHelper_Unified_SmokeRun_Verification_20260509.md`銆?

鏈枃鐢ㄤ簬鎶?`Develop/v0.3.6/DoneImplementaion` 涓?`Develop/v0.3.6/FieldMapping` 涓凡鏀舵暃鐨?UE 鑳藉姏璁捐锛屽鐓у綋鍓嶆簮鐮佸疄鐜扮姸鎬併€傜洰鏍囦笉鏄洖鍒?Agent 鐩磋皟鍘熷瓙宸ュ叿锛岃€屾槸纭畾鍝簺鑳藉姏宸茬粡鑳借繘鍏ュ綋鍓嶄富鏋舵瀯锛?

```text
Agent TaskSpec
-> CLI Task Commands
-> task-core / Python Task Compiler
-> UE Task Runtime
-> Existing UE Capability Clusters
```

## 2026-05-06 Rerun 4 鍚屾

- [x] GraphWrite Level 5 宸蹭粠婧愮爜寰呴獙璇佹帹杩涘埌 smoke verified銆?
- [x] `replace_owned_graph` 宸查獙璇?Python compiler -> Bridge preview -> Bridge execute -> compile -> LogicMd/LogicJson read-back銆?
- [x] Replace relink 宸查獙璇侊細preserved entry -> replacement body exec link 閲嶅缓鍚?read-back 涓?0 orphans銆?
- [x] Replace ownership metadata 宸查獙璇侊細Replace 鏂板缓鑺傜偣鐨勬満鍣?ownership 瀛楁鍐欏叆 `FMetaData`锛岃繘鍏?grouped LogicJson锛屽苟鍙 Patch/Merge 閫氳繃 `block_id` 瀹氫綅銆?
- [x] `patch_owned_graph` 宸查獙璇佸彲 patch Replace-created node銆?
- [x] `merge_owned_graph` 宸查獙璇?`insert_between + function_call`銆乣append_after + function_call`銆乣insert_between + custom_event_call`銆?
- [x] LogicJson grouped output 宸查獙璇佽緭鍑?`block_id`銆乣group_entry_node_path`銆佺粍鍐?`node_ref`銆乣pin_ref`銆乣link_ref`銆?
- [x] Ownership metadata / `NodeComment` 杩佺Щ杈圭晫宸插浐瀹氾細鏂板啓鍏ヤ笉鍐嶅悜 `NodeComment` 鍐?`block_id` / `tx`锛涙満鍣ㄥ瓧娈电粺涓€鍐?`FMetaData`锛沗NodeComment` 涓殑 `block_id` 鍙綔涓烘棫璧勪骇 fallback銆?
- [x] AgentGuide 宸茶ˉ Rerun 4 璇曢敊鏆撮湶鐨勪笁绫昏鍒欙細task tool 鍏ュ弬蹇呴』鍖?`task_spec`锛汳erge anchor 涓嶅厑璁稿彧浼?`link_ref`锛涘嚱鏁拌皟鐢ㄥ弬鏁板繀椤讳娇鐢ㄧ粨鏋勫寲 `args`銆?

## 2026-05-07 P1 Remaining Gap Smoke 鍚屾

- [x] `append_after + custom_event_call` 棰勮閿欒宸插彲璇婃柇锛歱review blocked 杩斿洖 `anchor_exec_pin_already_connected`锛屽甫 message 鍜?path锛屼笉鍐嶆槸绌洪敊璇€?
- [x] `branch_fork` 宸叉湁 UE smoke fixture锛歱review 閫氳繃 TaskSpec -> Python compiler -> TaskPlan -> Bridge -> UE preview锛宍capability=graph_write`锛宍insert_flow` 缁撴瀯鍖?IR 姝ｇ‘銆?
- [x] `branch_fork` execute / empty-error source fix 宸查泦鎴愶細MCP/Bridge 绌洪敊璇綊涓€鍖栧凡琛ワ紝UE MergeService `owned_block_call` 鐜板湪浼氳В鏋愬凡鏈?BlueprintHelper-owned CustomEvent block 骞剁敓鎴?call node 鍚庡啀搴旂敤 `branch_fork`銆?
- [x] R4 宸查獙璇?`branch_fork + custom_event_call` execute/read-back锛歋equence 鍒涘缓鎴愬姛锛宨nserted call 涓?original successor 鍧囧彲杈俱€?
- [ ] 鍚?graph `branch_fork + owned_block_call` 浠嶉渶琛ヤ竴鏉?execute/read-back smoke锛岄伩鍏?Level 3 鍙 `custom_event_call` 瑕嗙洊銆?
- [ ] UMGWidget / DataTable 浠?blocked_by_fixture锛歚WBP_WidgetSmoke`銆乣DT_DataTableSmoke` 缂哄け锛涙簮鐮佸凡琛?Structure fields銆丏ataTable `row_struct`銆乄idgetBlueprint 鍒涘缓璺緞锛岀姸鎬佷负 source integrated / smoke pending銆?
- [x] fixture 鍒涘缓璺緞鐨勬櫘閫?Blueprint `create_asset` 闂宸茶ˉ source fix锛歚asset_type=Actor` / `asset_type=blueprint` 褰掍竴涓?`blueprint_class + parent_class=Actor`锛沗create_blueprint_feature` preview 绌洪敊璇粛闇€鍚庣画褰掍竴鍖栥€?

褰撳墠浠嶄笉瑙嗕负 P1 瀹屽叏娓呯┖鐨勮竟鐣岄」锛?

- [x] `append_after + custom_event_call` preview 绌洪敊璇凡淇涓哄彲璇婃柇 blocker锛沞xecute 浠呭湪 preview 閫氳繃鏃跺啀楠岃瘉銆?
- [x] `branch_fork + custom_event_call` merge strategy 宸插畬鎴?R4 execute/read-back smoke銆?
- [ ] `branch_fork + owned_block_call` 浠嶉渶鍚?graph execute/read-back smoke銆?
- [ ] UMGWidget / DataTable 浠嶇己 disposable fixture execute smoke锛汚ssetFactory 鐩稿叧婧愮爜宸茶ˉ锛岀敤鎴蜂晶 UE build 宸查€氳繃锛屼笅涓€姝ユ寜 Unified SmokeRun Ring 3 楠岃瘉銆?
- [ ] TaskRunJournal partial failure / topology blocking 浠嶇己 controlled failure fixture銆?
- [ ] runtime profile 涓殑 GraphWrite merge/journal/review/store 鑳藉姏鏍囪鍙兘婊炲悗浜庡疄闄呮墽琛岃兘鍔涳紝闇€瑕佸崟鐙悓姝ャ€?

## 鐘舵€佹爣璁?

| 鏍囪 | 鍚箟 |
| --- | --- |
| 瀹屾垚 | 褰撳墠灞傚凡鍏峰鍙敤瀹炵幇锛屽苟涓斿懡浠ゆ垨鍏ュ彛宸叉帴閫?|
| 閮ㄥ垎 | 鍙畬鎴愪簡 DTO銆丼ervice銆丅ridge銆丷untime 鎴栨祴璇曚腑鐨勪竴閮ㄥ垎 |
| 缂哄け | 褰撳墠灞傛病鏈夊搴斿疄鐜?|
| 鍐呴儴 | 璁捐涓婁笉鏄?Agent 涓绘祦绋嬭兘鍔涳紝浠呬綔涓哄唴閮ㄦ敮鎾戙€乨ebug 鎴栧彧璇讳笂涓嬫枃 |

## 褰撳墠涓荤嚎缁撹

褰撳墠婧愮爜宸茬粡鍏峰杈冨搴曞眰 UE Service 鍜?Bridge command锛屼絾 TaskSpec-first 闂幆杩樻病鏈夎鐩栨墍鏈?v0.3.6 鑳藉姏銆?

褰撳墠 UE Task Runtime 鏀寔鐨?TaskPlan capability锛?

```text
graph_write
blueprint_variable
blueprint_signature
asset_factory
blueprint_component
blueprint_class_settings
umg_widget
data_table
object_property
graph_cleanup_ownership
```

褰撳墠 task-core / Python Task Compiler 鏀寔浠?TaskSpec 缂栬瘧鍑虹殑浠诲姟绫诲瀷锛?

```text
edit_blueprint_graph      -> graph_write锛屾敮鎸?append_new_owned_graph / replace_owned_graph / patch_owned_graph / merge_owned_graph
edit_blueprint_variables  -> blueprint_variable锛屾敮鎸?member changes/defaults/local variables 缂栬瘧涓虹粨鏋勫寲 IR
create_asset              -> asset_factory
edit_blueprint_components -> blueprint_component
edit_blueprint_class_settings -> blueprint_class_settings
edit_umg_widget           -> umg_widget
edit_data_table           -> data_table
edit_object_properties    -> object_property
manage_blueprinthelper_ownership -> graph_cleanup_ownership
create_blueprint_feature  -> composite compiler锛屽垎瑙ｄ负 blueprint_component / blueprint_variable / blueprint_class_settings / blueprint_signature / graph_write
```

浣?2026-05-05 smoke rerun 宸茬‘璁わ細**缂栬瘧鏀寔涓嶇瓑浜?execute 闂幆鍙敤锛宲review 閫氳繃涔熶笉绛変簬宸茬粡楠岃瘉鐪熷疄鍐欏叆**銆傚綋鍓嶇湡瀹炶窇閫氱殑 Agent-facing TaskSpec-first 鐘舵€佸垎灞傚涓嬶細

```text
execute 闂幆閫氳繃锛?
- edit_blueprint_graph + append_new_owned_graph + 鍏ㄦ柊鍥惧悕
- edit_blueprint_variables

preview 闂幆閫氳繃锛?
- create_asset
- edit_blueprint_components
- create_blueprint_feature
```

GraphWrite `replace_owned_graph` / `patch_owned_graph` / `merge_owned_graph` 鐨?TaskSpec 瀛愬瓧娈靛悎鍚屽凡缁忔敹鍙ｏ細replace 鍙娇鐢?`behavior.replace`锛宲atch 鍙娇鐢?`behavior.patches[]`锛宮erge 鍙娇鐢?`behavior.merges[]`銆俆S schema銆乀S fallback compiler銆丳ython compiler銆佸崗璁?fixtures銆佸悎鍚屽厓鏁版嵁涓?smoke 鏂囨。宸插悓姝ワ紱Agent 浠嶄笉搴旂洿鎺ヨ皟鐢ㄥ簳灞?MCP 鍘熷瓙鍐欏伐鍏凤紝榛樿鍏ュ彛浠嶆槸 TaskSpec -> TaskPlan -> UE Task Runtime銆?026-05-06 Rerun 4 宸茬‘璁?Level 5 GraphWrite full pipeline锛歊eplace 閫氳繃 compiler/preview/execute/compile/read-back锛孭atch 鍙畾浣嶅苟淇敼 Replace-created node锛孧erge 宸查獙璇?`insert_between + function_call`銆乣append_after + function_call`銆乣insert_between + custom_event_call`銆侾atch/Merge 涓荤嚎鍐欓敋鐐瑰浐瀹氫负 v0.3.6 grouped LogicJson / block-scoped anchor锛氱敱 `block_id` / `group_entry_node_path` 鍔犵粍鍐?`node_ref` / `pin_ref` / `link_ref` 瀹氫綅 BlueprintHelper-owned block 鍐呴儴鑺傜偣涓庡紩鑴氾紱GUID 鍙繚鐣欎负 expert/debug fallback銆侽wnership 鏈哄櫒瀛楁鐨勬柊鍐欏叆缁熶竴杩涘叆 `FMetaData`锛屼笉鍐嶅啓鍏?`NodeComment`锛沗NodeComment` 涓殑 `block_id` 浠呬繚鐣欎负 legacy fallback銆?

## 2026-05-05 / 2026-05-06 杩涘害鍚屾

- [x] UE Task Runtime 宸叉敮鎸佸 step 椤哄簭鎵ц锛屽苟鑱氬悎 child step result銆?
- [x] UE Task Runtime 宸叉寜 `execution_policy.should_compile` / `execution_policy.should_save` 鎵ц compile/save post operation銆?
- [x] TaskRunJournal 宸茶仛鍚?step result銆乸ost operation result锛屽苟鏀寔杩涚▼鍐呮煡璇€?
- [x] Blueprint Variable TaskPlan IR 宸叉敮鎸?`member_variables` / `member_defaults` / `local_variables` lowering銆?
- [x] Blueprint Variable ensure-only member batch 缁х画 lower 鍒?`add_blueprint_member_variables`銆?
- [x] Blueprint Variable mixed member/default/local ops lower 鍒板唴閮?`blueprint_variable_batch`锛屼笉鏆撮湶 adapter operation 缁?Agent銆?
- [x] Python/MCP TaskSpec 缂栬瘧宸茶鐩?`create_asset`銆乣edit_blueprint_components`銆乣edit_blueprint_class_settings`銆乣edit_umg_widget`銆乣edit_data_table`銆?
- [x] BlueprintVariableService 鐨?`set_member_variable_properties` 宸插畬鎴愰鐗囩湡瀹炴墽琛岋紝鏀寔 category/tooltip/instance_editable/expose_on_spawn銆?
- [x] BlueprintVariableService 鐨?`set_member_default(s)` 宸插畬鎴愰鐗囩湡瀹炴墽琛岋紝鍐欏叆 `FBPVariableDescription::DefaultValue` 骞惰繑鍥?ToolResultBase銆?
- [x] BlueprintVariableService 宸查噸鏂版敹鏁涗负 ToolResultBase fa莽ade / 缂栨帓灞傦紱member default 涓?member property mutation 缁嗚妭宸茶縼鍏?`FBlueprintHelperMemberVariableMutationHandler`銆?
- [x] `FBlueprintHelperMemberVariableMutationHandler` 宸叉敞鍐屽埌 `FBlueprintOperationHandlerRegistry`锛岃鐩?`set_member_default` / `set_member_defaults` / `set_member_variable_properties`銆?
- [x] MCP 鍥炲綊宸查€氳繃 `npm.cmd test`锛歂ode 106/106锛孭ython 30/30銆?
- [x] 11 绫诲伐鍏风皣鐩綍鍒嗙被宸插畬鎴愶紱婧愮爜 UTF-8/TEXT() 淇鍚庯紝鐢ㄦ埛鏈湴宸茬‘璁ら」鐩骇 `Build.bat` 閫氳繃锛坄Build.bat MrStoneEditor Win64 Development -Project=G:\UnrealPractise\MrStone\MrStone.uproject`锛夈€侰odex 娌欑洅澶嶈窇浼氳 MrStone 宸ョ▼绾?`Intermediate` 鍐欐潈闄愰檺鍒堕樆濉炪€?
- [x] BlueprintVariableService 鐨?local variable read/add/set/remove 宸叉帴鍏ョ湡瀹?Service/OperationHandler 璺緞锛沴ocal variable TaskPlan preview 璧扮湡瀹?dry-run锛屼笉鍐嶈蛋 synthetic preview銆?
- [x] Component / AssetFactory / Widget / DataTable / ClassSettings 宸蹭粠 Runtime synthetic preview 鍗囩骇涓烘湇鍔＄骇 true dry-run锛汿askPlan preview 浼氳皟鐢ㄥ搴?Service preflight锛屼絾涓嶄細杩涘叆瀹為檯 mutation/Modify/dirty 璺緞銆?
- [x] GraphWrite `replace_owned_graph` / `patch_owned_graph` / `merge_owned_graph` 宸插畬鎴?TaskSpec schema銆乀S fallback compiler銆丳ython Task Compiler銆佸崗璁?fixtures 鍜?MCP 鍥炲綊娴嬭瘯锛涚紪璇戠粨鏋滀粛鏄?compiler-owned `graph_write` structured IR锛屼笉鏆撮湶 `replace_blueprint_graph` / `patch_blueprint_graph` / `merge_blueprint_graph` 缁?Agent銆?
- [x] UE TaskRuntime 宸茶ˉ `replace_body` / `set_pin_default|set_node_comment|set_node_position` / `insert_flow` 鐨?structured IR lowering 婧愮爜涓?automation contract tests锛屽垎鍒?lower 鍒扮幇鏈?Replace/Patch/Merge capability cluster adapter payload锛涚敤鎴锋湰鍦板凡纭椤圭洰绾?Build.bat 閫氳繃銆?
- [x] Composite `create_blueprint_feature` 宸叉墿鍒?`integration.interface` 棣栫墖锛歍S schema銆乀S fallback compiler銆丳ython Task Compiler銆丮CP 鍥炲綊娴嬭瘯宸叉敮鎸佹妸涓€涓?Agent 璇箟 TaskSpec 鍒嗚В涓虹幇鏈?`blueprint_component` / `blueprint_variable` / `blueprint_class_settings` / `blueprint_signature` / `graph_write` TaskPlan steps銆俙integration.input` 宸叉寜褰撳墠鏋舵瀯纭瑁佸壀锛岀户缁樉寮忔嫆缁濓紱`allow_create_assets=true` 浠嶆嫆缁濓紝閬垮厤璧勪骇鍒涘缓琚潤榛樿烦杩囥€?
- [x] TaskSpec / TaskPlan 鎵ц璇箟宸茬‘璁わ細Agent 鍙啓灏戦噺璇箟椤跺眰 TaskSpec锛汿askPlan 鏄?compiler-owned 鍐呴儴 IR锛涙墽琛屽墠鍏?dry-run 鍏ㄩ儴姝ラ锛岄€氳繃鍚庨『搴?execute锛涗腑閫斿け璐ュ啓鍏?TaskRunJournal partial failure锛屽苟鎸?TaskPlan 鎷撴墤闃绘柇鍚庣画渚濊禆姝ラ锛屼笉榛樿鎵胯鍏ㄥ眬 rollback銆?
- [x] 2026-05-05 smoke rerun 宸茬‘璁や袱鏉″畬鏁?TaskSpec -> Execute 閾捐矾閫氳繃锛歚edit_blueprint_graph` 鐨?`append_new_owned_graph + 鏂板浘鍚峘锛坄task_5806121649296A709F32088EB10C55F0`锛夊拰 `edit_blueprint_variables`锛坄task_38C6DC0D4AC56E1DD89F4992D9A7B3AB`锛夈€?
- [x] 2026-05-05 smoke rerun 宸茬‘璁?`create_asset`銆乣edit_blueprint_components`銆乣create_blueprint_feature` 鐨?TaskSpec -> preview 闂幆閫氳繃锛汣omposite preview 鑳藉垎瑙ｄ负 component / variable / signature / graph_write 澶?step TaskPlan銆?
- [x] 2026-05-06 smoke rerun 宸茬‘璁?GraphWrite Replace/Patch/Merge 姝ｇ‘ TaskSpec shape锛歊eplace 閫氳繃 Python compiler銆丅ridge preview銆丅ridge execute銆乧ompile 鍘嗗彶鍏ㄩ摼璺紱Patch/Merge 閫氳繃 Python compiler锛屼絾褰撴椂 Bridge preview 琚棫 read ref / write anchor 涓嶅吋瀹归樆濉炪€?
- [x] LogicJson `target_type=custom_event` 鑷畾涔夊浘鏌ユ壘闂宸蹭慨澶嶅苟閫氳繃 smoke read-back锛汱ogicJson 鑳藉湪鑷畾涔夊浘涓畾浣?Custom Event銆?
- [ ] ClassSettings / UMGWidget / DataTable 浠嶇己 disposable fixture smoke锛?026-05-07 smoke 纭 fixture assets 缂哄け锛屾湭杩涘叆 execute銆?
- [x] GraphWrite Replace/Patch/Merge 瀛愬瓧娈靛悎鍚屽凡鍥哄畾锛汻eplace execute 宸查€氳繃锛汸atch/Merge 璇诲啓閿氱偣鍚堝悓宸插浐瀹氫负 grouped LogicJson / block-scoped anchor锛孡ogicJson 杈撳嚭銆乧ompiler lowering銆乁E block-scoped resolver 婧愮爜宸茶ˉ锛汻eplace body exec link 閲嶅缓婧愮爜涔熷凡琛ャ€備笅涓€姝ユ槸鏈湴 build/smoke 楠岃瘉銆?
- [x] P2 棣栨壒涓夌皣婧愮爜鎺ョ嚎宸插畬鎴愬埌 TaskSpec-first 涓荤嚎锛歚blueprint_signature`銆乣object_property`銆乣graph_cleanup_ownership` 鍧囨湁 TaskPlan adapter / Runtime dispatch锛沗edit_object_properties` 涓?`manage_blueprinthelper_ownership` 宸叉帴 TS/Python compiler銆俙blueprint_signature.remove_signature` 宸叉帴 TaskPlan preflight/blocked path锛屼絾涓嶆墽琛岀湡瀹炲垹闄ゃ€?
- [ ] P2 棣栨壒涓夌皣浠嶅緟缁熶竴 build銆乤utomation銆乨isposable fixture smoke锛涘綋鍓嶅彧鏍囪涓?source integrated锛屼笉鏍囪涓?smoke verified銆?

### 2026-05-05 / 2026-05-06 GraphWrite 鍚堝悓鏀跺彛琛ヨ

- [x] GraphWrite Replace/Patch/Merge TaskSpec 瀛愬瓧娈靛悎鍚屽凡鍥哄畾鍒?TS schema銆乀S fallback compiler銆丳ython compiler銆佸崗璁?fixture銆佸悎鍚屽厓鏁版嵁鍜?smoke 鏂囨。銆?
- [x] 瀛楁鍏ュ彛宸插浐瀹氫负锛歚replace_owned_graph -> behavior.replace`銆乣patch_owned_graph -> behavior.patches[]`銆乣merge_owned_graph -> behavior.merges[]`銆?
- [x] 宸茬姝㈡妸 Replace/Patch/Merge 濉炲洖 `behavior.entries` 鎴栭€氱敤 `ops`锛?026-05-06 smoke 宸茶瘉鏄庢纭?shape 鑳借繘鍏ュ搴?pipeline锛孊ridge resolver 涓?Replace exec link 琛屼负婧愮爜宸茶ˉ锛屽墿浣欐槸鏈湴 build/smoke 楠岃瘉銆?
- [x] Patch/Merge 鍐欓敋鐐瑰悎鍚屽凡鍥哄畾锛欱lueprintHelper-owned 鍐呭浼樺厛浣跨敤 `block_id` / `group_entry_node_path` 鍔犵粍鍐?`node_ref` / `pin_ref` / `link_ref`锛沗block_id` 瀹氫綅 owned block锛岀粍鍐?ref 閫夋嫨鍏蜂綋鑺傜偣/寮曡剼/杩炴帴锛涜８ `nodes[index]`銆佹樉绀哄悕鍜?GUID-first 涓嶄綔涓?Agent 涓荤嚎鍚堝悓锛孏UID 浠呬綔 expert/debug fallback銆?
- [x] Ownership metadata / NodeComment 杩佺Щ鍐崇瓥宸插浐瀹氾細鏂板啓鍏ュ彧鎶婃満鍣ㄥ瓧娈靛啓鍏?`FMetaData`锛涗笉鍐嶅湪 `NodeComment` 涓啓 `block_id` / `tx`锛涙棫娉ㄩ噴閲岀殑 `block_id` 鍙綔涓?legacy fallback銆?

## 鎬讳綋宸窛鐭╅樀

| 鑳藉姏绨?| v0.3.6 鏉ユ簮 | UE DTO/Structure | UE Service | Bridge command | UE Task Runtime | Python/MCP TaskSpec | 褰撳墠鐘舵€?| 涓嬩竴姝?|
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ToolResultBase/CommonEnvelope | Done + FieldMapping | 瀹屾垚锛屽凡杩佸埌 `Structure` | 鍐呴儴 builder 瀹屾垚 | 閫氳繃鍚?command 杩斿洖 | 琚?Runtime 澶嶇敤 | MCP 渚т粛鏈?normalize 灞?| 瀹屾垚 | 淇濇寔涓虹粺涓€杩斿洖鍗忚锛屼笉鍐嶄负鍗曠皣鑷畾涔夊澹?|
| TaskRuntime core | 鏂版灦鏋勬枃妗?| TaskPlan/validation 浣跨敤 JSON | `TaskRuntime` 瀹屾垚椤哄簭鎵ц鍣?| `preview_task_plan` / `execute_task_plan` / `get_task_run_journal` | 瀹屾垚澶?step銆乧ompile/save post operation銆佸唴瀛?TaskRunJournal 鑱氬悎 | MCP 浠诲姟宸ュ叿宸叉帴 Python | 瀹屾垚鍩虹闂幆 | 鍚庣画琛?TOCTOU銆佹寔涔?journal銆乸review blocker 涓板瘜鍖?|
| Composite Blueprint Feature | 鏂版灦鏋勬枃妗?| TaskSpec schema 宸叉帴 `create_blueprint_feature` | 澶嶇敤鐜版湁 Service | 澶嶇敤 `preview_task_plan` / `execute_task_plan` | 澶嶇敤澶?step Runtime锛屾柊澧?`blueprint_signature/ensure_function` 棣栫墖 | 瀹屾垚 components/variables/class_settings/behavior/interface integration 鍒嗚В鍒扮幇鏈?capability steps | preview smoke passed / execute pending | 琛?disposable execute fixture锛岄獙璇?component / variable / signature / graph_write 澶?step 鍐欏叆缁撴灉 |
| GraphWrite Append | Done + FieldMapping | 瀹屾垚 | `AppendBlueprintGraphService` 瀹屾垚 | `append_blueprint_graph` 瀹屾垚 | 瀹屾垚锛宍ensure_entry(custom_event)` structured IR lowering | 瀹屾垚 `append_new_owned_graph` | execute smoke passed锛歚append_new_owned_graph + 鏂板浘鍚峘 | 鎵╁睍鏇村 entry/statement锛屼笉鏀瑰彉 TaskPlan 涓虹粨鏋勫寲 IR 鐨勬柟鍚?|
| GraphWrite Replace | Done + FieldMapping | 瀹屾垚 | `ReplaceBlueprintGraphService` 瀹屾垚锛沺reserved entry -> replacement body relink 涓?ownership metadata 宸查獙璇?| `replace_blueprint_graph` 瀹屾垚 | `replace_body` -> replace adapter lowering 宸查獙璇?| 瀹屾垚 `replace_owned_graph` TaskSpec 缂栬瘧 | smoke verified full pipeline | 淇濇寔 owned-block 绾︽潫锛涚户缁ˉ闈?owned anchor 鍐崇瓥 |
| GraphWrite Patch | Done + FieldMapping | 瀹屾垚 | `PatchBlueprintGraphService` 瀹屾垚锛沚lock-scoped resolver 宸查獙璇佸彲瀹氫綅 Replace-created node | `patch_blueprint_graph` 瀹屾垚 | `set_pin_default` / `set_node_comment` / `set_node_position` -> patch adapter lowering 宸查獙璇侀鐗?| 瀹屾垚 `patch_owned_graph` TaskSpec 缂栬瘧 | smoke verified on owned block | 鎵╂洿澶?patch fixture锛涢潪 owned anchor 鍙﹁鍐崇瓥 |
| GraphWrite Merge | Done + FieldMapping | 瀹屾垚 | `MergeBlueprintGraphService` 瀹屾垚锛沚lock-scoped anchor resolver 涓?insert flow 棣栫墖宸查獙璇侊紱`branch_fork + owned_block_call` source fix 宸茶ˉ锛沗append_new_owned_graph` dependent append 澶嶇敤 signature-created entry 婧愮爜宸茶ˉ | `merge_blueprint_graph` 瀹屾垚 | `insert_flow` -> merge adapter lowering 宸查獙璇?`insert_between` / `append_after` 棣栫墖 | 瀹屾垚 `merge_owned_graph` TaskSpec 缂栬瘧 | smoke verified for supported owned-block strategies锛汻4 宸查獙璇?`branch_fork + custom_event_call` execute/read-back锛涜法鍥?`owned_block_call` preview 姝ｇ‘ blocked锛泂ame-graph `owned_block_call` source patched / smoke pending | 琛ュ悓 graph `branch_fork + owned_block_call` execute/read-back smoke锛沗append_after + custom_event_call` preview 宸插彲璇婃柇 |
| Cleanup BlueprintHelper Block | Done + FieldMapping | 瀹屾垚 | `CleanupBlueprintHelperBlockService` 瀹屾垚 | `cleanup_blueprint_helper_block` 瀹屾垚 | 宸叉帴 `graph_cleanup_ownership` adapter / Runtime dispatch | 宸叉帴 `manage_blueprinthelper_ownership` compiler | source integrated / smoke pending | 缁熶竴 smoke 鍚庡啀鏍囪瀹屾垚锛涗繚鎸?internal TaskPlan capability锛屼笉鏂板 Agent-facing 鍘熷瓙鍐欏伐鍏?|
| Rollback Cleanup Transaction | Done + FieldMapping | 瀹屾垚 | `RollbackCleanupTransactionService` 瀹屾垚 | `rollback_cleanup_transaction` 瀹屾垚 | 宸叉帴 `graph_cleanup_ownership` adapter / Runtime dispatch | 宸叉帴 `manage_blueprinthelper_ownership` compiler | source integrated / smoke pending | 浣滀负 task rollback/journal 鑳藉姏鎺ュ叆 Runtime锛屼笉浣滀负鏅€氬啓鍏ラ粯璁ゆ楠?|
| Convert Block To User Owned | Done + FieldMapping | 瀹屾垚 | `ConvertBlockToUserOwnedService` 瀹屾垚 | `convert_blueprint_helper_block_to_user_owned` 瀹屾垚 | 宸叉帴 `graph_cleanup_ownership` adapter / Runtime dispatch | 宸叉帴 `manage_blueprinthelper_ownership` compiler | source integrated / smoke pending | 缁熶竴 smoke 楠岃瘉鍚庡啀鎵╅珮椋庨櫓 replace/remove 鍓嶇疆浣跨敤 |
| Blueprint Variables/Defaults/Local Variables | Done | 瀹屾垚 | `BlueprintVariableService` 宸叉敮鎸?member add/remove銆乵ember property settings 棣栫墖銆乵ember default(s) 棣栫墖锛屼互鍙?local variable read/add/set/remove锛沵ember/local mutation 缁嗚妭宸茶縼鍏?OperationHandler锛孲ervice 淇濇寔 ToolResultBase fa莽ade | 鍙橀噺鐩稿叧 command 瀹屾垚 | 瀹屾垚鍙橀噺 IR lowering锛歟nsure-only -> `add_blueprint_member_variables`锛屾贩鍚?member/default/local -> `blueprint_variable_batch`锛沴ocal_variables preview 鏀寔鐪熷疄 dry-run | 瀹屾垚 TaskSpec 缂栬瘧锛歮ember changes/defaults/local variables | smoke-verified锛歚edit_blueprint_variables` execute | 鎵╅粯璁ゅ€煎拰灞炴€ц缃洿澶氱被鍨嬶紱琛ユ洿澶?UE automation/smoke 瑕嗙洊锛涚敤鎴锋湰鍦伴」鐩骇 `Build.bat` 宸查€氳繃锛涙渶杩戦獙璇?task id锛歚task_38C6DC0D4AC56E1DD89F4992D9A7B3AB` |
| Function/Event Signature Management | Plan 鏂囨。 | 宸叉柊澧?`Structure/BlueprintSignature` DTO 棣栫墖锛汿askPlan 宸叉湁 `blueprint_signature` | 宸叉柊澧炲唴閮?`FBlueprintHelperSignatureService` 棣栫墖锛歚ensure_function` dry-run/no-op/execute 涓?inputs/outputs锛沗ensure_custom_event` 宸叉湁鍏ュ彛鍒涘缓棣栫墖锛沗ensure_event_dispatcher` 鍙€氳繃鍐呴儴缁撴瀯鏈嶅姟鍒涘缓鏂?dispatcher锛岀幇闃舵鍙厑璁?`signature_mismatch_policy=block`锛沗ensure_override_event` 榛樿 blocked锛屾樉寮?`execute_policy=create_if_missing` 宸叉湁婧愮爜鍒涘缓璺緞锛況emove-signature 浠?blocked | 鏃?Agent-facing 鍘熷瓙 command锛涗粎 TaskRuntime 鍐呴儴鎵ц | Runtime 宸插鎵?SignatureService 鎵ц `blueprint_signature` step锛屽苟鏀寔 `ensure_function` / `ensure_custom_event` / `ensure_event_dispatcher` / `ensure_override_event` / `remove_signature` lowering锛沬nterface function/event 宸叉媶鍒嗭紱`custom_event_definition` 宸叉媶鎴?Signature declaration + GraphWrite body rewrite锛沷verride create-if-missing 宸叉帴鍏ワ紱remove 浠嶈繑鍥?blocked preflight | `integration.interface` 涓?`custom_event_definition` 鍙紪璇戝埌 `blueprint_signature` + `graph_write replace_body` | source integrated / smoke pending | 琛ョ湡瀹?remove 鎵ц涓?dispatcher 杩佺Щ绛栫暐锛涙柊 Signature 婧愮爜璺緞寰?build / automation / smoke 鍚庡啀鏍?verified |
| AssetFactory | FieldMapping | 瀹屾垚 | `AssetFactoryService` 瀹屾垚锛屾敮鎸?dry-run 鍐茬獊/鍒涘缓棰勬涓斾笉鍒涘缓璧勪骇锛涙簮鐮佸凡琛?Structure fields銆丏ataTable `row_struct`銆乄idgetBlueprint 鍒涘缓鍒嗘敮 | `create_asset` 瀹屾垚 | 瀹屾垚 adapter锛屾敮鎸?`asset_factory/asset_create/create_asset`锛沺review 璋?Service true dry-run | 瀹屾垚 `create_asset` TaskSpec 缂栬瘧锛涘凡琛?`data_table/datatable`銆乣widget_blueprint/widgetblueprint/widget` aliases | compiler-ready / preview smoke covered / new asset types source integrated | 鍚庣画琛?Structure/DataTable/WidgetBlueprint execute smoke锛屽啀鎵?Material 绛夎祫浜х被鍨?|
| AssetDiscovery/EditorNavigation | Done + FieldMapping | 瀹屾垚 | `AssetBrowseService` 瀹屾垚 | `list_assets` / `search_assets` / `open_asset` / `get_asset_info` 瀹屾垚 | 涓嶉渶瑕侀粯璁ゅ啓鍏?Runtime | 鍚庣画缁?`ReadSpec` / `read_context` 杩涘叆鍙涓婁笅鏂?| 閮ㄥ垎 | 淇濇寔鍙/瀵艰埅鑳藉姏锛屼絾涓嶆墿鏁ｆ垚澶?Agent-facing 鍘熷瓙宸ュ叿 |
| ProjectContext/SetupState | Done + FieldMapping | 绫诲瀷瀛樺湪 | `ContextService` 鍩虹瀛樺湪 | `get_editor_context` 绛夊叆鍙ｅ瓨鍦?| 涓嶅睘浜庡啓 Runtime | `read_task_context` 褰撳墠瀹氫綅涓嶆竻锛屾爣璁?deprecated锛涘悗缁粡 `read_context` 閲嶅畾涔?| 閮ㄥ垎 | 鍚堝苟鍒?ReadSpec/CapabilitySchema锛屼笉淇濈暀妯＄硦鐙珛鍏ュ彛 |
| RuntimeProfile | Done + FieldMapping | 瀹屾垚 | `RuntimeProfileService` 瀹屾垚 | `get_runtime_profile` 瀹屾垚 | 涓嶅睘浜庡啓 Runtime | MCP 榛樿宸ュ叿宸叉湁 runtime profile | 瀹屾垚 | 淇濇寔 Agent preflight 鍙鍏ュ彛 |
| Diagnostics | Done + FieldMapping | 瀹屾垚 | `DiagnosticsService` 瀹屾垚 | `diagnostics_runtime` 瀹屾垚 | 鏈 TaskRuntime 鑷姩鎵ц | MCP 榛樿宸ュ叿宸叉湁 diagnostics | 閮ㄥ垎 | TaskRuntime 鏍规嵁 execution_policy 澧炲姞 diagnostics 闃舵 |
| CompileBlueprintAsset | Done + FieldMapping | 瀹屾垚 | `CompileAssetService` 瀹屾垚 | `compile_blueprint_asset` 瀹屾垚 | 鍙妸 `should_compile` 鍐欏叆 validation锛屼笉瀹為檯璋冪敤 compile | 缂哄け | 閮ㄥ垎 | TaskRuntime 鎵ц鏈熬鎸?`execution_policy.should_compile` 璋冪敤 |
| SaveAsset | Done + FieldMapping | 绫诲瀷瀛樺湪 | Bridge 鍐呯洿鎺ュ疄鐜?| `save_asset` 瀹屾垚 | 鍙妸 `should_save` 鍐欏叆 validation锛屼笉瀹為檯淇濆瓨 | 缂哄け | 閮ㄥ垎 | TaskRuntime 鎵ц鏈熬鎸?`execution_policy.should_save` 璋冪敤 |
| EditorLifecycle/RiskCommand | Done + FieldMapping | 瀹屾垚 | `EditorCommandService` 瀹屾垚 | undo/redo/PIE/close/console 瀹屾垚 | 涓嶅簲榛樿杩涘叆鍐?Runtime | 缂哄け | 鍐呴儴/debug | `open_editor` / `close_editor` 淇濈暀骞惰縼绉诲埌 `blueprinthelper_*` 鍓嶇紑锛涘叏灞€ undo/redo 浠庨粯璁ゅ伐鍏烽泦涓Щ闄わ紝鍚庣画鏀瑰仛 transaction 绾?undo/redo |
| DebugCase / DebugBundle | Debug 绯荤粺鏋舵瀯 | DebugCase / DebugBundleManifest v1 宸叉垚涓哄綋鍓?developer diagnostics 鍙ｅ緞锛涙棫 DebugExport / LargePayload 涓嶅啀鏄?active Agent-facing contract | DebugCaseStore / DebugEntry / Review debug linkage 宸叉湁棣栫墖 | `get_debug_case` summary-only锛汥ebugBundle 浠嶆槸鏈湴寮€鍙戣€呭鍑鸿竟鐣?| TaskRuntime / Review / Transaction failure paths 宸叉帴鍏ラ鐗?| Agent 閫氳繃鍧楃骇 `logic_md` / `logic_json` 璇诲彇瀹氫綅锛涘け璐ュ彧鏆撮湶 `debug_case_ids[]` summary ref锛汥ebugBundle 涓嶇粡 MCP 浼犺緭 artifact | source integrated / verification tail pending | ReviewPanel 鐜板満楠岃瘉銆丏ebugBundle Review summary 杈圭晫銆乧ompile/post-operation failure debug surfacing銆乺etention / cleanup policy |
| DataAsset/Object Property | Done + FieldMapping | 绫诲瀷瀛樺湪 | `PropertyReflectionService` 瀹屾垚閫氱敤 UObject 灞炴€ц鍐欙紝骞舵柊澧?ToolResultBase fa莽ade / true dry-run 鎵归噺璁剧疆棣栫墖 | `get_object_properties` / `set_object_property` 瀹屾垚 | 宸叉帴 `object_property/property_edit` TaskPlan adapter / Runtime dispatch | 宸叉帴 `edit_object_properties` TS/Python compiler | source integrated / smoke pending | 缁熶竴 smoke 鍚庢墿鏇村畬鏁?value 绫诲瀷銆佸祵濂楄矾寰勫拰 DataAsset fixture |
| DataTable | Done + FieldMapping | 瀹屾垚 | `DataTableService` 瀹屾垚锛宎dd/update/delete row 鏀寔 true dry-run | get/add/update/delete row 瀹屾垚 | 瀹屾垚 adapter锛屾敮鎸?add/update/delete row锛沺review 璋?Service true dry-run | 瀹屾垚 `edit_data_table` TaskSpec 缂栬瘧 | compiler-ready / fixture smoke pending | 纭 read 琛屼负浠嶅彧璇伙紝涓嶆贩鍏ュ啓 TaskPlan锛涜ˉ disposable fixture smoke锛涙墿鏇村畬鏁?row schema/field 绫诲瀷瑕嗙洊 |
| UMG WidgetBlueprint | Done + FieldMapping | 瀹屾垚 | `WidgetService` 瀹屾垚锛宎dd/set_property/remove 鏀寔 true dry-run | get/add/remove/move/get_properties/set_property 瀹屾垚 | 閮ㄥ垎 adapter锛屾敮鎸?add/set_property/remove锛屼笉鏀寔 move/read锛沺review 璋?Service true dry-run | 瀹屾垚 `edit_umg_widget` TaskSpec 缂栬瘧锛屼笉鏀寔 move_widget | compiler-ready / fixture smoke pending | Runtime adapter 鎵?move_widget 鎴栦繚鎸佹槑纭笉鏀寔锛涜ˉ disposable WidgetBlueprint fixture smoke |
| Blueprint Component | FieldMapping | 褰撳墠缁撴瀯鍦?Service header 鍐咃紝鏈畬鍏ㄦ媶鍒?Structure | `ComponentService` 瀹屾垚锛屽凡缁熶竴 ToolResultBase锛宎dd/set/remove 鏀寔 true dry-run | read/add/set/remove command 瀹屾垚 | 閮ㄥ垎 adapter锛屾敮鎸?add/set_properties/remove锛沺review 璋?Service true dry-run | 瀹屾垚 `edit_blueprint_components` TaskSpec 缂栬瘧 | preview smoke passed / execute pending | 琛?component execute smoke锛涙妸 component DTO 杩涗竴姝ヨ縼鍒?Structure |
| Blueprint Class Settings | FieldMapping | 瀹屾垚 | `ClassSettingsService` 瀹屾垚锛宨nterface/default property 鍐欏叆鏀寔 true dry-run | read/add/remove interface/set class defaults 瀹屾垚 | 閮ㄥ垎 adapter锛屾敮鎸?interface/default property锛屼笉鏀寔 reparent锛沺review 璋?Service true dry-run | 瀹屾垚 `edit_blueprint_class_settings` TaskSpec 缂栬瘧锛宺eparent 鏄庣‘鎷掔粷 | compiler-ready / fixture smoke pending | reparent 浣滀负 future 鎴栧苟鍏?Function/Event/Class signature 鑳藉姏锛涜ˉ interface/default property disposable fixture smoke |
| Internal Dependency Analysis / Reference Context | Done | 瀹屾垚 | `Safety/DependencyAnalysisService` 閮ㄥ垎瀹屾垚 | `read_reference_context` 瀹屾垚 | 涓嶅睘浜庨粯璁ゅ啓 Runtime | MCP 鍙宸ュ叿宸插瓨鍦?| 閮ㄥ垎/鍐呴儴 | 淇濇寔 Agent 鍙寮曠敤鏌ョ湅鍣紱鍚庣画璁╅珮椋庨櫓 remove/replace preview 鍙紩鐢ㄥ叾 summary |
| LogicMD/LogicJson Read | FieldMapping + 鏋舵瀯鏂囨。 | 瀹屾垚 | `Logic` 灞傚畬鎴?| read logic md/json command 瀹屾垚 | 涓嶅睘浜庡啓 Runtime | 鐢ㄤ簬涓婁笅鏂?璋冭瘯锛屼繚鐣欎负 Agent 鍙閫昏緫鍏ュ彛 | 鍙/TaskSpec 杈呭姪锛汱ogicJson custom_event 鑷畾涔夊浘璇诲洖宸蹭慨澶?| LogicMD 淇濇寔 v0.3.6 閫昏緫淇℃伅鏍峰紡涓斾笉鎼哄甫 TaskSpec draft锛汱ogicJson 闇€瑕佽緭鍑?grouped block 淇℃伅浠ユ敮鎸?block-scoped write anchor |
| TransactionJournalQuery / Review transaction records | Done + FieldMapping | 瀹屾垚 | `Transactions` 灞傚畬鎴?query锛孯eview Store 鐙睘鐢ㄦ埛渚?| list/read transaction command 瀹屾垚 | TaskRunJournal 鐩墠鏄崟鐙唴瀛?journal | 缂哄け | 閮ㄥ垎 | 涓?Review 鑱氬悎鎴愬畬鏁存寔涔?Review 璁板綍浜嬪姟锛涙秷璐?UE 鍐欎簨鍔″拰 task_run_id 鍒嗙粍锛屼笉浣滀负 Agent-facing Review/ReviewPanel 宸ュ叿 |

## 褰撳墠 Runtime 鑳藉姏涓?v0.3.6 鐨勪富瑕佷笉涓€鑷?

1. **[x] TaskRuntime 澶?step 鍩虹宸茶ˉ榻愩€?*
   褰撳墠 `preview_task_plan` / `execute_task_plan` 宸茶兘椤哄簭鎵ц澶氫釜 TaskPlan step锛屽苟鑱氬悎 step result銆傚墿浣欓棶棰樻槸 TOCTOU銆侀槻閲嶅叆銆佷互鍙婇暱鏈熸寔涔呭寲 journal銆?

2. **[x] `execution_policy.should_compile` / `execution_policy.should_save` 鍩虹鎵ц宸茶ˉ榻愩€?*
   Runtime 宸插湪闈?dry-run 鎵ц鏈熬璋冪敤 `compile_blueprint_asset` / `save_asset` post operation锛屽苟鍐欏叆 runtime data 涓?TaskRunJournal銆傚悗缁渶瑕佽ˉ鏇寸粏鐨勫け璐ユ仮澶嶅拰 TOCTOU 澶勭悊銆?

3. **[x] P1 Python Compiler 瑕嗙洊宸茶ˉ榻愰鐗囥€?*
   Python/MCP TaskSpec 宸茶鐩?`asset_factory`銆乣blueprint_component`銆乣blueprint_class_settings`銆乣umg_widget`銆乣data_table`锛屽苟淇濇寔 Agent-facing 瀛楁涓鸿涔夊眰锛屼笉鏆撮湶 adapter operation銆?

4. **[x] P1 adapter dry-run 宸插崌绾т负鏈嶅姟绾?true dry-run銆?*
   AssetFactory銆丅lueprintComponent銆丅lueprintClassSettings銆乁MGWidget銆丏ataTable 鐨?TaskPlan adapter 宸叉爣璁?true dry-run 鏀寔锛屽苟鍦?preview 鏃惰皟鐢ㄥ搴?Service preflight銆俤ry-run 璺緞浼氳В鏋愮洰鏍囪祫浜с€佺被/鎺ュ彛/灞炴€?row/widget/component 绛夋墽琛屽墠缃潯浠讹紝浣嗕笉浼氳繘鍏?`FBlueprintHelperScopedAssetMutation`銆乣Modify`銆佸疄闄?Add/Remove/ImportText 鍐欏叆銆丮arkBlueprint銆丄ssetRegistry 鍒涘缓鎴?DataTable row mutation 璺緞銆俁untime synthetic preview 浠嶄繚鐣欑粰灏氭湭瀹屾垚 true dry-run 鐨勫叾浠?adapter銆?

5. **P2 鏂扮皣宸茶繘鍏?source integrated 闃舵锛屼絾鏈粺涓€楠岃瘉銆?*
   BlueprintVariableService 鐨?member property settings銆乵ember defaults銆乴ocal variables 宸插畬鎴愰鐗囩湡瀹炴墽琛岋紝涓斿疄闄?mutation 宸茶縼鍏?OperationHandler锛汧unction/Event Signature Management銆丏ataAsset/ObjectProperty銆丆leanup/Rollback/Ownership 宸叉帴鍏?TaskSpec -> TaskPlan -> Runtime 婧愮爜璺緞銆侱ebugCase / DebugBundle 宸叉浛浠ｆ棫 DebugExport LargePayload 浣滀负褰撳墠 developer diagnostics 鏂瑰悜锛涙壒閲忎笂涓嬫枃寮曠敤涓嶅啀浣滀负褰撳墠 Agent-facing 涓荤嚎鏂瑰悜锛汸2 棣栨壒涓夌皣闇€瑕佷笅涓€杞粺涓€ automation銆乨isposable fixture smoke 鍚庢墠鑳芥爣璁颁负 verified銆?
6. **UE 鏋勫缓楠岃瘉鐘舵€併€?*
   鍚庣画缁熶竴浣跨敤椤圭洰绾?`Build.bat`銆傛簮鐮?UTF-8/TEXT() 淇鍚庯紝鐢ㄦ埛鏈湴宸茬‘璁?`Build.bat MrStoneEditor Win64 Development -Project=G:\UnrealPractise\MrStone\MrStone.uproject` 鏋勫缓閫氳繃銆?026-05-09 璧峰綋鍓嶄富绾夸笉鍐嶆寜 Codex 鐜 build blocked 鍙ｅ緞鎺ㄨ繘锛涘墿浣欑姸鎬佹槸 grouped Automation 鍜?disposable fixture smoke 鏈叏缁裤€?

## 浼樺厛绾у缓璁?

### P0锛歊untime 闂幆鍩虹

1. [x] TaskRuntime 鏀寔澶?step 椤哄簭鎵ц銆?
2. [x] TaskRuntime 鎵ц `execution_policy.should_compile` / `execution_policy.should_save`銆?
3. [x] TaskRunJournal 鍚堝苟 child result銆乿alidation銆乧ompile/save 缁撴灉銆?
4. [ ] Preview blocked 鏃惰繑鍥炴洿鍙鐨?blockers锛屽苟鍙紩鐢?`ReferenceContextPack`銆?

杩欎簺鏄墍鏈夎兘鍔涚皣鍏卞悓渚濊禆锛屼笉搴旀帹杩熷埌鍗曚釜鑳藉姏鍚庨潰銆?

### P1锛氳ˉ Python/MCP TaskSpec 缂栬瘧瑕嗙洊

浼樺厛缁欏凡缁?TaskPlan-ready 鐨?UE capability 琛?TaskSpec锛?

1. [x] `asset_factory`
2. [x] `blueprint_component`
3. [x] `blueprint_class_settings`
4. [x] `umg_widget`
5. [x] `data_table`
6. [x] `blueprint_variable` 鐨?set/remove/default/local TaskSpec 涓?Runtime lowering
7. [x] GraphWrite replace/patch/merge TaskSpec compiler 涓?structured IR lowering 婧愮爜
8. [x] `create_blueprint_feature` composite compiler 棣栫墖锛氭妸涓€涓姛鑳界骇 TaskSpec 鍒嗚В鍒板凡鎺ュ叆鐨勭幇鏈?capability steps

杩欐壒涓嶉渶瑕佸厛鍐欏ぇ閲?UE 鏂拌兘鍔涳紝涓昏琛?TaskSpec schema銆丳ython compiler銆乀S schema/test 涓?MCP preview/execute contract銆傝繖閲岀殑 `[x]` 琛ㄧず compiler/contract/source 棣栫墖宸茶ˉ榻愶紝涓嶇瓑鍚屼簬鍏ㄩ儴 UE execute smoke 宸查€氳繃锛涘綋鍓嶇湡瀹?smoke-verified execute 闂幆鍙湁 `edit_blueprint_graph + append_new_owned_graph + 鏂板浘鍚峘 涓?`edit_blueprint_variables`銆?

### P2锛氭墿 UE 鏂拌兘鍔涚皣

1. [x] Function/Event Signature Management 棣栫墖锛氬唴閮?service銆丏TO銆乪nsure_function銆乪nsure_custom_event entry 鍒涘缓銆乪nsure_event_dispatcher 鏂板缓澹版槑銆乪nsure_override_event blocked preflight銆乺emove_signature blocked preflight銆丷untime delegation銆?
2. [x] DataAsset/ObjectProperty 棣栫墖锛歍askSpec schema銆乀S/Python compiler銆乀askPlan adapter銆乀oolResultBase fa莽ade銆丷untime dispatch銆?
3. [x] Cleanup/Rollback/Ownership 棣栫墖锛歍askSpec schema銆乀S/Python compiler銆乀askPlan adapter銆丷untime dispatch 鍒?cleanup / convert / rollback service銆?
4. [ ] P2 棣栨壒涓夌皣缁熶竴 build銆乤utomation銆乨isposable fixture smoke銆?
5. [x] Signature 杈圭晫鎵╁睍锛氬嚱鏁板弬鏁般€佽繑鍥炲€笺€乮nterface function vs interface event銆乪vent dispatcher signature mutation policy銆乷verride/native event default blocked policy銆乷verride/native explicit create-if-missing source path銆乧ustom_event_definition split銆乺emove execute policy 宸插浐瀹氬埌 TaskSpec/TaskPlan 涓?UE 婧愮爜棣栫墖銆?
6. [x] DebugCase / DebugBundle developer diagnostics 棣栫墖锛氬け璐ヨ矾寰勪娇鐢?`debug_case_ids[]` summary ref锛孌ebugBundle 鏄湰鍦板紑鍙戣€呭鍑虹墿锛屼笉鍐嶈蛋鏃?DebugExport LargePayload 鍙ｅ緞銆?
7. [ ] Debug diagnostics verification tail锛歊eviewPanel 鐜板満楠岃瘉銆丏ebugBundle Review summary export 杈圭晫銆乧ompile/post-operation failure debug surfacing銆乺etention / cleanup policy銆?
7. [ ] DependencyAnalysis 涓庨珮椋庨櫓 preview 鐨勯泦鎴愩€?

## 鍚庣画璁ㄨ寰呭姙

1. [x] Agent-facing MCP 榛樿宸ュ叿闆嗗悎鏈€缁堝喕缁擄細`blueprinthelper_read_agent_guide`銆乣blueprint_get_runtime_profile`銆乣blueprinthelper_diagnostics`銆乣blueprinthelper_read_context`銆乣blueprinthelper_read_reference_context`銆乣blueprinthelper_preview_task`銆乣blueprinthelper_execute_task`銆乣blueprinthelper_get_task_result`銆乣blueprinthelper_open_editor`銆乣blueprinthelper_close_editor`銆?
2. [x] 鏃у師瀛愬伐鍏峰鐞嗙瓥鐣ョ‘璁わ細宸插疄鐜?TaskPlan adapter + TaskSpec compiler 瑕嗙洊鐨勮兘鍔涗紭鍏堢Щ闄ゆ棫 Agent-facing 鍘熷瓙鍛戒护锛涙湭瑕嗙洊鑳藉姏鏆備繚鐣欎负 legacy/internal/debug/expert/test锛岀瓑 adapter 涓?TaskSpec 鏀寔钀藉湴鏃跺悓姝ョЩ闄ゃ€?3. [x] 杩斿洖浣撳垎灞傛渶缁堝喕缁擄細`blueprinthelper_read_agent_guide` 杩斿洖 Markdown锛涘叾浠栭粯璁よ/浠诲姟宸ュ叿浣跨敤 `BlueprintHelper.McpToolResult.v1` 澶栧３锛沗read_context` -> `ReadContextPack.v1`锛宍read_reference_context` -> `ReferenceContextPack.v1`锛宍preview_task` -> `TaskPreviewResult.v1`锛宍execute_task` -> `TaskRunSummary.v1` 鎴?`TaskRunJournal.v1`锛宍get_task_result` -> `TaskRunJournal.v1`锛沀E fa莽ade 缁熶竴 `FBlueprintHelperToolResultBase`锛涙櫘閫?Agent 涓嶈蛋鎵归噺涓婁笅鏂囧紩鐢紝寮€鍙戣瘖鏂蛋 DebugCase / DebugBundle summary/export 杈圭晫銆?
4. [x] TaskRuntime partial failure 鎷撴墤闃绘柇鍚堝悓锛歍askPlan step 浣跨敤 `steps[].depends_on` 琛ㄨ揪渚濊禆锛汿askRunJournal step status 鍥哄畾涓?`completed|failed|blocked|skipped`锛沚locked step 浣跨敤 `blocked_by_step_ids` / `blocked_reason`锛沺artial failure 浣跨敤 `recovery.recommended_action`銆乣safe_to_retry`銆乣rollback_available`銆乣notes` 缁欏嚭鐢ㄦ埛鍙鎭㈠寤鸿锛涗笉榛樿鍏ㄥ眬 rollback銆?
5. [ ] Function/Event Signature 鎵╁睍瀛楁鍚堝悓锛氱户缁ˉ鐪熷疄 remove 寮曠敤娓呯悊绛栫暐銆乨ispatcher 绛惧悕杩佺Щ绛栫暐锛屼互鍙?override/native create-if-missing 鐨?UE automation / smoke 楠岃瘉璁板綍銆?
6. [x] LogicJson 涓?TaskSpec 缁勫悎璇箟锛氬凡纭 `logic_json` 涓嶈繑鍥?`taskspec_hints`锛屼繚鎸佸彧璇荤粨鏋勫寲閫昏緫瑙嗗浘锛涘叾浠栫粍鍚堣涔夊悗缁崟鐙璁°€?
7. [x] LogicJson reference 鏄犲皠杈圭晫锛氬凡纭 `node_ref` / `link_ref` 涓嶈兘榛樿涓?TaskSpec patch/merge selector 鍏煎锛涘畠浠彧鏄?read-view references銆?
8. [ ] ReadSpec 閫氱敤璇诲眰鍚堝悓锛歚BlueprintHelper.ReadSpec.v1`銆乣blueprinthelper_read_context` 鐨勪富绾垮畾浣嶅凡鍩烘湰纭锛涜兘鍔涢潰鍙戠幇涓嶅啀璁捐杩愯鏃?schema 鏌ヨ宸ュ叿锛屾敼鐢?`blueprinthelper_read_agent_guide` 杩斿洖 AgentGuide 绱㈠紩锛屽啀鐢?AgentGuide 鏂囦欢鎵胯浇鍏蜂綋鏍煎紡銆?
9. [x] 閫氱敤璇绘牸寮忥細宸茬‘璁?`logic_md` / `logic_json` 鍥哄畾涓烘墍鏈夊彲閫傞厤 read capability 鐨勯€氱敤 view format锛涢粯璁や娇鐢?`logic_md` 鑺傜渷 token锛屽彧鏈夌簿纭畾浣嶃€乨iff銆乸atch/merge/debug 鏃朵娇鐢?`logic_json`锛沗summary` 鐢ㄤ簬浣?token 鍒濈瓫锛宍schema` 鐢ㄤ簬瀛楁璇存槑涓斾笉璇诲彇璧勪骇姝ｆ枃銆?
10. [x] 绉婚櫎杩愯鏃惰兘鍔?schema 鏌ヨ宸ュ叿鏂瑰悜锛氳兘鍔涢潰閫氳繃鏂囨。鍜?AgentGuide 鑾峰緱锛屽叿浣撴牸寮忚繘鍏?AgentGuide 鏂囦欢澶广€?
11. [x] Rule markdown 宸ュ叿鏀瑰悕锛氫粠 `blueprint_get_rule_markdown` 杩佺Щ鍒?`blueprinthelper_read_agent_guide`锛涜宸ュ叿杩斿洖 `Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md`锛屼笉鍐嶈繑鍥?`JsonToBlueprintRules.md`銆?
12. [ ] Transaction 绾ф仮澶嶏細绉婚櫎榛樿 `blueprint_undo` / `blueprint_redo` 鍚庯紝璁捐鍩轰簬 TaskRunJournal 鎴?UE transaction 鐨?undo/redo/replay 鑳藉姏銆?
13. [x] ReadSpec target 瀛楁鏀舵暃锛氬凡纭 `graph_name` / `function_name` / `event_name` 鍘嬬缉涓?`target.target_name`锛岀敱 `target.target_type` 瑙ｉ噴锛沗block_id` 鍥?ownership/id 璇箟淇濈暀鐙珛瀛楁銆?
14. [x] Read result schema 鐭悕瑙勫垯锛氬凡纭 `data.schema` 浣跨敤 `ReadContextPack.v1`銆乣LogicMd.v1`銆乣LogicJson.v1` 绛夌煭鍚嶏紝涓嶉噸澶?`BlueprintHelper.` 鍓嶇紑銆?
15. [x] ReadSpec 棣栨壒 `read_type` 宸茬‘璁わ細`asset_context`銆乣blueprint_logic`銆乣component_context`銆乣variable_context`銆乣graph_context`銆乣widget_context`銆乣data_table_context`銆乣object_property_context`銆?
16. [x] ReadContextPack 棣栫墖杩斿洖瀛楁锛氬凡纭浣跨敤 `payload` 鎵胯浇鍏蜂綋 read view锛涗笉璁剧疆鐙珛 `read_id`锛涘彧璇荤粨鏋滀笉甯?`diagnostics`锛岄敊璇蛋澶栧眰 `error`锛屽畬鏁存€х敤 `truncated` 鍜屽潡绾?涓婁笅鏂囧垏鐗囬噸璇诲缓璁紱涓嶅啀鎶婃壒閲忎笂涓嬫枃寮曠敤浣滀负 Agent 涓荤嚎銆?
17. [x] AgentGuide 宸ュ叿杩斿洖鍚堝悓锛歚blueprinthelper_read_agent_guide` 鏃犺姹傚瓧娈碉紝杩斿洖 AgentGuide 绱㈠紩 Markdown锛涘畠鍙礋璐ｆ枃妗ｅ叆鍙ｏ紝涓嶈鍙?UE 璧勪骇锛屼篃涓嶈繑鍥炲姩鎬?schema銆?
18. [x] ReadRef 鍒?WriteAnchor 杞崲鍚堝悓锛氬凡纭閲囩敤 v0.3.6 grouped LogicJson / block-scoped anchor銆侭lueprintHelper-owned block 鐢?`block_id` / `group_entry_node_path` 鍔犵粍鍐?`node_ref` / `pin_ref` / `link_ref` 鏄犲皠鍒?TaskSpec patch/merge selector锛涜８ `nodes[index]`銆佹樉绀哄悕鍜?GUID-first 涓嶄綔涓?Agent 涓荤嚎鍐欓敋鐐癸紝GUID 浠呬綔 expert/debug fallback銆?
19. [x] Signature 鑳藉姏鑱岃矗纭锛歚blueprint_signature` 璐熻矗鍒涘缓/纭繚銆佷慨鏀广€佺Щ闄ゅ嚱鏁扮鍚嶃€丆ustom Event 绛惧悕銆乮nterface function / interface event 鍏ュ彛銆乪vent dispatcher 绛惧悕銆乷verride/native event 鍏ュ彛锛汫raphWrite 璐熻矗 body銆佽妭鐐广€佽繛绾裤€佽皟鐢ㄣ€乥ind/unbind銆?
20. [x] Custom Event 鍏ュ彛涓?Append 渚濊禆杈圭晫纭锛歚graph_write.ensure_entry(entry_type=custom_event)` 鍙互淇濈暀涓?append 璇箟鐨勭粨鏋勫寲 IR锛屼絾 Custom Event 鍏ュ彛澹版槑/绛惧悕鍒涘缓蹇呴』鐢?`blueprint_signature.ensure_custom_event` 鎴?UE 鍐呴儴 BlueprintSignatureService 瀹屾垚锛涗笉寰楁柊澧?Agent-facing custom event 鍘熷瓙宸ュ叿銆?
21. [x] `custom_event_definition` 涓?Signature 杈圭晫锛氬凡纭骞跺疄鐜颁负 Signature 鐨?`ensure_custom_event` 澹版槑/绛惧悕 step 鍔?GraphWrite 鐨?`custom_event_body` body rewrite step锛涗笉鏂板 Agent-facing custom event 鍘熷瓙宸ュ叿銆?
22. [x] Interface/override/native event lowering 缁嗚妭锛歩nterface function 闄嶅埌 `ensure_function`锛宨nterface event 闄嶅埌 `ensure_custom_event`锛沷verride/native event 榛樿 `execute_policy=blocked_preflight`锛屾樉寮?`execute_policy=create_if_missing` 宸叉湁 source-integrated 鍒涘缓璺緞锛岀敤鎴蜂晶 UE build 宸查€氳繃锛屽緟 Unified SmokeRun Ring 7 automation / smoke 鍚庡啀鏍?verified銆?
23. [x] Signature removal 瀹夊叏鍚堝悓棣栫墖锛氱Щ闄ょ鍚嶅繀椤?`execute_policy=blocked_preflight` 涓旇姹?reference context锛涘綋鍓嶄笉鎵ц鐪熷疄鍒犻櫎锛屽悗缁啀纭寮曠敤鍒嗘瀽鍚庣殑鐪熷疄 cleanup 绛栫暐銆?
24. [x] Event Dispatcher 瀛楁缁嗚妭棣栫墖锛歞ispatcher 澹版槑銆佸弬鏁板拰绛惧悕灞炰簬 Function/Event Signature锛沝ispatcher call/bind/unbind 浠嶅睘浜?GraphWrite锛涚幇闃舵鐜版湁 dispatcher 绛惧悕涓嶅尮閰嶆椂鍙厑璁?block銆?
25. [ ] 闈?BlueprintHelper-owned 鍥惧唴瀹圭殑绋冲畾鍐欓敋鐐癸細owned block 宸叉湁 `block_id` 涓荤嚎锛涚敤鎴峰凡鏈夊浘鑺傜偣銆侀潪 owned 鑺傜偣鍜屾棫璧勪骇杩佺Щ鍦烘櫙浠嶉渶鍗曠嫭纭绋冲畾 read/write anchor 绛栫暐銆?
26. [ ] Ownership metadata migration/repair锛氬綋鍓嶉鐗囦笉鏄垹闄ゆ棫璧勪骇娉ㄩ噴锛屼笉褰卞搷 TaskSpec / TaskPlan 涓荤嚎锛涘悗缁彲鍦?fallback銆佸璁¤緭鍑哄拰 smoke 瑕嗙洊鏄庣‘鍚庢竻鐞嗘棫 `NodeComment` 涓殑 `block_id` / `tx` 鐗囨銆?

## 涓嬭疆鍙苟琛屾媶鍒?

| 浠诲姟 | 鍐欏叆鑼冨洿 | 鏄惁鍐茬獊 | 寤鸿妯″瀷 |
| --- | --- | --- | --- |
| GraphWrite Patch/Merge block-scoped 鍐欓敋鐐瑰疄鐜颁笌 Replace exec link 淇 | `LogicJson` grouped builder銆丅ridge node resolver銆乀askSpec compiler銆丟raphWrite Replace/Patch/Merge services/tests | 涓庢柊 UE 鑳藉姏涓瓑鍐茬獊 | 5.5 xhigh |
| Preview blocker / ReferenceContextPack 闆嗘垚 | `TaskRuntime`銆乣DependencyAnalysisService`銆丮CP task result | 涓?Runtime 鏀瑰姩鍐茬獊 | 5.5 xhigh |
| ReadSpec 閫氱敤璇诲眰璁捐涓庨鐗囪惤鍦?| `ClaudePlugin/mcp/src`锛孉gentGuide read schema锛孡ogicMD/LogicJson adapter | 涓庡啓 Runtime 涓嶅啿绐?| 5.5 xhigh |
| Function/Event Signature UE 鑳藉姏璁捐钀藉湴 | `Source/BlueprintHelper/Public|Private/Services`锛宍Structure`锛孊ridge锛孴ests | 涓?Runtime 鍩虹浣庡啿绐?| 5.5 xhigh |
| Component DTO 杩佸嚭 Service header | `Structure` + `ComponentService` + tests | 涓?Component compiler 涓嶅啿绐?| 5.3 codex-spark xhigh |

## 鎺ㄨ崘涓嬩竴姝?

P0 涓?P1 compiler/contract 棣栫墖宸茬粡瀹屾垚锛屽彉閲忕皣 member property/default/local variable 棣栫墖宸茬粡钀藉埌 UE Service + OperationHandler锛屼笖 `edit_blueprint_variables` 宸插畬鎴?TaskSpec -> Execute smoke銆侫ssetFactory 涓?Component preview 宸查€氳繃锛孋lassSettings銆乁MG銆丏ataTable 杩橀渶瑕?disposable fixture銆侰omposite `create_blueprint_feature` 宸茶兘鎶婄墿鐞嗛棬杩欑被鏍稿績鍔熻兘 TaskSpec 鍒嗚В涓哄 step TaskPlan锛屽苟宸茶ˉ `integration.interface` 棣栫墖锛氱‘淇濇帴鍙ｃ€佺‘淇濆嚱鏁板叆鍙ｃ€佺敤 GraphWrite replace_body 鍐欐帴鍙ｅ嚱鏁板疄鐜帮紱鏈€鏂?smoke 宸茬‘璁?composite preview 閫氳繃锛屼絾 fixture 鍒涘缓璺緞浠嶆毚闇?`create_blueprint_feature` 绌洪敊璇綊涓€鍖栭棶棰橈紝涓嬩竴姝ユ槸 execute fixture 鍜岄敊璇綊涓€鍖栥€侴raphWrite replace/patch/merge 鐨?TaskSpec compiler 涓?Runtime lowering 宸茶繘鍏?Rerun 4 verified 鐘舵€侊細Replace full pipeline 涓?read-back 閫氳繃锛孭atch 鍙慨鏀?owned block锛孧erge 鐨?`insert_between + function_call`銆乣append_after + function_call`銆乣insert_between + custom_event_call` 宸查€氳繃锛?026-05-07 smoke 杩涗竴姝ョ‘璁?`branch_fork` preview 鍙蛋閫氾紝鍚庣画宸茶ˉ MCP/Bridge 绌洪敊璇綊涓€鍖栦笌 `branch_fork + owned_block_call` source fix锛屼粛闇€鏈湴 UE build / Editor reload 鍚庡璺?execute smoke銆?

P2 棣栨壒涓夌皣宸茶繘鍏ユ簮鐮佹帴绾块樁娈碉細`blueprint_signature`銆乣object_property`銆乣graph_cleanup_ownership` 閮芥部 TaskSpec -> TaskPlan -> Runtime dispatch 鎺ュ叆锛屼笉鏂板 Agent-facing 鍘熷瓙鍐欏伐鍏枫€傚綋鍓嶇姸鎬佷粛鏄?source integrated锛屼笉鏄?smoke verified锛涙寜鐢ㄦ埛瀹夋帓锛屽厛涓嶅崟鐙祴璇曡繖浜涚皣锛岀瓑涓変釜瀹屾暣绨囪惤榻愬悗缁熶竴 Automation 鍜?disposable fixture smoke銆俇E build 宸查€氳繃锛屼笉鍐嶄綔涓哄綋鍓嶉樆濉炲彛寰勩€?

褰撳墠 P1/P2 鍓╀綑涓嶉樆濉炵户缁紑鍙戠殑楠岃瘉椤规槸锛歎MG/DataTable disposable fixture銆丆omposite execute fixture銆乀askRunJournal partial failure fixture銆佸悓 graph `branch_fork + owned_block_call` execute smoke銆乣create_blueprint_feature` preview 绌洪敊璇紝浠ュ強 P2 棣栨壒涓夌皣缁熶竴楠岃瘉銆俁4 宸查獙璇?`branch_fork + custom_event_call` execute/read-back锛沗append_after + custom_event_call` 鐨?preview 绌洪敊璇凡闄嶇骇涓哄彲璇婃柇 blocker锛宺untime profile 鐨?GraphWrite merge stale 鏍囪宸蹭粠婧愮爜绉婚櫎銆?026-05-09 FullTestLog 涓殑 grouped failure 宸插畬鎴愭簮鐮佹垨娴嬭瘯鍙ｅ緞淇锛屼笅涓€姝ユ寜 Unified SmokeRun Ring 1 澶嶈窇鍚庡啀鏍?smoke verified銆?

```text
Prepare fixtures and rerun AssetFactory/Component/UMG/DataTable/Composite execute smoke
-> Add controlled partial-failure fixture for TaskRunJournal topology blocking
-> Rerun same-graph branch_fork + owned_block_call execute smoke in the unified Automation / Editor fixture pass, and fix create_blueprint_feature preview empty error
-> Run grouped P2 verification for Signature / ObjectProperty / CleanupOwnership
-> Function/Event Signature Management 鍚庣画瀛楁鍜?remove execute policy
-> DebugCase / DebugBundle verification tail and retention / cleanup policy
```

杩欐牱鑳界户缁部 TaskSpec -> TaskPlan -> Runtime lowering 鐨勭粨鏋勫寲缂栬緫璇█鏂瑰悜鎵╁睍锛屼笉浼氶€€鍥炲埌搴曞眰宸ュ叿鑶ㄨ儉銆?

## 2026-05-07 AssetFactory Blueprint Alias Follow-up

- [x] Ordinary Blueprint fixture creation source fix integrated: `create_asset` accepts `asset_type=Actor` and `asset_type=blueprint` as `blueprint_class` with `parent_class=Actor`.
- [ ] WidgetBlueprint and DataTable factory source is integrated; UE smoke is still pending under Unified SmokeRun Ring 3. `create_blueprint_feature` preview empty-error remains open under Ring 6.

