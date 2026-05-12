# BlueprintHelper Graph Write / Setup / Cleanup 缁煎悎璁捐姹囨€?

鏃ユ湡锛?026-05-03  
閫傜敤鑼冨洿锛欱lueprintHelper v0.4/v0.5 瑙勫垝鍓嶇疆璁捐  
鐘舵€侊細宸插悓姝ュ瓧娈靛崗璁?Diff 鐨勪慨璁㈢

---

## 0. 鏈枃鐩殑

鏈枃鏁村悎骞朵慨姝ｅ綋鍓嶅凡纭鐨?BlueprintHelper 鐩稿叧璁捐璁板繂锛岄噸鐐硅鐩栵細

- Setup Profile 涓庡畨鍏ㄦ。浣?
- Graph Write 宸ュ叿绨囷細Append / Replace / Patch / Merge
- transaction_id / block_id / operation_id
- BlueprintHelper-owned ownership 鏍囪
- Transaction Journal / Review / Diff / Rollback 鏁版嵁璁板綍
- Cleanup / Ownership 宸ュ叿绨?
- dry_run 涓?Review 鐨勫叧绯?
- runtime_profile / diagnostics / ToolResultBase 瀛楁杈圭晫
- LogicMD / LogicJson 鍒嗙粍璇诲彇涓庣簿纭宸ュ叿瑙勫垝

鏈枃鐢ㄤ簬鍚庣画瀹炵幇銆佹枃妗ｆ媶鍒嗐€佹祴璇曠敤渚嬫洿鏂板拰 Agent Skill 鐢熸垚銆?

---

## 0.1 鏈鍚屾 Diff 鎽樿锛?026-05-03锛?

鏈増鏈悓姝ュ凡纭鐨勫瓧娈靛崗璁拰 Agent 瑙勫垯宸紓锛?

```text
1. 鏅€氳兘鍔涘伐鍏蜂笉榛樿鍚?Agent 杩斿洖 transaction / review / safety锛汫raph Write銆丆leanup銆丱wnership銆丷ollback 绛夐珮椋庨櫓鎴栧悗缁紩鐢ㄦ祦绋嬪彲鎸夊伐鍏烽渶瑕佹毚闇插繀瑕佹憳瑕併€?
2. safety_profile 鍙粠 runtime_profile.active_profile 璇诲彇锛涘崟娆″伐鍏风粨鏋滀笉鎼哄甫 safety_profile銆?
3. dry_run 鏁版嵁鍙湪 status=dry_run 鏃舵斁鍦?data.dry_run銆?
4. runtime_profile.tool_capabilities 浣跨敤 unavailable_only 璐熷悜绋€鐤忔ā寮忥紝涓嶆槸瀹屾暣宸ュ叿绱㈠紩锛屼篃涓嶆槸 CLI command contract銆?
5. diagnostics 鏄彧璇昏瘖鏂紝瀹為檯鎶ュ憡鍦?data.markdown锛汳arkdown 涓殑 Blocking 涓嶇瓑浜庡伐鍏疯皟鐢ㄥけ璐ャ€?
6. LogicMD 鐨?target_graph / blueprint / multi_target 鏄鍏ュ彛鍒嗙粍璇诲彇锛岃繑鍥?grouped=true銆?
7. LogicJson 鐨?target_graph / blueprint / multi_target 浣跨敤 logic.groups[]锛沶ode_ref / link_ref 鏄?group 鍐呭眬閮ㄥ紩鐢ㄣ€?
8. 鏈枃鏈彂鐜扮埗绫讳慨鏀瑰啓宸ュ叿鏃у瓧娈碉紝鍥犳鏃犻渶鍋氬搴斿垹闄わ紱Parent Class 淇敼涓嶅睘浜庢湰鏂囦欢鐨?Graph Write / Setup / Cleanup 鑼冨洿銆?
9. Graph Write 鎴愬姛杩斿洖閲囩敤鏋佺畝 Agent-facing 鍙ｅ緞锛欰ppend 鍙繑鍥?graph / block_refs / write_ref / validation锛汻eplace 鍙繑鍥?target / write_ref / validation锛汸atch 鍙繑鍥?target / patch / write_ref / validation銆俢reated_nodes / created_links / summary 绛夎繘鍏?Journal / Review锛屼笉榛樿杩斿洖銆?
```

---

# 1. 宸蹭慨姝ｇ殑鍏抽敭鍙ｅ緞

## 1.1 transaction_id 鍙ｅ緞淇

鏈€缁堣鍒欙細

```text
涓€涓?transaction_id 瀵瑰簲涓€娆″啓宸ュ叿璋冪敤銆?
```

transaction_id 涓嶅啀琛ㄧず锛?

```text
浠庢墦寮€ Editor 鍒板叧闂?Editor 鐨勬暣涓細璇?
```

涔熶笉璁板綍锛?

```text
璇绘搷浣溿€佹墦寮€璧勪骇銆佺紪璇戙€佷繚瀛樸€丳IE銆乸reflight銆佹悳绱㈣祫浜?
```

transaction_id 鍙敤浜庤褰曚竴娆′細淇敼椤圭洰鎴栬摑鍥剧姸鎬佺殑鍐欐搷浣滐紝渚嬪锛?

```text
CreateBlueprint
AddVariable
AddFunction
AddComponent
SetClassDefaultProperty
AppendBlueprintGraph
ReplaceBlueprintGraph
PatchBlueprintGraph
MergeBlueprintGraph
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
ConvertBlueprintHelperBlockToUserOwned
```

鐢熸垚鏂癸細

```text
UE 鎻掍欢渚х敓鎴愬拰绠＄悊銆?
```

鐢ㄩ€旓細

```text
钃濆浘瀹￠槄銆丏iff銆丷ollback銆丷ecovery銆佹搷浣滃璁°€?
```

濡傛灉涓€娆″啓宸ュ叿璋冪敤淇敼澶氫釜钃濆浘鎴栧涓浘琛紝浠嶅睘浜庡悓涓€涓?transaction_id锛汻eview UI 闇€瑕佹寜钃濆浘銆佸浘琛ㄣ€佸嚱鏁般€乥lock_id 鍒嗙粍灞曠ず銆?

Agent-facing 鏆撮湶瑙勫垯锛?

```text
transaction_id 鍙敱 UE 鎻掍欢渚т负鍐欐搷浣滃唴閮ㄧ敓鎴愶紝骞跺啓鍏?Transaction Journal / Review Store銆?
浣嗗苟闈炴墍鏈夊啓宸ュ叿閮藉繀椤绘妸 transaction / review 瀛楁榛樿杩斿洖缁?Agent銆?
鏅€氳兘鍔涘伐鍏凤紙Asset Factory銆丅lueprint Component銆丅lueprint Class Settings 绛夛級鎴愬姛缁撴灉搴旇仛鐒?status銆乵odified銆乨ata.*_result銆乿alidation銆?
Graph Write銆丆leanup銆丱wnership銆丷ollback 绛夐渶瑕佸悗缁紩鐢?block_id / rollback / review 鐨勫伐鍏凤紝鍙寜宸ュ叿闇€瑕佸悜 Agent 鏆撮湶蹇呰鐨?transaction / block / rollback 鎽樿銆?
```


---

## 1.2 block_id 鍙ｅ緞淇

鏈€缁堣鍒欙細

```text
block_id = {鍥捐〃鍚嶆垨鍑芥暟鍚峿_{璋冪敤鍚嶇О}{閫掑id}
```

绀轰緥锛?

```text
EG_PhysicsDoor_TogglePhysicsDoor0
EG_PhysicsDoor_TogglePhysicsDoor1
OpenDoor_SetDoorOpen0
```

閫掑 id 浣滅敤鍩燂細

```text
鍚屼竴钃濆浘 + 鍚屼竴鍥捐〃/鍑芥暟 + 鍚屼竴璋冪敤鍚嶇О 鍐呬粠 0 閫掑銆?
```

瑙勫垯锛?

- block_id 鐢卞伐鍏风敓鎴愶紝涓嶇敱 Agent 鐢熸垚銆?
- block_id 涓嶅寘鍚摑鍥炬枃浠跺悕锛屽洜涓?asset_path / target_blueprint 鏄皟鐢ㄥ墠鎻愩€?
- block_id 缁熶竴浣跨敤涓嬪垝绾?`_`锛屼笉浣跨敤杩炲瓧绗?`-`銆?
- NodeComment 鍙互鏄剧ず浜虹被鍙鍐呭锛屼絾绋冲畾鏍囪瘑浠嶄娇鐢?`_`銆?
- block_id 蹇呴』鍐欏叆 Metadata 涓?NodeComment銆?
- ReplaceBlueprintGraph 鏇挎崲鍚屼竴涓?BlueprintHelper-owned block 鏃朵繚鐣欏師 block_id銆?

---

## 1.3 dry_run 鍙ｅ緞淇

鍗充娇鏈潵瀹炵幇 Review / Diff / Rollback锛宒ry_run 浠嶇劧蹇呰銆?

鏈€缁堝畾浣嶏細

```text
dry_run = 鍐欏叆鍓嶅畨鍏ㄩ妫€
Review = 鍐欏叆鍚庣敤鎴峰闃?
```

dry_run 涓嶅簲鎴愪负棰戠箒鎵撴柇鐢ㄦ埛鐨勭‘璁ゆ祦绋嬶紝鑰屽簲鐢卞伐鍏峰拰 Agent 鍦ㄥ啓鍏ュ墠鎷︽埅鏄庢樉閿欒銆?

dry_run 璐熻矗妫€鏌ワ細

```text
鏉冮檺
鐩爣璧勪骇/鍥捐〃
椋庨櫓绛夌骇
鍛藉悕鍐茬獊
鍏ㄥ眬浜嬩欢绂佺敤
鐩爣鍑芥暟/浜嬩欢鏄惁瀛樺湪
Pin / Schema / K2 鍙繛鎺ユ€?
ownership / dependency
Cleanup 鍒犻櫎鑼冨洿
```

Review 璐熻矗锛?

```text
灞曠ず鏈€缁?diff
鎸夎摑鍥?鍥捐〃/鍑芥暟/block_id 瀹￠槄
鎺ュ彈 / 鎷掔粷 / 鍥炴粴
鍘嬬缉鎴栧綊妗?Transaction Journal
```

Conservative 涓嬶細

```text
dry_run 鏃?error / conflict 鍙嚜鍔ㄦ寮忓啓鍏ャ€?
warning 涓嶉樆鏂紝浣嗗啓瀹屽悗闃舵鎶ュ憡璇存槑銆?
error / conflict 闃绘柇銆?
```

---

# 2. Setup Profile 涓庡畨鍏ㄦ。浣?

## 2.1 Agent 榛樿鍦烘櫙

榛樿 Agent 瀹㈡埛绔細

```text
Claude Code
```

濡傛灉娌℃湁鍔犺浇 BlueprintHelper Skill / 椤圭洰寮曞鏂囦欢锛岄渶瑕侀€氳繃椤圭洰鏍圭洰褰曪細

```text
CLAUDE.md
AGENTS.md
Setup Profile
```

鏆撮湶鎻掍欢鏂囨。銆佸伐鍏疯竟鐣屽拰瀹夊叏绛栫暐銆?

---

## 2.2 缂哄け鑳藉姏榛樿绛栫暐

Agent 閬囧埌宸ュ叿鑳藉姏缂哄け鏃讹細

```text
绔嬪嵆鍋滄骞舵姤鍛婄己澶卞伐鍏枫€?
```

涓嶅簲鍋囪缁х画瀹屾垚銆?

鐢ㄦ埛鎵嬪姩琛ラ綈鍙兘浣滀负棰濆娴嬭瘯锛屼笉璁″叆 Agent 鐙珛瀹屾垚鑳藉姏銆?

---

## 2.3 鍥涗釜瀹夊叏妗ｄ綅

Setup 搴旀彁渚涳細

```text
ReadOnly / 鍙
Conservative / 淇濆畧
Standard / 鏍囧噯
AutoRepair / 鑷姩淇
```

Claude Code 榛樿锛?

```text
Conservative / 淇濆畧
```

---

## 2.4 Conservative 榛樿绛栫暐

Conservative 涓嬶細

```text
鍏佽鍐欐搷浣溿€?
楂橀闄╁浘琛ㄥ繀椤?dry_run銆?
dry_run 鏃?error / conflict 鍙嚜鍔ㄦ寮忓啓鍏ャ€?
info / warning 涓嶉樆鏂紝鍐欏畬鍚庨樁娈垫姤鍛婅鏄庛€?
error / conflict 榛樿鍋滄骞舵姤鍛娿€?
涓嶈嚜鍔?cleanup 鏃?BlueprintHelper-owned 瀵煎叆鍧椼€?
涓嶈嚜鍔ㄤ慨鏀圭敤鎴锋墜鍐欒妭鐐广€?
鐢ㄦ埛鏄庣‘鎸囧畾鐩爣鍑芥暟/鍥捐〃鏃讹紝鍏佽淇敼璇ョ洰鏍囧唴鐨勭敤鎴疯妭鐐广€?
```

---

## 2.5 Standard 榛樿绛栫暐

Standard 涓嬶細

```text
鏃?BlueprintHelper-owned block 鍐茬獊鏃讹紝鍏佽鑷姩 cleanup 鍚庨噸璇曘€?
浣嗗彧娓呯悊鍚?block_id 鐨勮妭鐐广€?
```

---

## 2.6 楂橀闄╁浘琛ㄥ畾涔?

榛樿楂橀闄╋細

```text
鍥捐〃鍐呭凡缁忓啓濂界殑浜嬩欢鍥?
鍥捐〃鍐呭凡缁忓啓濂界殑鍑芥暟鍥?
宸叉湁鐢ㄦ埛鑺傜偣鐨勫浘琛?
宸叉湁 BlueprintHelper-owned block 鐨勫浘琛?
闇€瑕佷慨鏀瑰凡鏈夋墽琛岄摼鐨勫浘琛?
```

闄ら潪鐢ㄦ埛鏄庣‘鎸囧畾瑕佷慨鏀瑰摢涓嚱鏁版垨鍝釜鍥撅紝鍚﹀垯 Agent 涓嶅簲鐩存帴淇敼宸叉湁鍥捐〃銆?

Agent 鍐欎簨浠堕€昏緫鏃讹紝搴斾紭鍏堝垱寤烘柊鐨勪簨浠跺浘琛細

```text
EG_{FeatureName}
```

---

## 2.7 鍏ㄥ眬浜嬩欢榛樿绛栫暐

Agent 榛樿涓嶅簲鍒涘缓鎴栭噸澶嶅垱寤猴細

```text
BeginPlay
Tick
ConstructionScript
InputAction 鍏ュ彛
ActorBeginOverlap
ActorEndOverlap
ActorHit
鐢熷懡鍛ㄦ湡浜嬩欢
寮曟搸鍥炶皟浜嬩欢
```

榛樿搴斿垱寤哄畬鏁村懡鍚嶇殑 Custom Event锛屼緥濡傦細

```text
ShotBullet 鍔熻兘 鈫?Custom Event: ShotBullet
PhysicsDoor 鍔熻兘 鈫?Custom Event: TogglePhysicsDoor
```

濡傛灉闇€瑕佹帴鍏ュ凡鏈?BeginPlay / Tick / InputAction / Overlap锛屽簲浣跨敤 MergeBlueprintGraph锛屽苟鏄庣‘鎺ュ叆鐐广€?

---

## 2.8 鍛藉悕绛栫暐

鍑芥暟鍥惧懡鍚嶈鍒欑敱 Setup 閰嶇疆銆?

鐢ㄦ埛鏈寚瀹氬懡鍚嶅亸濂芥椂锛岄粯璁や娇鐢ㄦ弿杩板瀷鍛藉悕锛?

```text
TogglePhysicsDoor
InitializePhysicsDoor
ApplyDoorImpulse
ResetPhysicsDoor
```

涓嶄娇鐢細

```text
NewFunction
DoThing
杩囩煭娉涘悕
```

## 2.9 runtime_profile 浣跨敤瑙勫垯

姣忎釜鍐欏叆浠诲姟杩涘叆鍐欏叆闃舵鍓嶏紝Agent 蹇呴』璇诲彇涓€娆?runtime_profile銆?

runtime_profile 鐨勮亴璐ｆ槸鎻愪緵褰撳墠杩愯鏃朵簨瀹烇細

```text
Bridge / UE 鎻掍欢鐘舵€?
config_status
write_permission / Token 鐘舵€?
risk_command 鐘舵€?
active_profile.safety_profile
active_profile.missing_capability_policy
tool_capabilities unavailable_only 鍒楄〃
```

runtime_profile 涓嶈礋璐ｆ彁渚涳細

```text
瀹屾暣宸ュ叿璇存槑
瀹屾暣 CLI command contract
瀹屾暣鍛藉悕鍋忓ソ鍏ㄦ枃
瀹屾暣钃濆浘 / C++ 杈圭晫鍏ㄦ枃
Transaction / Review 鍘嗗彶
```

tool_capabilities 閲囩敤璐熷悜绋€鐤忔ā寮忥細

```json
{
  "tool_capabilities": {
    "mode": "unavailable_only",
    "unavailable": [
      {
        "cluster": "graph_write",
        "capability": "merge",
        "status": "unavailable",
        "reason": "not_implemented"
      }
    ]
  }
}
```

Agent 蹇呴』鐞嗚В锛?

```text
鏈嚭鐜板湪 unavailable 涓紝涓嶄唬琛?runtime_profile 宸插畬鏁寸‘璁ゅ叾 schema銆?
runtime_profile 涓嶆槸宸ュ叿绱㈠紩锛屼篃涓嶆槸 CLI command contract 鏂囨。銆?
鍏蜂綋宸ュ叿杈圭晫鏉ヨ嚜 AgentGuide / tools 鏂囨。锛涘叿浣撳弬鏁版潵鑷綋鍓?CLI command contract銆?
stop_and_report 鐢?Agent 鏍规嵁褰撳墠浠诲姟銆乵issing_capability_policy銆佷笉鍙敤鑳藉姏鍜屽畨鍏ㄦ浛浠ｈ矾寰勫垽鏂€?
```

## 2.10 diagnostics 杈圭晫

Diagnostics 鏄彧璇昏瘖鏂伐鍏凤紝鐢ㄤ簬瀹夎銆侀厤缃€丅ridge銆乺untime 閾捐矾闂瀹氫綅銆?

Diagnostics 杩斿洖 ToolResultBase 澶栧３锛屽疄闄呮姤鍛婂湪锛?

```text
data.markdown
```

Diagnostics 涓嶈繑鍥烇細

```text
blocking / warning / info JSON 鏁扮粍
```

Markdown 鍥哄畾鍖呭惈锛?

```md
## Blocking
...

## Warning
...
```

`## Info` 鍙€夈€?

濡傛灉 diagnostics 鍛戒护鑷韩鎵ц鎴愬姛锛屽嵆浣?Markdown 涓瓨鍦?Blocking锛屼篃搴旇繑鍥烇細

```text
ok=true
status=completed
```

Markdown Blocking 琛ㄧず璇婃柇鎶ュ憡鍙戠幇闃绘柇鐜鏉′欢锛屼笉琛ㄧず CLI 鍛戒护璋冪敤澶辫触銆傚彧鏈夛細

```text
ok=false
status=failed
```

鎵嶈〃绀?diagnostics 宸ュ叿鑷韩澶辫触銆?

---

# 3. Graph Write 宸ュ叿绨囨€昏

搴熷純鍚硦鐨?Import 鍛藉悕銆?

Graph Write 宸ュ叿绨囦娇鐢細

```text
AppendBlueprintGraph
ReplaceBlueprintGraph
PatchBlueprintGraph
MergeBlueprintGraph
```

鏃у伐鍏凤細

```text
blueprint_import_agent_graph
```

搴旀爣璁颁负 Deprecated / Legacy锛屽苟鎻愮ず浣跨敤鏄庣‘鍐欏叆宸ュ叿銆?

---

# 4. AppendBlueprintGraph

## 4.1 鏈€灏忚亴璐?

AppendBlueprintGraph 鍙礋璐ｏ細

```text
杩藉姞鏂扮殑鐙珛閫昏緫鍧椼€?
```

瀹冨彲浠ワ細

```text
鍒涘缓鏂扮殑 EG_{FeatureName} 浜嬩欢鍥捐〃
鍚戝凡鏈夊浘琛ㄨ拷鍔犵嫭绔嬮€昏緫鍧?
鍒涘缓鍞竴鍛藉悕鐨?Custom Event
鍒涘缓鏅€氳妭鐐?
鍒涘缓鏂拌妭鐐逛箣闂寸殑杩炵嚎
鍐欏叆 BlueprintHelper-owned Metadata + NodeComment
```

瀹冧笉鍙互锛?

```text
鑷姩杩炴帴宸叉湁鑺傜偣
鑷姩鎺ュ叆宸叉湁鎵ц娴?
瑕嗙洊鏃ц妭鐐?
鍒犻櫎鏃ц妭鐐?
娓呯悊鏃?block
淇敼鐢ㄦ埛鑺傜偣
鍒涘缓鍏ㄥ眬浜嬩欢鑺傜偣
鍒涘缓鍑芥暟鍥?
杩藉姞鍒板嚱鏁板浘锛圕laude Code Conservative 涓嬬姝級
```

---

## 4.2 鏂颁簨浠跺浘琛ㄨ鍒?

Claude Code Conservative 涓嬶細

```text
AppendBlueprintGraph 鍏佽鍒涘缓鏂扮殑 EG_{FeatureName}銆?
杩欐槸榛樿鎺ㄨ崘璺緞銆?
```

鍥捐〃鍚嶅啿绐佸鐞嗭細

```text
濡傛灉鍚屽悕鍥捐〃涓嶅瓨鍦細鍒涘缓鏂板浘琛ㄥ苟鍐欏叆銆?
濡傛灉鍚屽悕鍥捐〃宸插瓨鍦ㄤ笖涓虹┖锛氬厑璁哥户缁啓鍏ャ€?
濡傛灉鍚屽悕鍥捐〃宸插瓨鍦ㄤ笖闈炵┖锛氳繑鍥?error銆?
涓嶈嚜鍔ㄦ敼鍚嶃€?
```

---

## 4.3 Custom Event 瑙勫垯

Append 鍒涘缓 Custom Event 鏃讹細

```text
浜嬩欢鍚嶅繀椤诲敮涓€銆?
閲嶅悕鐩存帴 error銆?
涓嶈嚜鍔ㄦ敼鍚嶃€?
```

Append 涓嶅厑璁稿垱寤哄叏灞€浜嬩欢鑺傜偣銆?

涓€娆?Append 鍙啓鍏ュ涓嫭绔?Custom Event锛屼緥濡傦細

```text
EG_PhysicsDoor
- InitializePhysicsDoor
- TogglePhysicsDoor
- OpenPhysicsDoor
- ClosePhysicsDoor
```

涓€涓?transaction_id 瀵瑰簲璇ユ Append 鍐欏伐鍏疯皟鐢ㄣ€?

姣忎釜鐙珛 Custom Event 閫昏緫鍏ュ彛鐢熸垚涓€涓?block_id锛?

```text
EG_PhysicsDoor_InitializePhysicsDoor0
EG_PhysicsDoor_TogglePhysicsDoor0
EG_PhysicsDoor_OpenPhysicsDoor0
EG_PhysicsDoor_ClosePhysicsDoor0
```

---

## 4.4 鍚屼竴 transaction 鍐呴儴璋冪敤

鍚屼竴娆?AppendBlueprintGraph write transaction 鍐咃細

```text
鍏佽澶氫釜鏂板缓 Custom Event 浜掔浉璋冪敤銆?
```

渚嬪锛?

```text
TogglePhysicsDoor
鈫?Branch bDoorOpen
  鈫?false: OpenPhysicsDoor
  鈫?true: ClosePhysicsDoor
```

杩欏彧灞炰簬鏂?EG 鍥捐〃鍐呴儴閫昏緫缁勭粐锛屼笉绛変簬鑷姩鎺ュ叆宸叉湁 BeginPlay / Tick / InputAction銆?

---

## 4.5 璋冪敤宸叉湁鍑芥暟鎴?Custom Event

Append 鍒涘缓鐨勯€昏緫鍏佽璋冪敤鍥捐〃澶栧凡鏈夊嚱鏁版垨宸叉湁 Custom Event锛屼絾蹇呴』楠岃瘉锛?

```text
鐩爣瀛樺湪
绛惧悕/鍙傛暟鍖归厤
Pin 鍙繛鎺?
```

濡傛灉鐩爣涓嶅瓨鍦細

```text
鐩存帴 error
modified=false
涓嶈嚜鍔ㄥ垱寤虹己澶卞嚱鏁?
涓嶈嚜鍔ㄥ垱寤虹己澶变簨浠?
涓嶇户缁啓鍏ユ湭杩炴帴鑺傜偣
```

Append 鍙厑璁糕€滆皟鐢ㄢ€濆凡鏈夊嚱鏁?浜嬩欢锛屼笉鍏佽淇敼瀹冧滑鐨勫疄鐜般€?

---

## 4.6 浜嬪姟寮忓啓鍏?

AppendBlueprintGraph 蹇呴』浜嬪姟寮忓啓鍏ャ€?

濡傛灉鍐欏叆杩囩▼涓嚭鐜帮細

```text
閮ㄥ垎鑺傜偣鍒涘缓鎴愬姛浣嗗悗缁繛绾垮け璐?
UE/K2/Schema 妫€鏌ュけ璐?
鐩爣鍑芥暟涓嶅瓨鍦?
Pin 绫诲瀷涓嶅尮閰?
Custom Event 閲嶅悕
```

搴旀暣浣撳洖婊氾紝涓嶇暀涓嬪崐鎴愬搧鑺傜偣鎴?broken block銆?

澶辫触杩斿洖蹇呴』鍖呭惈锛?

```text
error_code
message
failed_stage
failed_node / failed_link
conflicts
modified=false
rollback_result
recommended_next_actions
```

澶辫触璇︾粏淇℃伅涓嶅彈 verbose 鎺у埗銆?

---

## 4.7 dry_run

Append 蹇呴』鏀寔 dry_run銆?

dry_run 涓嶅緱淇敼钃濆浘銆?

dry_run 鏀寔锛?

```text
quick
full
```

quick dry_run锛?

```text
鍙傛暟鏍￠獙
鏉冮檺妫€鏌?
鐩爣璧勪骇/鍥捐〃妫€鏌?
鍥捐〃椋庨櫓鍒ゆ柇
鍛藉悕鍐茬獊妫€鏌?
鍏ㄥ眬浜嬩欢绂佺敤妫€鏌?
鐩爣鍑芥暟/浜嬩欢瀛樺湪鎬ф鏌?
```

full dry_run锛?

```text
鍖呭惈 quick 鍏ㄩ儴妫€鏌?
灏藉彲鑳芥ā鎷熻妭鐐瑰垱寤?
妯℃嫙 Pin 杩炴帴
Schema / K2 鍚堟硶鎬ф鏌?
瀛ょ珛鎵ц娴佹鏌?
涓嶈惤鐩樸€佷笉淇敼钃濆浘
```

dry_run 妯″紡鐢憋細

```text
Setup 榛樿绛栫暐
Agent 鏄惧紡鍙傛暟
宸ュ叿椋庨櫓鍒ゆ柇
```

鍏卞悓鍐冲畾銆?

宸ュ叿鍙洜楂橀闄╄嚜鍔ㄥ皢 quick 鍗囩骇涓?full銆?

---

## 4.8 鎴愬姛杩斿洖

Append 姝ｅ紡鍐欏叆鎴愬姛鍚庯紝Agent-facing 杩斿洖閲囩敤鏋佺畝鍙ｅ緞锛屽彧淇濈暀鍚庣画鎿嶄綔蹇呴渶鐨?handle锛?

```text
ok
status
modified
target.asset_path
target.graph
data.append_result.graph.graph_id
data.append_result.graph.graph_name
data.append_result.block_refs[]
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation.should_compile
validation.should_save
validation.compiled
validation.saved
```

`block_refs` 鏄?string 鏁扮粍锛屼笉杩斿洖 block 瀵硅薄蹇収銆?

瀹屾暣 block_id 鍙嶆帹瑙勫垯锛?

```text
full_block_id = graph_id + "_" + block_ref
```

`transaction_id` 鍙繑鍥炵粰 Agent锛屽洜涓?Graph Write 浜х敓鐨?block / rollback / review 甯搁渶瑕佸悗缁紩鐢紱浣嗚繖涓嶆槸鎵€鏈夋櫘閫氬啓宸ュ叿鐨勯€氱敤杩斿洖瑕佹眰銆?

Append 鎴愬姛涓嶉粯璁よ繑鍥烇細

```text
summary
created_blocks / created_nodes / created_links / created_variables 璁℃暟
called_existing_functions / called_existing_events 璁℃暟
blocks[].entry_type
blocks[].entry_name
ownership
review
safety
diagnostics
next
```

涓婅堪璁℃暟涓庡畬鏁?diff 杩涘叆 Transaction Journal / Review / verbose/debug銆?

---

# 5. ReplaceBlueprintGraph

## 5.1 鏈€灏忚亴璐?

ReplaceBlueprintGraph 鐨勬渶灏忚亴璐ｏ細

```text
鏇挎崲涓€涓槑纭洰鏍囩殑瀹屾暣瀹炵幇銆?
```

鐩爣鍙互鏄細

```text
block_id
function
custom_event
event
graph
```

Replace 涓嶅厑璁革細

```text
妯＄硦鍖归厤鍚屽悕閫昏緫骞惰嚜鍔ㄦ浛鎹?
鐩爣涓嶆槑纭椂缁х画鍐欏叆
鑷姩鐚滅敤鎴锋兂鏇挎崲鍝釜瀹炵幇
```

---

## 5.2 鐢ㄦ埛鎵嬪啓鐩爣

Replace 鏇挎崲鐢ㄦ埛鎵嬪啓鍑芥暟/鍥捐〃鏃讹紝榛樿鍏佽鐨勫墠鎻愭槸锛?

```text
鐢ㄦ埛鏄庣‘鎸囧畾鐩爣鍑芥暟鎴栫洰鏍囧浘琛ㄣ€?
```

鐩爣涓嶆槑纭€佺敤鎴锋湭鎺堟潈淇敼宸叉湁鐢ㄦ埛鑺傜偣銆佹垨浼氬奖鍝嶇洰鏍囪寖鍥村鐢ㄦ埛鑺傜偣鏃讹紝搴斿仠姝㈠苟鎶ュ憡銆?

Claude Code Conservative 榛樿绛栫暐锛?

```text
review_only
```

鍗筹細

```text
涓嶇敓鎴?block_id
涓嶆帴绠?BlueprintHelper ownership
璁板綍 transaction_id
璁板綍 before / after diff
杩涘叆 Review
鏀寔瀹￠槄銆佸洖婊氥€佸璁?
```

濡傛灉鐢ㄦ埛鏄庣‘瑕佹眰鈥滀互鍚庝氦缁?BlueprintHelper 绠＄悊鈥濓紝鍙互鐢熸垚 block_id 骞舵帴绠?ownership锛屼絾蹇呴』 dry_run 鏄庣‘鎻愮ず ownership 灏嗘敼鍙樸€?

---

## 5.3 dry_run / replace plan

Replace 鏇挎崲浠讳綍宸叉湁鐩爣鍓嶉兘蹇呴』 dry_run銆?

replace plan 搴旀槑纭細

```text
will_delete_nodes
will_delete_links
will_create_nodes
will_create_links
will_preserve_nodes
will_modify_nodes
will_reuse_nodes
affected_user_nodes
affected_blueprinthelper_blocks
external_dependents
external_dependencies
target_scope
can_execute
recommended_next_actions
```

---

## 5.4 鍐呴儴瀹炵幇鍙渶灏?diff

Replace 瀵瑰璇箟鏄浛鎹㈢洰鏍囧畬鏁村疄鐜般€?

鎻掍欢鍐呴儴涓嶅己鍒跺繀椤绘暣浣撳垹闄ゆ棫鑺傜偣鍐嶉噸寤猴紝鍙互鍋氾細

```text
鏈€灏?diff
鍘熷湴鏇存柊
鑺傜偣澶嶇敤
甯冨眬淇濇寔
```

浣?dry_run / replace plan 蹇呴』鏄庣‘鍝簺鑺傜偣浼氾細

```text
鍒犻櫎
淇濈暀
澶嶇敤
淇敼
鏂板缓
```

---

## 5.5 block_id 澶勭悊

鏇挎崲 BlueprintHelper-owned block 鏃讹細

```text
淇濈暀鍘?block_id銆?
```

鍥犱负 Replace 琛ㄧず鍚屼竴閫昏緫鍧楃殑鏂扮増鏈紝涓嶆槸鍒涘缓鏂伴€昏緫鍧椼€?

鍙樺寲閫氳繃锛?

```text
鏂扮殑 transaction_id
鏂扮殑 operation_id
Transaction Journal 涓殑鏂扮増鏈褰?
before / after diff
```

琛ㄨ揪銆?

鏇挎崲鐢ㄦ埛鎵嬪啓鐩爣鏃讹紝鏄惁鐢熸垚 block_id / 鏄惁鎺ョ ownership锛岀敱鐢ㄦ埛 / Setup 鍐冲畾銆?

---

## 5.6 replace_scope

Replace 蹇呴』鏄惧紡鍖哄垎锛?

```text
block_implementation
function_body
event_body
function_definition
event_definition
graph
```

鍚箟锛?

```text
block_implementation锛?
鏇挎崲 BlueprintHelper-owned block 鐨勫疄鐜帮紝淇濈暀鍘?block_id銆?

function_body锛?
淇濈暀鍑芥暟鍏ュ彛銆佺鍚嶃€佸閮ㄥ彲璋冪敤韬唤锛屽彧鏇挎崲鍐呴儴鑺傜偣/杩炵嚎銆?

event_body锛?
淇濈暀浜嬩欢鍏ュ彛銆佸悕绉般€佸弬鏁般€佸閮ㄥ彲璋冪敤韬唤锛屽彧鏇挎崲鍐呴儴閫昏緫銆?

function_definition锛?
鏇挎崲鎴栭噸寤哄嚱鏁版湰浣擄紝鍙兘鏀瑰彉 UUID銆佺鍚嶃€佸叆鍙ｆ垨澶栭儴寮曠敤銆?

event_definition锛?
鏇挎崲鎴栭噸寤轰簨浠舵湰浣擄紝鍙兘褰卞搷澶栭儴璋冪敤鏂广€?

graph锛?
鏇挎崲鏄庣‘鎸囧畾鍥捐〃鑼冨洿鍐呯殑瀹屾暣瀹炵幇锛岄珮椋庨櫓銆?
```

---

## 5.7 澶栭儴渚濊禆瑙勫垯

濡傛灉 Replace 鐨勭洰鏍囨槸鍑芥暟鎴栦簨浠舵湰浣擄細

```text
閬囧埌 external_dependents 榛樿闃绘骞舵姤鍛娿€?
```

鍘熷洜鏄浛鎹㈠悗钃濆浘鍐呴儴 UUID銆佽妭鐐瑰紩鐢ㄦ垨鍏朵粬鏁版嵁鍙兘鍙樺寲锛屽閮ㄨ皟鐢ㄦ柟鍙兘澶辨晥锛岀紪璇戞垨杩愯鏃舵姤閿欍€?

濡傛灉 Replace 鐨勭洰鏍囨槸鍑芥暟鎴栦簨浠跺唴閮ㄩ€昏緫瀹炵幇锛?

```text
涓嶅洜 external_dependents 鐩存帴闃绘銆?
dry_run 蹇呴』鎶ュ憡 external_dependents銆?
鑻ョ鍚嶃€佸叆鍙ｈ韩浠姐€佽皟鐢?Pin 鎴栧閮ㄥ紩鐢ㄤ細鍙樺寲锛屽垯闃绘銆?
```

---

## 5.8 Conservative 鑷姩鎵ц绛栫暐

Claude Code Conservative 涓嬪彲鑷姩鎵ц锛?

```text
block_implementation
function_body
event_body
```

鍓嶆彁锛?

```text
鐢ㄦ埛鏄庣‘鎸囧畾鐩爣
dry_run 鏃?error / conflict
涓嶆敼鍙樺叆鍙ｈ韩浠?
涓嶆敼鍙樼鍚?
涓嶇牬鍧忓閮ㄨ皟鐢ㄦ柟
```

涓嶅彲鑷姩鎵ц锛?

```text
function_definition
event_definition
graph
```

---

## 5.9 function_body / event_body

鏇挎崲 function_body / event_body 鏃讹紝鍏佽鍒犻櫎骞堕噸寤哄唴閮ㄦ櫘閫氳妭鐐瑰拰杩炵嚎锛屼絾蹇呴』淇濈暀锛?

```text
鍑芥暟鍏ュ彛 / 浜嬩欢鍏ュ彛鏈綋
鍑芥暟绛惧悕
鍙傛暟 Pin
杩斿洖鍊?Pin
澶栭儴鍙皟鐢ㄨ韩浠?
澶栭儴璋冪敤寮曠敤绋冲畾鎬?
```

鎵€鏈夎鍒犻櫎銆佹浛鎹€佷慨鏀广€佹柇寮€杩炴帴鐨勭敤鎴锋墜鍐欏唴閮ㄨ妭鐐瑰拰杩炵嚎锛屽繀椤昏繘鍏?Review diff銆?

濡傛灉鐩爣鍐呴儴娣锋湁 BlueprintHelper-owned 鑺傜偣鍜岀敤鎴锋墜鍐欒妭鐐癸紝鍏佽鏇挎崲鏁翠釜 body锛屼絾 Review diff 蹇呴』鍖哄垎 owned 鑺傜偣鍜岀敤鎴疯妭鐐广€?

鍐呴儴鏃?block_id 鎸夊鐢ㄥ叧绯诲鐞嗭細

```text
涓€瀵逛竴澶嶇敤 / 鍘熷湴鏇存柊锛氫繚鐣欏師 block_id
鍒犻櫎 / 閲嶅缓 / 鏃犳硶绋冲畾瀵瑰簲锛氬簾寮冩棫 block_id锛屾柊閫昏緫鍧楃敓鎴愭柊 block_id
```

---

## 5.10 鎴愬姛杩斿洖

Replace 姝ｅ紡鍐欏叆鎴愬姛鍚庯紝Agent-facing 杩斿洖閲囩敤鏋佺畝鍙ｅ緞锛?

```text
ok
status
modified
target.asset_path
target.graph
target.replace_scope
data.replace_result.target.graph_id
data.replace_result.target.target_ref
data.replace_result.target.target_kind
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation.should_compile
validation.should_save
```

Replace 鎴愬姛涓嶉粯璁よ繑鍥烇細

```text
summary
deleted_nodes / created_nodes / modified_nodes / preserved_nodes 璁℃暟
before / after
full_diff
ownership
review
safety
diagnostics
next
```

鏇挎崲 BlueprintHelper-owned block 鏃朵繚鐣欏師 block_id / block_ref銆傛浛鎹㈢敤鎴锋墜鍐欑洰鏍囨椂榛樿涓嶆帴绠?ownership锛屼笉鐢熸垚 block_ref銆?

---

# 6. PatchBlueprintGraph

## 6.1 鏈€灏忚亴璐?

PatchBlueprintGraph 鐨勬渶灏忚亴璐ｏ細

```text
绮剧‘淇敼涓€涓槑纭洰鏍囩偣銆?
```

Patch 蹇呴』瀹氫綅鍒帮細

```text
鍏蜂綋鑺傜偣
鍏蜂綋 Pin
鍏蜂綋榛樿鍊?
鍏蜂綋灞炴€?
鍏蜂綋杩炴帴
```

涓嶅厑璁告牴鎹嚜鐒惰瑷€鎻忚堪妯＄硦鏌ユ壘骞剁洿鎺ヤ慨鏀广€?

鐩爣鏃犳硶鍞竴瀹氫綅鏃讹紝杩斿洖 error锛屽苟寤鸿鍏堣鍙?LogicJson / LogicMD 鎴栦娇鐢?Replace / Merge銆?

---

## 6.2 expected_old_value

Patch 涓嶅己鍒舵墍鏈夊満鏅兘鎼哄甫 expected_old_value / expected_old_state銆?

蹇呴』鎴栧缓璁惡甯︼細

```text
鐢ㄦ埛鎵嬪啓鑺傜偣
楂橀闄╀慨鏀?
杩炴帴鍏崇郴淇敼
褰卞搷鎵ц娴佺殑 Pin
鐩爣瀛樺湪澶氫箟鎬?
```

鍙渷鐣ワ細

```text
BlueprintHelper-owned 鑺傜偣
鐩爣瀹氫綅鏄庣‘
浣庨闄╅粯璁ゅ€间慨鏀?
old/new value 鏄暱鏂囨湰锛岄噸澶嶄紶杈?Token 鎴愭湰楂?
```

鍗充娇鐪佺暐 expected_old_value锛屽伐鍏蜂粛搴旓細

```text
鎵ц鍓嶈鍙栧綋鍓嶇姸鎬?
Journal / Review 涓褰?before / after
榛樿 Agent-facing 鎴愬姛缁撴灉涓嶈繑鍥?before_summary / after_summary
verbose/debug 鍙繑鍥炲畬鏁?before/after diff
```

---

## 6.3 鐩爣瀹氫綅

Patch 鐩爣瀹氫綅浼樺厛浣跨敤锛?

```text
BlueprintHelper block_id + LogicJson 灞€閮ㄨ矾寰?
```

涓嶈冻鏃跺啀浣跨敤锛?

```text
UE 鍘熺敓 node GUID / pin GUID
```

鑺傜偣鏄剧ず鍚?/ Pin 鍚嶄笉鑳藉崟鐙綔涓虹ǔ瀹氬畾浣嶄緷鎹€?

Patch 鐨?node_path / pin_path / link_path 搴斾笌 LogicJson schema 淇濇寔涓€鑷淬€?

褰撳墠 LogicJson 璺緞瑙勫垯锛?

```text
target_graph / blueprint / multi_target 浣跨敤 logic.groups[]锛屼笉鏄崟鍏ュ彛 entry + nodes銆?
target_block / target_function / target_event / target_custom_event / target_node / target_pin 鍙娇鐢?entry + nodes 绠€鍐欍€?
鏅€氳妭鐐归粯璁よ繑鍥?node_ref锛屼笉榛樿杩斿洖瀹屾暣 node_path銆?
杩炴帴榛樿杩斿洖 link_ref锛屼笉榛樿杩斿洖瀹屾暣 link_path銆?
node_ref / link_ref 鍙湪褰撳墠 group 鍐呮湁鏁堛€?
濡傛灉宸ュ叿闇€瑕佸畬鏁?node_path / link_path锛孉gent 搴斾粠 group.entry.node_path 鍙嶆帹銆?
links 瀛樺湪 source node 鐨?node.links 鍐咃紝琛ㄧず outgoing links锛汱ogicJson 涓嶈繑鍥為《灞?logic.links銆?
```


---

## 6.4 鎴愬姛杩斿洖

Patch 姝ｅ紡鍐欏叆鎴愬姛鍚庯紝Agent-facing 杩斿洖閲囩敤鏋佺畝鍙ｅ緞锛?

```text
ok
status
modified
target.asset_path
target.graph
target.patch_scope
data.patch_result.target
data.patch_result.patch.patch_type
data.patch_result.patch.expected_old_state_provided
data.patch_result.patch.changed
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation.should_compile
validation.should_save
```

Patch 鎴愬姛涓嶉粯璁よ繑鍥烇細

```text
summary
modified_nodes / modified_pins / created_links / deleted_links 璁℃暟
before / after
old_value / new_value
patch_plan
full_diff
ownership
review
safety
diagnostics
next
```

Patch 鐩爣蹇呴』浣跨敤鏄庣‘ `node_path / pin_path / link_path`锛屾垨浣跨敤鍙粠 LogicJson group 鍙嶆帹瀹屾暣璺緞鐨勫眬閮ㄥ紩鐢ㄣ€備粎闈犳樉绀哄悕 / Pin 鍚嶄笉鍏佽鐩存帴淇敼銆?

---

# 7. MergeBlueprintGraph

## 7.1 鏈€灏忚亴璐?

MergeBlueprintGraph 鐨勬渶灏忚亴璐ｏ細

```text
鎶婃柊閫昏緫鎺ュ叆宸叉湁鎵ц娴併€?
```

Merge 涓撻棬璐熻矗灏嗭細

```text
BlueprintHelper 閫昏緫鍧?
鍑芥暟璋冪敤
Custom Event 璋冪敤
```

鎺ュ叆鏄庣‘鎸囧畾鐨勫凡鏈夋墽琛岄摼锛屼緥濡傦細

```text
BeginPlay
DoInteract Override
InputAction
Overlap
宸叉湁鍑芥暟鍏ュ彛
宸叉湁 Branch 鍒嗘敮
```

Merge 涓嶈礋璐?Append / Replace / Patch銆?

---

## 7.2 鎺ュ叆鐐硅姹?

Merge 蹇呴』瑕佹眰鏄庣‘鎺ュ叆鐐癸紝鍚﹀垯 error銆?

璋冪敤鏂瑰繀椤绘彁渚涳細

```text
鐩爣鍥捐〃
鐩爣鑺傜偣鎴栫ǔ瀹氬畾浣嶈矾寰?
鐩爣 Pin
鎻掑叆绛栫暐
```

鎺ュ叆鐐瑰畾浣嶄紭鍏堬細

```text
LogicJson node_path / pin_path
```

濡傛灉璇诲彇鐨勬槸 target_graph / blueprint / multi_target 鑼冨洿锛孉gent 蹇呴』鍏堥€夋嫨鏄庣‘ group锛屽啀浣跨敤璇?group 鍐呯殑 node_ref / link_ref 鍙嶆帹瀹屾暣璺緞锛涗笉寰楄法 group 浣跨敤灞€閮ㄥ紩鐢ㄣ€?

涓嶈冻鏃朵娇鐢細

```text
UE node GUID / pin GUID
```

鑺傜偣鏄剧ず鍚?+ Pin 鍚嶅彧鑳戒綔涓鸿緟鍔╂樉绀轰俊鎭€?

---

## 7.3 insert_strategy

鎻掑叆宸叉湁 Exec 閾炬椂锛屽繀椤荤敱 Agent 鏄惧紡鎸囧畾 insert_strategy銆?

鑷冲皯鍖呮嫭锛?

```text
append_after
insert_between
branch_fork
```

涓嶅悓绛栫暐浼氭敼鍙樻墽琛岄『搴忓拰鍓綔鐢紝涓嶈兘榛樿鐚溿€?

---

## 7.4 append_after

濡傛灉 target Exec Pin 宸叉湁鍚庣户杩炴帴锛?

```text
鐩存帴 error銆?
```

涓嶈嚜鍔ㄦ敼鎴?insert_between 鎴?branch_fork銆?

---

## 7.5 insert_between

insert_between 鍏佽锛?

```text
鏂紑鐩爣 Exec Pin 鐨勬棦鏈夎繛鎺?
鎻掑叆鏂伴€昏緫
閲嶆帴鍘熷悗缁?
```

鍓嶆彁锛?

```text
鏄庣‘ target_graph
鏄庣‘ target_node / node_path
鏄庣‘ target_pin / pin_path
鏄庣‘ insert_strategy=insert_between
蹇呴』 dry_run
```

dry_run / merge plan 蹇呴』灞曠ず锛?

```text
灏嗘柇寮€鐨勬棫杩炴帴
灏嗘柊澧炵殑鑺傜偣鎴栬皟鐢?
灏嗘柊寤虹殑杩炴帴
鍘熷悗缁у浣曢噸鎺?
鎵ц椤哄簭鍙樺寲
鏄惁娑夊強鐢ㄦ埛鑺傜偣
```

---

## 7.6 branch_fork

branch_fork 浣跨敤鏃讹紝宸ュ叿搴旇嚜鍔ㄦ彃鍏ワ細

```text
Sequence 鑺傜偣鎴栫瓑浠峰垎鍙戣妭鐐?
```

涓嶈兘鍋囪 Exec Pin 鏀寔澶氬悗缁с€?

Sequence 鐨勯『搴忓繀椤荤敱 Agent 鏄惧紡鎸囧畾锛?

```text
鍘熷悗缁ф帴 Then0 杩樻槸 Then1
鏂伴€昏緫鎺?Then0 杩樻槸 Then1
```

dry_run / merge plan 蹇呴』灞曠ず锛?

```text
灏嗘彃鍏ョ殑 Sequence 鑺傜偣
鍘熻繛鎺ヨ縼绉讳綅缃?
鏂伴€昏緫杩炴帴浣嶇疆
sequence_order / branch_order
鎵ц椤哄簭鍙樺寲
鏄惁褰卞搷鐢ㄦ埛鑺傜偣
```

---

# 8. Ownership / Cleanup 宸ュ叿绨?

## 8.1 宸ュ叿娓呭崟

Cleanup / Ownership 宸ュ叿绨囧垵鐗堬細

```text
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
RollbackCleanupTransaction
ConvertBlueprintHelperBlockToUserOwned
```

dry_run 浣滀负鍙傛暟锛屼笉鍗曠嫭鎷嗗伐鍏枫€?

---

## 8.2 CleanupBlueprintHelperBlock

鍙帴鍙楁槑纭?block_id銆?

涓嶆帴鍙楋細

```text
妯＄硦鍚嶇О
entry_name
graph / event_name 澶氬瓧娈靛畾浣?
```

濡傛灉 block_id 涓嶅瓨鍦紝鐢卞弬鏁版帶鍒讹細

```text
missing_policy = error | ignore
```

榛樿锛?

```text
missing_policy=error
```

鎭㈠銆佹壒澶勭悊銆侀噸澶嶆竻鐞嗗満鏅彲鐢細

```text
missing_policy=ignore
```

浣?ignore 鍙鐞?block 涓嶅瓨鍦紝涓嶆帺鐩?ownership 鍐茬獊銆佷緷璧栧啿绐併€佺敤鎴疯妭鐐瑰啿绐佹垨 rollback 鐘舵€佷笉鍖归厤銆?

---

## 8.3 CleanupBlueprintHelperFeature

鏀寔鎸夊姛鑳界粍娓呯悊锛屼絾蹇呴』 dry_run銆?

澶氬瓧娈佃仈鍚堝尮閰嶏細

```text
feature_name
feature_group
graph 鍚?
block_id 鍓嶇紑
Transaction Journal 璁板綍
target_asset
鍏ュ彛浜嬩欢鍚?/ 鍑芥暟鍚?
```

鍖归厤缃俊搴︼細

```text
high
medium
low
```

榛樿姝ｅ紡 cleanup 鍙嚜鍔ㄥ垹闄?high 缃俊搴﹀尮閰嶃€?

medium / low 闇€瑕佺敤鎴锋槑纭‘璁ゃ€?

dry_run 蹇呴』杩斿洖榛樿鎵ц璁″垝锛?

```text
will_delete
will_keep
requires_confirmation
blocked_by
external_dependencies
external_dependents
confidence 鍒嗙粍
would_delete_nodes
would_delete_links
can_execute
recommended_next_actions
```

---

## 8.4 Cleanup 渚濊禆鏂瑰悜

蹇呴』鍖哄垎锛?

```text
external_dependents锛?
澶栭儴璋佹鍦ㄤ緷璧栫洰鏍?block / 鍔熻兘缁勩€?
鍒犻櫎鐩爣鍙兘鐮村潖澶栭儴璋冪敤鏂广€?

external_dependencies锛?
鐩爣 block / 鍔熻兘缁勬鍦ㄤ緷璧栧摢浜涘閮?block銆?
鍒犻櫎鐩爣涓嶄細鐮村潖杩欎簺澶栭儴 block銆?
```

绛栫暐锛?

```text
澶栭儴渚濊禆鐩爣锛氶渶瑕佺敤鎴风‘璁ゆ垨鍋滄鎶ュ憡銆?
鐩爣渚濊禆澶栭儴锛歋tandard / AutoRepair 鍙垹闄ょ洰鏍囧苟淇濈暀澶栭儴渚濊禆銆?
```

浠讳綍 cleanup 鍙兘鍒犻櫎 BlueprintHelper-owned block锛屼笉鑳藉垹闄ょ敤鎴锋墜鍐欒妭鐐规垨鏉ユ簮涓嶆槑鑺傜偣銆?

---

## 8.5 Cleanup 鎵ц涓?Review

Cleanup 姝ｅ紡鎵ц鍚庡繀椤伙細

```text
鐢熸垚 transaction_id
鍐欏叆 Transaction Journal
璁板綍 rollback_data
杩涘叆 Review 闃熷垪
```

鍗充娇鍙垹闄?BlueprintHelper-owned 鑺傜偣锛屼篃灞炰簬鐮村潖鎬у啓鎿嶄綔銆?

Cleanup Review 琚嫆缁濇椂锛?

```text
涓嶅湪 Agent 鎵ц闂幆涓嚜鍔?rollback銆?
鍙爣璁?rejected / needs_action锛屾垨鐢?Review UI 鐨勬樉寮?Reject / rollback 娴佺▼澶勭悊銆?
Agent 涓嶇瓑寰呯敤鎴?Review Accept / Reject锛屼篃涓嶆妸 Review 鐘舵€佷綔涓烘湰杞换鍔″畬鎴愭潯浠躲€?
```

---

## 8.6 ConvertBlueprintHelperBlockToUserOwned

鏀寔锛?

```text
鍗?block 杞崲
feature 鎵归噺杞崲
```

feature 鎵归噺蹇呴』 dry_run銆?

鎵ц鍚庯細

```text
绉婚櫎鎴栨敼鍐?BlueprintHelper ownership metadata
娓呯悊鎴栬浆鎹?NodeComment
Journal 澶勭悊鏂瑰紡鐢?Setup 閰嶇疆
杩涘叆 Review 闃熷垪
```

Review 閫氳繃鍓嶅彲浠?rollback銆?

Review 閫氳繃骞跺綊妗ｅ悗锛岄粯璁や笉鍐嶈嚜鍔ㄦ仮澶?ownership銆?

鏆備笉瑙勫垝閲嶆柊鎺ョ鐢ㄦ埛鑺傜偣宸ュ叿銆?

---

# 9. Transaction Journal / Review 鏁版嵁璁捐

## 9.1 鑺傜偣 Metadata

鑺傜偣 Metadata 鍙繚瀛樻渶灏?ownership 绱㈠紩锛?

```json
{
  "BlueprintHelperOwned": true,
  "BlueprintHelperBlockId": "EG_PhysicsDoor_TogglePhysicsDoor0",
  "BlueprintHelperTransactionId": "tx_20260501_0007",
  "BlueprintHelperTool": "AppendBlueprintGraph",
  "BlueprintHelperFeatureName": "TogglePhysicsDoor"
}
```

瀹屾暣渚濊禆銆乨iff銆乨iagnostics 涓嶅簲鍏ㄩ儴鍐欏叆鑺傜偣 metadata銆?

---

## 9.2 NodeComment

NodeComment 鐢ㄤ簬浜虹被瀹℃煡锛?

```text
[BlueprintHelper]
block_id=EG_PhysicsDoor_TogglePhysicsDoor0
tx=tx_20260501_0007
tool=AppendBlueprintGraph
```

宸ュ叿鍒ゆ柇 ownership 鏃朵互 Metadata 涓哄噯锛孨odeComment 鍙綔杈呭姪銆?

---

## 9.3 Transaction Journal

瀹屾暣鍐欏叆璁板綍鏀惧湪锛?

```text
<Project>/Saved/BlueprintHelper/Transactions/
```

Transaction Journal 璁板綍锛?

```text
transaction_id
tool
status
target_assets
operations
blocks
created_nodes
created_links
references_block_ids
references_events
diagnostics
validation
rollback_data
review_status
```

瀹℃煡閫氳繃鍚庢槸鍚﹀帇缂╃敱 Setup 鍐冲畾锛?

```text
KeepFull
CompactToSummary
DeleteJournalKeepMetadata
```

榛樿淇濈暀鑺傜偣 Metadata / NodeComment锛岄櫎闈炵敤鎴锋槑纭?ConvertToUserOwned銆?

---

# 10. Read / Logic 宸ュ叿瑙勫垝鍚屾

## 10.1 LogicMD 榛樿闃呰瑙勫垯

LogicMD 鏄?Agent 榛樿钃濆浘閫昏緫闃呰鏍煎紡锛岀敤浜庡揩閫熺悊瑙ｈ摑鍥惧仛浜嗕粈涔堛€佹湁鍝簺鍏ュ彛銆佸摢浜涢€昏緫灞炰簬 BlueprintHelper-owned block銆佸摢浜涘睘浜庣敤鎴峰尯鍩熴€?

澶氬叆鍙?scope锛?

```text
target_graph
blueprint
multi_target
```

杩欎簺 scope 蹇呴』鎸?group 鍒嗘闃呰锛岃€屼笉鏄涓轰竴鏉¤繛缁墽琛屾祦銆?

澶氬叆鍙?LogicMD 杩斿洖锛?

```json
{
  "grouped": true
}
```

`grouped=true` 琛ㄧず Markdown 宸叉寜浠ヤ笅鍖哄煙鍒嗘锛?

```text
BlueprintHelper Block
User Region
Global Event Flow
Orphan Group
Unknown Group
```

Agent 涓嶅緱鎶婁笉鍚?group 鑷姩杩炴帴鎴愬悓涓€鏉℃墽琛岄摼銆?

鍗曞叆鍙?scope 閫氬父涓嶈繑鍥?grouped 瀛楁銆傚瓧娈电己澶变笉琛ㄧず `grouped=false`锛屽彧琛ㄧず褰撳墠 scope 涓嶉渶瑕佸垎缁勬爣璁般€?

## 10.2 LogicJson 绮剧‘鍒嗘瀽瑙勫垯

LogicJson 鏄?Patch / Merge / Replace / Cleanup 鍓嶇殑缁撴瀯鍖栧垎鏋愭牸寮忋€?

澶氬叆鍙?scope锛?

```text
target_graph
blueprint
multi_target
```

浣跨敤锛?

```text
logic.groups[]
```

鍗曞叆鍙?scope锛?

```text
target_block
target_function
target_event
target_custom_event
target_node
target_pin
```

鍙互浣跨敤锛?

```text
logic.entry + logic.nodes
```

姣忎釜 group 蹇呴』鏈?entry锛屼笖 `entry.node_path` 鏄綋鍓?group 鐨勫畬鏁磋妭鐐硅矾寰勫拰璺緞鍙嶆帹閿氱偣銆?

鏅€?node 榛樿鍙繑鍥烇細

```text
node_ref
```

鏅€?link 榛樿鍙繑鍥烇細

```text
link_ref
```

瑙勫垯锛?

```text
node_ref / link_ref 鍙湪褰撳墠 group 鍐呮湁鏁堬紝涓嶆槸鍏ㄥ眬璺緞銆?
links 瀛樺湪 source node 鐨?node.links 鍐咃紝琛ㄧず outgoing links銆?
link 鍐呬笉鍐?from_node锛屽洜涓?source node 灏辨槸褰撳墠 node銆?
to_node 鏄洰鏍?node_ref锛屼篃鍙湪褰撳墠 group 鍐呮湁鏁堛€?
绗竴鐗堜笉鎻愪緵 incoming_refs锛岄渶瑕佸弽鏌ュ叾浠?node 鐨?outgoing links銆?
LogicJson 榛樿杩斿洖璇箟 kind锛屼笉榛樿杩斿洖 UE K2Node 鍘熷绫诲悕銆?
LogicJson 蹇呴』 importable=false锛屼笉鍙綔涓哄鍏ユ牸寮忋€?
```

## 10.3 绮剧‘璇诲伐鍏疯鍒?

鏂板鎸夌洰鏍囪鍙栭€昏緫宸ュ叿锛?

```text
ReadBlueprintLogicJsonByTarget
ReadBlueprintLogicMdByTarget
```

杈撳叆锛?

```text
asset_path / target_blueprint
target_type: function / event / custom_event / graph / block
target_name 鎴?block_id
format: logic_json / logic_md
```

鎺ㄨ崘璇诲伐鍏峰垎灞傦細

```text
Global LogicMD锛氬揩閫熸悳绱㈠拰鍏ㄥ眬鐞嗚В
Target LogicMD锛氱悊瑙ｅ崟涓洰鏍?
Target LogicJson锛氱簿纭慨鏀瑰墠鍒嗘瀽
```

---

# 11. 鍚庣画浼樺厛绾?

褰撳墠娴嬭瘯鍚庣画淇浼樺厛绾э細

```text
E. 鍥捐〃鍐欏叆
F. 楠岃瘉閾捐矾
G. 寮曞閾捐矾
H. 閫氫俊鎭㈠
D. Override
C. 鎺ュ彛
B. 杈撳叆閾捐矾
A. 缁勪欢娣诲姞
```

褰撳墠鏂囨。涓昏瀹屾垚锛?

```text
E. 鍥捐〃鍐欏叆
Cleanup / Ownership
閮ㄥ垎 Transaction Journal / Review 鏁版嵁璁捐
```

鍚庣画寤鸿缁х画琛ラ綈鎴栧紩鐢ㄥ凡鎷嗗垎鏂囨。锛?

```text
Validation / Diagnostics 宸ュ叿绨囷細閬靛惊 data.markdown diagnostics 瑙勫垯銆?
Preflight / Setup 宸ュ叿绨囷細浠?runtime_profile + active_profile 涓哄啓鍏ラ樁娈典簨瀹炴潵婧愩€?
Interface / Override 宸ュ叿绨囷細Class Settings 鍙坊鍔?Implemented Interface锛涙帴鍙ｅ嚱鏁板疄鐜颁綋浠嶄氦缁?Graph Write銆?
Enhanced Input 宸ュ叿绨囷細褰撳墠闃舵榛樿涓嶇紪杈?IA / IMC锛屽彧寮曠敤鐢ㄦ埛宸叉湁 IA 骞堕€氳繃 Graph Write 鍒涘缓浜嬩欢鍏ュ彛銆?
Component 宸ュ叿绨囷細add_component 鍙垱寤虹粍浠跺拰 attachment锛泃ransform / collision / physics / mesh / material 鍧囪蛋 property 鍐欏叆銆?
```
---

# 2026-05-04 娣峰悎 TaskSpec / TaskPlan 鏋舵瀯鍚屾

## 鍚屾缁撹

Graph Write 宸ュ叿绨囦笉鎺ㄧ炕銆侫ppend / Replace / Patch / Merge 鐨勮竟鐣岀户缁綔涓哄簳灞傝兘鍔涜竟鐣岋紝浣嗘櫘閫?Agent 涓嶅啀鐩存帴鎵嬪姩閫夋嫨鍜屾嫾瑁呮墍鏈?Graph Write 宸ュ叿銆?

鏂板彛寰勶細

```text
Agent-facing锛歍askSpec 鎻忚堪鐩爣銆佽寖鍥淬€佽涓哄拰闆嗘垚绛栫暐銆?
Python / CLI锛歍askSpec 鈫?TaskPlan锛屽喅瀹氫娇鐢?Append / Replace / Patch / Merge銆?
UE Task Runtime锛氭墽琛?TaskPlan 涓殑 Graph Write step銆?
Graph Write 鑳藉姏绨囷細淇濇寔鍘熻涔夎竟鐣屽拰瀛楁鍗忚銆?
```

## 鏂板 TaskSpec / TaskPlan 鍏崇郴

TaskSpec 鏄?Agent-facing 璇箟瑙勬牸锛屼緥濡傦細

```json
{
  "task_type": "create_blueprint_feature",
  "feature_name": "PhysicsDoor",
  "target": { "asset_path": "/Game/BP/BP_Door" },
  "scope_policy": {
    "prefer_new_graph": true,
    "graph_name": "EG_PhysicsDoor",
    "allow_modify_user_nodes": false,
    "allow_merge_existing_execution_flow": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": []
  }
}
```

TaskPlan 鏄?Task Compiler 杈撳嚭缁?UE Task Runtime 鐨勬墽琛岃鍒掞紝渚嬪锛?

```json
{
  "schema": "BlueprintHelper.TaskPlan.v1",
  "steps": [
    {
      "step_id": "step_graph_001",
      "capability": "graph_write",
      "target": {
        "asset_path": "/Game/BP/BP_Door",
        "graph": "EG_PhysicsDoor"
      },
      "write": {
        "strategy": "owned_graph_edit",
        "ops": [
          {
            "op": "ensure_entry",
            "entry": {
              "kind": "custom_event",
              "name": "OnSmokeTest",
              "statements": []
            }
          }
        ]
      }
    }
  ]
}
```

`append_blueprint_graph` 鏄?UE Task Runtime lowering 鍒扮幇鏈?Bridge capability cluster 鏃剁殑 adapter operation锛屼笉鏄?Agent 鎴?Task Compiler 鍐欏叆 TaskPlan step 鐨勫瓧娈点€?

## Agent-facing 璋冩暣

Agent 涓嶅簲鍐嶈緭鍑猴細

```text
read_logic_json 鈫?append_blueprint_graph 鈫?merge_blueprint_graph 鈫?compile 鈫?save
```

Agent 搴旇緭鍑猴細

```text
TaskSpec 鈫?preview_task 鈫?execute_task
```

Graph Write 鏂囨。浠嶅繀椤讳繚鐣欙紝鍥犱负 Task Compiler / Task Runtime 蹇呴』閬靛畧锛?

```text
1. Append 鍙兘杩藉姞鐙珛閫昏緫鍧椼€?
2. Replace 鍙兘鏇挎崲鏄庣‘鐩爣鐨勫畬鏁村疄鐜般€?
3. Patch 蹇呴』绮剧‘瀹氫綅 node / pin / link / default value銆?
4. Merge 鎵嶈兘鎺ュ叆宸叉湁鎵ц娴併€?
5. 鎺ュ叆宸叉湁鎵ц娴佸繀椤?dry_run銆?
6. rollback blocked / failed 鍚庝笉寰楃户缁?compile/save/patch銆?
```

## 鏂板 task_run_id 鍙ｅ緞

鏈枃浠跺師鏈?transaction_id 鍙ｅ緞淇濇寔涓嶅彉锛?

```text
涓€涓?transaction_id 瀵瑰簲涓€娆＄湡瀹炲啓宸ュ叿璋冪敤銆?
```

鏂板锛?

```text
涓€涓?task_run_id 瀵瑰簲涓€娆?TaskSpec / TaskPlan 鎵ц銆?
涓€涓?task_run_id 涓嬪彲浠ュ寘鍚涓?child transaction_id銆?
```

Graph Write step 浠嶇敓鎴愯嚜宸辩殑 transaction_id锛涙暣涓换鍔＄敱 TaskRunJournal 鍏宠仈銆?

## 鎴愬姛杩斿洖灞傜骇璋冩暣

搴曞眰 Graph Write 鎴愬姛杩斿洖浠嶄繚鎸佹瀬绠€瀛楁锛屼緥濡?write_ref / validation銆?

浣?Agent-facing execute_task 榛樿鍙繑鍥烇細

```text
task_run_id
feature_name
target_assets
applied_steps
created_assets / modified_assets 鎽樿
validation.compiled / should_save / saved
```

涓嶉粯璁よ繑鍥?child transaction_ids锛岄櫎闈炲け璐ャ€乨ebug銆乺ollback 鎴栫敤鎴锋槑纭姹傘€?


