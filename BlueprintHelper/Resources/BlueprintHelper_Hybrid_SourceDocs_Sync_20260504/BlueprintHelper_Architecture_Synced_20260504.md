鍙互銆傚缓璁互鍚庢妸 **BlueprintHelper 鎬讳綋鏋舵瀯** 鍥哄畾鎷嗘垚鍥涗釜閮ㄥ垎锛?

```text id="igo5rj"
BlueprintHelper = UE 鎻掍欢渚?+ CLI 鏈嶅姟渚?+ Agent Skill 渚?+ 鐢ㄦ埛寮曞渚?
```

杩欏洓閮ㄥ垎鍒嗗埆瑙ｅ喅涓嶅悓闂锛屼笉搴旀贩鍐欏湪鍚屼竴绫绘枃妗ｆ垨鍚屼竴鐗堟湰鐩爣閲屻€?

---

## BlueprintHelper 鍥涢儴鍒嗘€诲垎绫?

| 鍒嗙被 | 鎺ㄨ崘鍚嶇О | 鏍稿績鑱岃矗 | 闈㈠悜瀵硅薄 |
|---|---|---|---|
| **1** | **UE 鎻掍欢渚?/ UE Plugin Layer** | 鍦?Unreal Editor 鍐呯湡姝ｈ鍙栥€佷慨鏀广€佷繚瀛樿祫浜?| Unreal Editor銆佽摑鍥俱€乁MG銆丏ataAsset銆丏ataTable |
| **2** | **CLI 鏈嶅姟渚?/ CLI Layer** | 鎶?Agent 璇锋眰杞崲鎴?UE Bridge 璋冪敤锛屽苟澶勭悊鍗忚銆佽繛鎺ャ€佽繑鍥炴牸寮?| Agent銆丮CP Client銆乁E Bridge |
| **3** | **Agent Skill 渚?/ Agent Skill Layer** | 鍛婅瘔 Agent 濡備綍姝ｇ‘浣跨敤宸ュ叿銆佸浣曞垽鏂摑鍥?C++杈圭晫銆佸浣曢€夋嫨 LogicMD/LogicJson/RawJson | Claude / Codex / ChatGPT Agent |
| **4** | **鐢ㄦ埛寮曞渚?/ User Guidance & Setup Layer** | 闈㈠悜鐢ㄦ埛瀹屾垚瀹夎銆侀厤缃€侀棶绛斿紡鍋忓ソ閲囬泦銆侀」鐩鑼冪敓鎴?| 鎻掍欢鐢ㄦ埛銆侀」鐩淮鎶よ€?|

杩欎釜鎷嗗垎鍜屼箣鍓嶇増鏈嚎鍙互鍏煎銆傚凡鏈夌増鏈鍒掗噷宸茬粡鎶婁富绾垮畾涔変负 **鏁版嵁琛ㄨ揪 鈫?缂栬緫鍣ㄦ帴鍏?鈫?Agent 鍙敤鎬т紭鍖?*锛屽叾涓?v0.1.0 鏄摑鍥?JSON 琛ㄨ揪鍩虹锛寁0.2.0 鏄?CLI 缂栬緫鍣ㄦ帴鍏ワ紝v0.3.0 鏄?Agent 绋冲畾鎬т笌閫氫俊浼樺寲銆傤垁filecite顖倀urn2file0顖?鍚庣画 Skill銆丼etup銆佸紩瀵兼枃妗ｆ濂借ˉ涓娾€淎gent 濡備綍姝ｇ‘浣跨敤鈥濆拰鈥滅敤鎴峰浣曢厤缃€濈殑涓婂眰鑳藉姏銆?

---

# 1. UE 鎻掍欢渚?/ UE Plugin Layer

## 瀹氫綅

UE 鎻掍欢渚ф槸 **鐪熸鎵ц缂栬緫鍣ㄦ搷浣滅殑搴曞眰鑳藉姏灞?*銆?

瀹冧笉璐熻矗鏁?Agent 鎬庝箞鎬濊€冿紝涔熶笉璐熻矗鐢ㄦ埛瀹夎璇存槑锛涘畠鍙礋璐ｅ湪 Unreal Editor 鍐呭畨鍏ㄣ€佺ǔ瀹氥€佸彲鍥炴粴鍦板畬鎴愯祫浜ц鍙栧拰淇敼銆?

## 鍖呭惈鍐呭

```text id="6i7sq5"
BlueprintHelper.uplugin
Source/BlueprintHelper/
Resources/
Editor Widget / Panel
UE Bridge Server
Blueprint Export / Import
LogicProcessor
Asset / Blueprint / UMG / DataTable / DataAsset 鎿嶄綔
Undo / Redo / Save / Compile / PIE 鎺у埗
```

## 涓昏鑱岃矗

- 钃濆浘 RawJson 瀵煎嚭/瀵煎叆銆?
- LogicJson / LogicMD 鐢熸垚銆?
- AgentImportGraph 瀵煎叆銆?
- 璧勪骇娴忚銆佹悳绱€佹墦寮€銆佷繚瀛樸€?
- 钃濆浘鍙橀噺銆佸嚱鏁般€佸畯銆佷簨浠躲€佽妭鐐规搷浣溿€?
- UMG Widget 鏍戣鍙栦笌缂栬緫銆?
- DataAsset / UObject 灞炴€ц鍐欍€?
- DataTable 琛岃鍐欍€?
- 缂栬緫鍣ㄥ懡浠ゃ€佺紪璇戙€丳IE銆乁ndo/Redo銆?
- 鏈潵鐨?Diff銆佸闃呫€佸彉鏇寸‘璁ゃ€佸洖婊?UI銆?

## 涓嶅簲璇ユ壙鎷?

- 涓嶈礋璐?Agent 鎻愮ず璇嶃€?
- 涓嶈礋璐?Codex / Claude Skill 瑙勫垯銆?
- 涓嶈礋璐ｇ敤鎴烽棶绛斿紡 setup銆?
- 涓嶈礋璐ｈВ閲娾€滀粈涔堟椂鍊欑敤钃濆浘锛屼粈涔堟椂鍊欑敤 C++鈥濄€?

涓€鍙ヨ瘽锛?

> UE 鎻掍欢渚ц礋璐ｂ€滆兘涓嶈兘鍦?UE 閲屾纭墽琛屸€濄€?

---

# 2. CLI 鏈嶅姟渚?/ CLI Layer

## 瀹氫綅

CLI 鏈嶅姟渚ф槸 **Agent 涓?Unreal Editor 涔嬮棿鐨勫崗璁€傞厤灞?*銆?

瀹冧笉鐩存帴淇敼 `.uasset`锛岃€屾槸閫氳繃 UE Bridge 璋冪敤 UE 鎻掍欢渚ц兘鍔涖€傚畠鐨勫叧閿环鍊兼槸锛氭妸 UE 鑳藉姏鍖呰鎴?Agent 鍙皟鐢ㄣ€佸彲楠岃瘉銆佸彲杩借釜銆佷綆 Token 鐨勫伐鍏锋帴鍙ｃ€?

## 鍖呭惈鍐呭

```text id="8i1dr2"
ClaudePlugin/mcp/
src/tools.ts
src/bridge-client.ts
src/config.ts
src/resources.ts
CLI command contract
CLI resources
Bridge connection / reconnect / timeout
structuredContent / resource_ref / payload protocol
```

## 涓昏鑱岃矗

- 娉ㄥ唽 CLI 鍛戒护闈€?- 娉ㄥ唽 CLI Resources銆?
- 绠＄悊 UE Bridge 杩炴帴銆?
- 澶勭悊宸ュ叿鍙傛暟鏍￠獙銆?
- 澶勭悊閿欒鐮佸拰閿欒娑堟伅銆?
- 澶勭悊 `request_id` / `trace_id`銆?
- 澶勭悊 `logic_md`銆乣logic_json`銆乣raw_json_structured`銆乣resource_ref` 杩斿洖鏂瑰紡銆?
- 绠＄悊澶?payload 寤惰繜璇诲彇銆?
- 绠＄悊 build_project / open_editor 杩欑被鏈湴鐢熷懡鍛ㄦ湡宸ュ叿銆?
- 鏈潵澶勭悊鎸佷箙杩炴帴銆佽嚜鍔ㄩ噸杩炪€丩ength-Prefixed JSON framing 绛夐€氫俊绋冲畾鎬ч棶棰樸€?

涔嬪墠 v0.5.0 閫氫俊渚ц鍒掑凡缁忔妸鐭繛鎺ユ棩蹇楀埛灞忋€佹寔涔?BridgeClient銆丆lientSession銆乺equest_id / trace_id銆佽嚜鍔ㄩ噸杩炪€乼imeout銆佸崗璁檷绾х瓑鍒楀叆閫氫俊渚ф敼鍔ㄨ寖鍥淬€傤垁filecite顖倀urn2file2顖?杩欎簺鍐呭搴斿綊鍒?CLI 鏈嶅姟渚э紝涓嶅簲娣峰叆 Skill 鎴栫敤鎴锋枃妗ｅ眰銆?

## 涓嶅簲璇ユ壙鎷?

- 涓嶇洿鎺ヤ慨鏀?UE C++ 婧愮爜銆?
- 涓嶇洿鎺ュ啓 `.uasset`銆?
- 涓嶆浛浠?UE 鎻掍欢渚у仛璧勪骇浜嬪姟銆?
- 涓嶈礋璐ｈВ閲婇」鐩紑鍙戣鑼冦€?
- 涓嶈礋璐ｇ敤鎴峰亸濂介棶绛旓紝鍙秷璐?setup 缁撴灉銆?

涓€鍙ヨ瘽锛?

> CLI 鏈嶅姟渚ц礋璐ｂ€淎gent 鎬庝箞绋冲畾銆佷綆鎴愭湰鍦拌皟鐢?UE 鑳藉姏鈥濄€?

---

# 3. Agent Skill 渚?/ Agent Skill Layer

## 瀹氫綅

Skill 渚ф槸 **缁?Agent 鐪嬬殑鎿嶄綔瑙勭害灞?*銆?

瀹冪殑鐩爣涓嶆槸缁欎汉闃呰锛岃€屾槸璁?Agent 鍦ㄨ璋冪敤鏃惰嚜鍔ㄧ煡閬擄細

- 杩欐槸 UE5.3+ 鐨?BlueprintHelper 鎻掍欢椤圭洰銆?
- CLI 鍛戒护闈笉鏄€氱敤鏂囦欢缂栬緫鍣ㄣ€?- 鍐欐搷浣滃繀椤绘寚瀹氳祫浜ц矾寰勫拰鍥捐〃銆?
- 榛樿鍏堣 LogicMD銆?
- 缁撴瀯鍖栧垎鏋愯 LogicJson銆?
- 绮剧‘淇濈湡銆佸鍏ャ€丳in 绾ц皟璇曟墠璇?RawJson銆?
- C++ 婧愮爜淇敼涓嶈兘璧?BlueprintHelper CLI銆?- 钃濆浘鍜?C++ 鐨勫紑鍙戣竟鐣屽簲鎸夌敤鎴?setup profile 鍒ゆ柇銆?

## 鍖呭惈鍐呭

```text id="l4rmzw"
Skills/
BlueprintHelper/SKILL.md
BlueprintHelper/tool_usage_policy.md
BlueprintHelper/blueprint_cpp_boundary.md
BlueprintHelper/logic_reading_strategy.md
BlueprintHelper/import_strategy.md
BlueprintHelper/error_recovery.md
BlueprintHelper/examples/
```

## 涓昏鑱岃矗

- 瀹氫箟 Agent 璋冪敤 CLI 鍛戒护鐨勯粯璁ら『搴忋€?- 瀹氫箟 LogicMD / LogicJson / RawJson / resource_ref 鐨勪娇鐢ㄧ瓥鐣ャ€?
- 瀹氫箟钃濆浘缂栬緫鍓嶇疆妫€鏌ャ€?
- 瀹氫箟鍐欐搷浣滃畨鍏ㄨ鍒欍€?
- 瀹氫箟澶辫触鎭㈠绛栫暐銆?
- 瀹氫箟浣曟椂鍋滄浣跨敤 CLI锛屾敼鐢ㄤ唬鐮佸伐鍏枫€?
- 瀹氫箟钃濆浘鍛藉悕銆佸嚱鏁板懡鍚嶃€佸彲鎼滅储鎬ц姹傘€?
- 娑堣垂鐢ㄦ埛 setup profile锛岀敓鎴愪釜鎬у寲 Agent 琛屼负瑙勫垯銆?

宸叉湁鏂囨。琛ラ綈璁″垝涓紝P0 宸茬粡鎶?**README銆丮CP QuickStart銆丼etup 闂瓟銆丼etup Profile銆丆laude Skill銆丮CP API Reference** 鍒椾负涓嬩竴鎵逛紭鍏堥」锛汸1 杩樺寘鍚?CLI Resources銆丩ogicJson/LogicMD/AgentImportGraph Schema銆佸懡鍚嶈鑼冦€佽摑鍥?C++杈圭晫銆佸畨鍏ㄦā鍨嬨€傤垁filecite顖倀urn2file1顖?鍏朵腑 Claude Skill銆佸伐鍏蜂娇鐢ㄧ瓥鐣ャ€佽摑鍥?C++杈圭晫瑙勫垯搴斿綊鍏?Skill 渚с€?

## 涓嶅簲璇ユ壙鎷?

- 涓嶅疄鐜?UE 璧勪骇鎿嶄綔銆?
- 涓嶅疄鐜?CLI 鍗忚銆?
- 涓嶄繚瀛樼敤鎴?UI 閰嶇疆銆?
- 涓嶅啓闀跨瘒鐢ㄦ埛鎵嬪唽銆?
- 涓嶆浛浠?README銆?

涓€鍙ヨ瘽锛?

> Agent Skill 渚ц礋璐ｂ€淎gent 搴旇鎬庝箞姝ｇ‘浣跨敤 BlueprintHelper鈥濄€?

---

# 4. 鐢ㄦ埛寮曞渚?/ User Guidance & Setup Layer

## 瀹氫綅

鐢ㄦ埛寮曞渚ф槸 **闈㈠悜鐢ㄦ埛鐨勫畨瑁呫€侀厤缃€佸亸濂介噰闆嗗拰瑙勮寖鐢熸垚灞?*銆?

瀹冭В鍐崇殑鏄敤鎴峰浣曞紑濮嬩娇鐢ㄦ彃浠讹紝浠ュ強濡備綍鎶婅嚜宸辩殑寮€鍙戝亸濂戒紶閫掔粰 Agent銆?

## 鍖呭惈鍐呭

```text id="1eibc3"
README.md
QuickStart.md
Install.md
CLI_Setup.md
Setup Wizard / Setup Profile
Blueprint_CPP_Boundary_QA.md
User_Preferences.md
Troubleshooting.md
Examples.md
Release Notes
CHANGELOG
```

## 涓昏鑱岃矗

- 瀹夎鎻掍欢銆?
- 閰嶇疆 CLI Server銆?
- 閰嶇疆 UE_ENGINE_DIR / UE_PROJECT_FILE銆?
- 妫€鏌?UE Bridge 鏄惁鍙敤銆?
- 鐢熸垚 setup profile銆?
- 閫氳繃闂瓟纭畾鐢ㄦ埛钃濆浘/C++杈圭晫銆?
- 璁板綍鐢ㄦ埛寮€鍙戝亸濂姐€?
- 鐢熸垚 Agent 鍙秷璐圭殑瑙勫垯鏂囦欢銆?
- 鎸囧鐢ㄦ埛濡備綍娴嬭瘯杩為€氭€с€?
- 鎸囧鐢ㄦ埛濡備綍澶勭悊甯歌閿欒銆?
- 瑙ｉ噴鐗堟湰宸紓鍜屽姛鑳借寖鍥淬€?

## 涓嶅簲璇ユ壙鎷?

- 涓嶇洿鎺ュ疄鐜版柊鐨?Agent 鐩磋揪鍛戒护闈€?- 涓嶇洿鎺ヤ慨鏀?UE 璧勪骇銆?
- 涓嶅鍏ヨ繃澶?Agent 鍐呴儴瑙勫垯銆?
- 涓嶆浛浠?Skill 鑷姩鍙戠幇鏈哄埗銆?

涓€鍙ヨ瘽锛?

> 鐢ㄦ埛寮曞渚ц礋璐ｂ€滅敤鎴峰浣曞畨瑁呫€侀厤缃紝骞舵妸涓汉寮€鍙戦鏍间氦缁?Agent鈥濄€?

---

# 鎺ㄨ崘鏈€缁堟灦鏋勫彛寰?

鍙互鍦?README 鎴栨灦鏋勬枃妗ｄ腑杩欐牱鍐欙細

```md id="98xjvx"
# BlueprintHelper Architecture

BlueprintHelper is divided into four major parts:

1. UE Plugin Layer
   Provides the Unreal Editor-side capabilities, including Blueprint export/import,
   LogicJson/LogicMD generation, asset operations, UMG editing, DataAsset/DataTable
   access, compilation, save, undo/redo, and editor command integration.

2. CLI Layer
   Exposes the UE Plugin capabilities to AI Agents through BlueprintHelper CLI commands and local guide artifacts.
   It manages Bridge communication, tool schemas, request validation, payload formats,
   error handling, and editor lifecycle commands.

3. Agent Skill Layer
   Provides machine-readable rules for AI Agents. It defines how Agents should use
   BlueprintHelper tools, when to read LogicMD, LogicJson, RawJson, or resource_ref,
   how to handle Blueprint/C++ boundaries, and how to perform safe write operations.

4. User Guidance & Setup Layer
   Provides human-facing onboarding, setup, configuration, troubleshooting, and
   preference collection. It generates setup profiles and development-boundary rules
   that can be consumed by the Agent Skill layer.
```

---

# 鍥涢儴鍒嗗拰鐗堟湰瑙勫垝鐨勫叧绯?

| 鐗堟湰 | 涓讳綋褰掑睘 | 璇存槑 |
|---|---|---|
| **v0.1.0** | UE 鎻掍欢渚?| 钃濆浘 JSON 琛ㄨ揪銆丷awJson 瀵煎叆瀵煎嚭 |
| **v0.2.0** | UE 鎻掍欢渚?+ CLI 鏈嶅姟渚?| Agent 閫氳繃 CLI 璁块棶 Unreal Editor |
| **v0.3.0** | UE 鎻掍欢渚?+ CLI 鏈嶅姟渚?| LogicJson / LogicMD / AgentImport / Token 浼樺寲 / 绋冲畾鎬?|
| **v0.4.0** | UE 鎻掍欢渚?+ 鐢ㄦ埛寮曞渚?| Diff銆佸闃呫€佺‘璁ゃ€佸洖婊氥€佸畨鍏ㄥ伐浣滄祦 |
| **v0.5.0** | CLI 鏈嶅姟渚?+ Skill 渚?+ 鐢ㄦ埛寮曞渚?| Setup銆佽摑鍥?C++杈圭晫銆丄gent 鍋忓ソ娉ㄥ叆銆侀€氫俊绋冲畾鎬?|
| **v0.6.0** | Skill 渚?+ CLI 鏈嶅姟渚?| 澶у瀷椤圭洰绱㈠紩銆佽法钃濆浘鎼滅储銆佸紩鐢ㄨ拷韪€佸嚱鏁板懡鍚嶈鑼?|
| **v1.0.0** | 鍥涢儴鍒嗗叏閮ㄦ敹鏁?| 绋冲畾 API銆佺ǔ瀹氬崗璁€佸畬鏁存枃妗ｃ€佸畬鏁存祴璇?|

---

# 鎺ㄨ崘鐩綍鍒嗙被

涓嶄竴瀹氳绔嬪埢鏀圭湡瀹炵洰褰曪紝浣嗛暱鏈熷彲浠ユ寜杩欎釜閫昏緫鏁寸悊锛?

```text id="ot7td9"
BlueprintHelper/
鈹溾攢 UEPlugin/
鈹? 鈹溾攢 BlueprintHelper.uplugin
鈹? 鈹溾攢 Source/
鈹? 鈹溾攢 Content/
鈹? 鈹斺攢 Resources/
鈹?
鈹溾攢 ClaudePlugin/mcp/
鈹? 鈹溾攢 src/
鈹? 鈹溾攢 package.json
鈹? 鈹斺攢 README.md
鈹?
鈹溾攢 Skills/
鈹? 鈹斺攢 BlueprintHelper/
鈹?    鈹溾攢 SKILL.md
鈹?    鈹溾攢 tool_usage_policy.md
鈹?    鈹溾攢 blueprint_cpp_boundary.md
鈹?    鈹溾攢 logic_format_strategy.md
鈹?    鈹斺攢 examples/
鈹?
鈹斺攢 Docs/
   鈹溾攢 README.md
   鈹溾攢 QuickStart.md
   鈹溾攢 Setup.md
   鈹溾攢 CLI_API_Reference.md
   鈹溾攢 Troubleshooting.md
   鈹溾攢 VersionRoadmap.md
   鈹斺攢 Changelog.md
```

濡傛灉涓嶆兂鏀圭洰褰曪紝鍙互淇濇寔鐜版湁鎻掍欢鐩綍锛屽彧鍦ㄦ枃妗ｄ腑閲囩敤杩欎釜閫昏緫鍒嗙被锛?

```text id="rol31q"
BlueprintHelper/Develop/Plan/
BlueprintHelper/Resources/Docs/
BlueprintHelper/Resources/Skills/
BlueprintHelper/ClaudePlugin/mcp/
BlueprintHelper/Source/
```

---

# 鏈€缁堝畾涔?

寤鸿鎶?BlueprintHelper 鐨勬€讳綋瀹氫箟鏀规垚锛?

> **BlueprintHelper 鏄竴涓敱 UE 鎻掍欢渚с€丮CP 鏈嶅姟渚с€丄gent Skill 渚с€佺敤鎴峰紩瀵间晶缁勬垚鐨?UE5 Agent 缂栬緫杈呭姪绯荤粺銆俇E 鎻掍欢渚ц礋璐ｇ紪杈戝櫒鍐呰兘鍔涳紝CLI 鏈嶅姟渚ц礋璐ｅ崗璁笌閫氫俊锛孉gent Skill 渚ц礋璐?Agent 琛屼负瑙勫垯锛岀敤鎴峰紩瀵间晶璐熻矗瀹夎銆侀厤缃€佸亸濂介噰闆嗕笌寮€鍙戣竟鐣屽畾涔夈€?*

杩欎釜鍥涘垎娉曟瘮鈥滄彃浠?+ CLI鈥濇洿鍑嗙‘锛屼篃鑳藉绾冲悗缁?Skill銆丼etup銆佸紩瀵笺€丳rofile銆佸闃呭伐浣滄祦銆?
---

# 2026-05-04 娣峰悎 TaskSpec / TaskPlan 鏋舵瀯鍚屾

## 鍚屾缁撹

鏈枃浠跺師鏈夆€滃洓灞傛灦鏋勨€濅笉鎺ㄧ炕锛屼絾闇€瑕佽ˉ鍏呬竴涓柊鐨勪换鍔＄紪鎺掑垏鍒嗭細

```text
Agent
鈫?BlueprintHelper CLI Task Commands
鈫?task-core / Python Task Compiler
鈫?UE Plugin Task Runtime
鈫?Existing UE Capability Clusters
```

鍘熷洓灞備粛淇濈暀锛?

```text
BlueprintHelper = UE 鎻掍欢渚?+ CLI 鏈嶅姟渚?+ Agent Skill 渚?+ 鐢ㄦ埛寮曞渚?
```

鏂板鍙ｅ緞涓嶆槸绗簲涓钩绾у眰锛岃€屾槸鎶婂師 CLI 鏈嶅姟渚у拰 UE 鎻掍欢渚т箣闂寸殑浠诲姟鑱岃矗鎷嗘竻妤氾細

```text
task-core / Python Task Compiler锛氳礋璐?TaskSpec 鏍￠獙銆佷笂涓嬫枃鎵撳寘銆佽涔夋鏌ャ€乻uggested_patch銆乀askPlan 鐢熸垚銆?
UE Plugin Task Runtime锛氳礋璐?TaskPlan 鎵ц銆乀OCTOU 妫€鏌ャ€乼ask_run_id銆乼ransaction_id銆丣ournal / Review / rollback銆乧ompile/save銆?
```

## UE 鎻掍欢渚ц亴璐ｄ慨姝?

UE 鎻掍欢渚т粛璐熻矗鐪熷疄缂栬緫鍣ㄦ搷浣滐紝浣嗘柊澧?Task Runtime 鑱岃矗锛?

```text
FBlueprintHelperTaskRuntime
FBlueprintHelperTaskPlanValidator
FBlueprintHelperTaskRunJournalService
FBlueprintHelperTaskExecutionContext
FBlueprintHelperTaskRollbackCoordinator
FBlueprintHelperTaskReviewGrouper
```

UE 鎻掍欢渚у簲璇ヨ礋璐ｏ細

```text
1. 鎺ユ敹宸茬紪璇戠殑 TaskPlan锛岃€屼笉鏄洿鎺ョ悊瑙?Agent 鍘熷鑷劧璇█銆?
2. 鎵ц鍓嶉噸鏂拌鍙?UE 褰撳墠鐘舵€佸苟鍋?TOCTOU 妫€鏌ャ€?
3. 鎵ц TaskPlan steps銆?
4. 姣忎釜鐪熷疄鍐欐搷浣滅敓鎴?transaction_id銆?
5. 鏁翠釜 TaskPlan 鎵ц鐢熸垚 task_run_id銆?
6. 鍐?TaskRunJournal / Transaction Journal / Review snapshot銆?
7. 澶辫触鏃舵寜宸叉墽琛屽啓姝ラ閫嗗簭 rollback銆?
8. 鎵ц compile / diagnostics / save銆?
```

UE 鎻掍欢渚т笉寤鸿璐熻矗锛?

```text
1. Agent-facing TaskSpec suggested_patch銆?
2. JSONPath / JsonPatch 绾ч敊璇慨姝ｅ缓璁€?
3. 鑷劧璇█ goal 鈫?TaskSpec銆?
4. 涓嶅悓 Agent 瀹㈡埛绔吋瀹广€?
5. Agent 琛屼负绛栫暐鏂囨湰銆?
```

## CLI 鏈嶅姟渚ц亴璐ｄ慨姝?

CLI 鏈嶅姟渚т笉鍐嶉粯璁ゅ悜 Agent 鏆撮湶澶ч噺搴曞眰宸ュ叿銆傛櫘閫?Agent-facing 宸ュ叿搴旀敹鏁涗负锛?

```text
blueprinthelper_read_task_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprint_get_runtime_profile
blueprinthelper_diagnostics
```

搴曞眰宸ュ叿绨囦粛淇濈暀锛屼絾榛樿浣滀负锛?

```text
1. task-core / Python Task Compiler 鍙敤鐨?capability銆?
2. UE Task Runtime 鍐呴儴 step operation銆?
3. Debug / Expert / 娴嬭瘯鍏ュ彛銆?
4. CLI API Reference 鐨勪綆灞傝兘鍔涜鏄庛€?
```

## Agent Skill 渚ц亴璐ｄ慨姝?

Agent Skill 涓嶅啀鏁?Agent 鎵嬪姩鎷煎ぇ閲忓簳灞傚伐鍏疯皟鐢紝鑰屾槸鏁?Agent锛?

```text
1. 鍏?read_task_context 鑾峰彇 TaskContextPack銆?
2. 鍩轰簬 ContextPack 鐢熸垚 TaskSpec銆?
3. 璋?preview_task銆?
4. 鏍规嵁缁撴瀯鍖?error.issues 淇 TaskSpec銆?
5. preview 閫氳繃鍚?execute_task銆?
6. 鏈€缁堟姤鍛?task_run 鎽樿锛屼笉榛樿鎶ュ憡 transaction_id / journal path銆?
```

## 鐢ㄦ埛寮曞渚ц亴璐ｄ慨姝?

鐢ㄦ埛鏂囨。闇€瑕佽鏄庯細BlueprintHelper 鐨勬櫘閫氫娇鐢ㄥ叆鍙ｆ槸浠诲姟绾у伐鍏凤紝涓嶆槸搴曞眰宸ュ叿銆傚簳灞傚伐鍏蜂粛瀛樺湪锛屼絾涓昏鐢ㄤ簬璋冭瘯銆佹祴璇曞拰楂樼骇 Expert 鍦烘櫙銆?


