# BlueprintHelper 娣峰悎 TaskSpec / TaskPlan 鏋舵瀯婧愭枃妗ｅ悓姝ョ储寮?

鏃ユ湡锛?026-05-04  
鐘舵€侊細鍚屾琛ヤ竵鍖? 
鐩爣锛氭妸鏂扮‘璁ょ殑娣峰悎鏋舵瀯鍚屾鍥炵幇鏈夎璁℃枃妗ｏ紝鑰屼笉鏄帹缈诲師鏈夊伐鍏风皣鏂囨。銆?

---

## 1. 鏈鍚屾鎬诲師鍒?

鏈鍚屾涓嶆帹缈诲凡鏈?11 绫诲伐鍏风皣锛屼篃涓嶆帹缈诲師鏈夊洓灞傛灦鏋勩€?

鏂扮殑缁熶竴鍙ｅ緞鏄細

```text
Agent
鈫?BlueprintHelper CLI Task Commands
鈫?task-core / Python Task Compiler
鈫?UE Plugin Task Runtime
鈫?Existing UE Capability Clusters
```

宸叉湁宸ュ叿绨囨敼涓猴細

```text
鍐呴儴鑳藉姏绨?/ TaskPlan step / Debug-Expert 宸ュ叿 / 娴嬭瘯鍏ュ彛
```

鏅€?Agent-facing 鍏ュ彛鏀舵暃涓猴細

```text
blueprinthelper_read_task_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprint_get_runtime_profile
blueprinthelper_diagnostics
```

---

## 2. 蹇呴』鍚屾鐨勬簮鏂囨。

| 浼樺厛绾?| 鏂囨。 | 鍚屾鍐呭 |
|---|---|---|
| P0 | BlueprintHelper 鎻掍欢鏋舵瀯 | 澧炲姞 Task Compiler / Task Runtime 鐨勬贩鍚堟灦鏋勫彛寰?|
| P0 | 鍐欏伐鍏疯璁?鍚屾绋?| 澧炲姞 TaskSpec / TaskPlan / task_run_id 涓?Graph Write 鍏崇郴 |
| P0 | Transaction / Journal / Review | 澧炲姞 task_run_id / TaskRunJournal / Review 鎸?task 鍒嗙粍 |
| P0 | Safety Profile / dry_run | 澧炲姞 TaskSpec / TaskPlan 瀹夊叏绛栫暐銆乧ontext_stale銆乸review/execute 瑙勫垯 |
| P0 | Validation / Diagnostics | 澧炲姞 read_task_context / preview_task / execute_task 杈圭晫 |
| P1 | Asset Factory / Component / Class Settings / Enhanced Input | 鏍囨敞涓哄唴閮?capability / TaskPlan step锛屼笉鍐嶉粯璁?Agent 鐩存帴璋冪敤 |
| P1 | Version Roadmap | 澧炲姞 v0.4/v0.5 娣峰悎鏋舵瀯鐗堟湰褰掑睘 |
| P1 | 鏍稿績涓夌鑳藉姏缂哄彛 | 澧炲姞 Task Orchestration Gap |

---

## 3. 鏈ˉ涓佸寘鏂囦欢

```text
BlueprintHelper_Architecture_Synced_20260504.md
GraphWrite_Setup_Cleanup_Synced_20260504.md
06_Transaction_Journal_Review_Design_Synced_20260504.md
07_Safety_Profile_DryRun_Design_Synced_20260504.md
05_Validation_Diagnostics_Tools_Design_Synced_20260504.md
01_Asset_Factory_Tools_Design_Synced_20260504.md
02_Blueprint_Component_Tools_Design_Synced_20260504.md
03_Blueprint_Class_Settings_Tools_Design_Synced_20260504.md
04_Enhanced_Input_Boundary_Design_Synced_20260504.md
Version_Roadmap_Synced_20260504.md
Core_Three_End_Gap_Synced_20260504.md
```

---

## 4. 涓嶉渶瑕佹敼鍔ㄧ殑鏍稿績鍙ｅ緞

浠ヤ笅鍙ｅ緞淇濇寔涓嶅彉锛?

```text
1. Asset Factory 鍙垱寤鸿祫浜с€?
2. add_component 鍙垱寤虹粍浠跺拰 attachment銆?
3. Class Settings 鍙慨鏀圭被璁剧疆锛屼笉鍐欏浘琛ㄩ€昏緫銆?
4. Enhanced Input 褰撳墠涓嶉粯璁ょ紪杈?IA / IMC銆?
5. Append / Replace / Patch / Merge 鐨?Graph Write 杈圭晫涓嶅彉銆?
6. transaction_id 浠嶆槸涓€鍐欐搷浣滀竴娆°€?
7. 鎵€鏈?UE 鍐欐搷浣滃唴閮ㄤ粛杩涘叆 Journal / Review銆?
8. 鏅€氬伐鍏锋垚鍔熺粨鏋滀笉榛樿杩斿洖 transaction / review / safety銆?
9. safety_profile 鍙潵鑷?runtime_profile.active_profile銆?
10. dry_run 鏄啓鍓嶉妫€锛孯eview 鏄啓鍚庡璁°€?
```

---

## 5. 鏂板鏍稿績鍙ｅ緞

```text
1. TaskSpec 鏄?Agent-facing 璇箟瑙勬牸銆?
2. TaskPlan 鏄?Task Compiler 杈撳嚭缁?UE Task Runtime 鐨勬墽琛岃鍒掋€?
3. TaskContextPack 鐢ㄤ簬 Agent 鐢熸垚 TaskSpec 鍓嶈幏鍙栬冻澶熶笂涓嬫枃銆?
4. preview_task 璐熻矗 TaskSpec 鏍￠獙銆乸olicy 妫€鏌ャ€乨ry_run/preflight锛屼笉鍐欒祫浜с€?
5. execute_task 璐熻矗鎵ц閫氳繃 preview 鐨?TaskPlan銆?
6. task_run_id 鏄竴娆?TaskSpec / TaskPlan 鎵ц鐨勬€?ID銆?
7. TaskRunJournal 璐熻矗鍏宠仈 child transaction_ids銆?
8. Review UI 榛樿搴旀寜 task_run_id 鍒嗙粍锛屽啀灞曞紑 transaction銆?
9. Bridge 灞傞敊璇敱 Python / CLI 褰掍竴鍖栦负 Agent-facing Task Error銆?
10. UE 鎻掍欢渚ч€傚悎鍋?Task Runtime锛屼笉閫傚悎鍋?Agent TaskSpec suggested_patch 缂栬瘧鍣ㄣ€?
```

---

## 6. 寤鸿鍚堝叆璺緞

鏈悓姝ュ寘宸叉寜鏂版灦鏋勬鏌ワ紝浠ヤ笅涓ゅ宸蹭慨姝ｏ細

```text
1. Task Compiler 缁熶竴鍐欎綔 task-core / Python Task Compiler銆?
2. UE 鎵ц灞傜粺涓€鍐欎綔 UE Task Runtime 鈫?Existing UE Capability Clusters銆?
```

濡傛灉淇濇寔鐜版湁鎻掍欢鐩綍缁撴瀯锛屽缓璁斁鍏ワ細

```text
BlueprintHelper/Develop/Plan/HybridArchitecture/
```

鎴栫洿鎺ヨ鐩栫幇鏈夎璁℃枃妗ｏ細

```text
BlueprintHelper/Develop/Plan/BlueprintHelper_Architecture.md
BlueprintHelper/Develop/Plan/GraphWrite_Setup_Cleanup.md
BlueprintHelper/Develop/Plan/Transaction_Journal_Review.md
BlueprintHelper/Develop/Plan/Safety_Profile_DryRun.md
BlueprintHelper/Develop/Plan/Validation_Diagnostics.md
```

---

## 7. 涓嬩竴姝ュ缓璁?

涓嬩竴姝ュ簲浼樺厛杈撳嚭涓変唤 schema 鏂囨。锛?

```text
1. BlueprintHelper.TaskContextPack.v1
2. BlueprintHelper.TaskSpec.v1
3. BlueprintHelper.TaskPlan.v1
```

鐒跺悗鍐嶅悓姝?Agent Skill锛?

```text
Agent 榛樿娴佺▼锛歳ead_task_context 鈫?build TaskSpec 鈫?preview_task 鈫?repair 鈫?execute_task 鈫?report summary
```


