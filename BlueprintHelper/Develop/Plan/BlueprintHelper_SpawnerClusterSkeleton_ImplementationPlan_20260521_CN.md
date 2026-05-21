# BlueprintHelper SpawnerCluster Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to execute follow-up implementation tasks.

**Current canonical pipeline:**
`AgentFace statement -> Semantic Resolver -> FBlueprintHelperActionResolutionRequest { ClusterKind, SemanticConstraints, GraphContext, TypedPins } -> BlueprintActionResolutionCore -> SpawnerClusterResolver.SelectCluster(ClusterKind) -> Cluster.Resolve(Request) -> UBlueprintNodeSpawner candidate -> NodeFragment adapter`.


## 0.1 2026-05-21 Runtime Smoke 同步

- [x] Bridge Server 恢复长连接：`task execute` 内部 preview + execute 复用同一 BridgeClient 时不再触发 `read ECONNRESET`。
- [x] Signature dependency dry-run entry fact 已接入：GraphWrite dry-run 只在存在 `signature_dependency` entry fact 时创建临时 CustomEvent entry 用于验证，真实写入仍由 Signature step 负责。
- [x] Signature step 在 `append_new_owned_graph` 目标图不存在时，会在真实执行中创建 Ubergraph page 和 CustomEvent entry；GraphWrite 只追加 body。
- [x] FieldVariableAction `get/set` 已从 provider 候选解析推进到 `UBlueprintVariableNodeSpawner` NodeFragment emission。
- [x] 修复 FieldVariable 成员变量 spawner scope：成员变量不再把目标图传入 `VarContext`，避免 UE 将成员变量 Set/Get 误判为 local variable。
- [x] CLI runtime smoke 通过：创建测试 Blueprint、创建 `SmokeFloat` 成员变量、`get/set` graph execute、logic_json 读回均成功。

距离期望差距：`get_property/set_property`、`EventDelegateAction`、`GenericAssetStructControlAction` 仍未迁移到完整 provider + NodeFragment adapter；本轮完成 Bridge 长连接、Signature entry 生命周期和 FieldVariable `get/set` 闭环。

## 0. 鎵ц鐘舵€侊紙2026-05-21锛?

- [x] `ActionResolutionCore` 宸蹭互 `ClusterKind` 浣滀负鍞竴涓€绾у垎鍙戦敭銆?
- [x] `SpawnerClusterResolver` 宸插彧鎸?`Request.ClusterKind` 鍒嗗彂锛屼笉鍐嶆寜璇箟 kind 閫夋嫨绨囥€?
- [x] `call/get/set/get_property/set_property/op/construct/deconstruct/select/control` 宸叉敹鏁涗负 `FBlueprintHelperActionSemanticConstraints.Kind`銆?
- [x] `FunctionActionCluster` 宸叉帴鍏ョ幇鏈?call resolver銆?
- [x] `FieldVariableActionCluster` 宸叉帴鍏?get/set provider 瑙ｆ瀽閾捐矾銆?
- [x] 鏃у彉閲?direct spawn 鍏ュ彛宸蹭粠 GraphWrite facade / node spawner 鍏紑璺緞绉婚櫎銆?
- [x] FieldVariableAction get/set NodeFragment adapter 已完成，并通过真实 Blueprint preview/execute/read-back smoke。
- [ ] get_property/set_property銆丟enericAssetStructControl銆丒ventDelegate 绛夌皣浠嶉渶瑕佸悗缁?provider/adapter 杩佺Щ銆?

璺濈鏈熸湜宸窛锛欶ieldVariableAction get/set 宸蹭粠鍊欓€夎В鏋愭帹杩涘埌 NodeFragment adapter 涓?CLI preview/execute 閫氳繃锛涘墿浣欏樊璺濅负 get_property/set_property銆丟enericAssetStructControl銆丒ventDelegate銆侀€氱敤 candidate action cache 涓庢洿澶ц寖鍥磋鐩栨祴璇曘€?

## 1. 涓€绾у垎鍙戠‖瑙勫垯

1. `FBlueprintHelperActionResolutionRequest.ClusterKind` 鏄敮涓€涓€绾у垎鍙戦敭銆?
2. AgentFace 璇箟瀛楁鍙兘鍐欏叆 `FBlueprintHelperActionSemanticConstraints`銆?
3. `SpawnerClusterResolver` 涓嶅厑璁告牴鎹?semantic kind銆乹uery銆乼arget銆乼ype 鍋氫竴绾х皣閫夋嫨銆?
4. 鏂板鑳藉姏蹇呴』鎵╁睍瀵瑰簲 SpawnerCluster / Resolver / Adapter 杈圭晫锛屼笉鍏佽鎭㈠鏃?node handler銆佹棫 direct spawn銆佹棫 fallback銆?

## 2. 褰撳墠鍥涚被绨?

| ClusterKind | 褰撳墠鐘舵€?| 宸窛 |
|---|---|---|
| `FunctionAction` | `call` 宸叉帴鍏?call resolver | 鍚庣画鎶?call 涓撻」缂撳瓨娉涘寲鍒?ActionResolution 缂撳瓨 |
- [x] FieldVariableAction get/set NodeFragment adapter 已完成，并通过真实 Blueprint preview/execute/read-back smoke。
| `EventDelegateAction` | skeleton 宸插瓨鍦?| 缂虹湡瀹?provider / adapter |
| `GenericAssetStructControlAction` | skeleton 宸插瓨鍦?| 缂?construct/deconstruct/select/control 绛?provider / adapter |

## 3. 鍚庣画鎵ц椤哄簭

- [x] FieldVariableAction get/set NodeFragment adapter 已完成，并通过真实 Blueprint preview/execute/read-back smoke。
2. 杩佺Щ get_property/set_property provider銆?
3. 杩佺Щ GenericAssetStructControl provider銆?
4. 鎺ュ叆缁熶竴 candidate action cache銆?
5. 璺?CLI preview/execute 瑕嗙洊娴嬭瘯骞跺悓姝ョ幇瀹炶兘鍔涖€?
