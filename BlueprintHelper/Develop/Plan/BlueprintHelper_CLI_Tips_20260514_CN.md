# BlueprintHelper CLI Tips

## 0. 统一错误返回策略：stdout 可裁切，artifact 必须可诊断

原则：

1. CLI 的 `--select status,summary,artifacts.full_result` 可以只返回短摘要，避免 stdout 过大。
2. 任何 `cli_error`、`preview_blocked`、`execute_failed`、`compile_failed` 都必须在 `artifacts.full_result` 指向的 JSON 中保留可诊断细节。
3. 如果错误来自 UE 编译失败，artifact 里必须包含编辑器 Compiler Results 等价信息：`data.compile_result.compiler_results[]` 和可读 markdown；同时 `error.actual` 应携带可直接给 Agent 阅读的编译结果摘要。
4. 如果 artifact 只有笼统错误，如仅有 `Blueprint compile failed with N error(s)`，应视为 CLI/Bridge 错误返回能力缺口，而不是要求 Agent 去猜测或手动查 UE 面板。
5. Tips 文档中的 PowerShell、JSON、ExecutionPolicy、BOM 等条目只记录“非插件代码导致的调用问题”；插件内部失败必须优先把细节回传到 artifact。

稳定排查命令：

```powershell
bh.cmd <tool_name> --file params.json --select status,summary,artifacts.full_result
```

若 stdout 仍不足，打开 `artifacts.full_result`；不要把“stdout 被裁切”与“artifact 缺失诊断”混为同一类问题。
鏃ユ湡锛?026-05-14

鐢ㄩ€旓細璁板綍 BlueprintHelper CLI 璋冪敤杩囩▼涓湡瀹為亣鍒扮殑闂銆佸師鍥犲拰绋冲畾鍐欐硶锛屼緵鍚庣画娴嬭瘯涓?Agent 鎵ц鍙傝€冦€?
## 1. 鏃犲弬 CLI 鍛戒护涔熼渶瑕佹樉寮忓弬鏁拌緭鍏ユ簮

閿欒鍛戒护锛?
```powershell
bh.cmd blueprint_get_runtime_profile --format full
```

鐜拌薄锛?
```text
Choose exactly one params input source: --file, --json, or --stdin.
```

绋冲畾鍐欐硶锛?
```powershell
bh.cmd blueprint_get_runtime_profile --json "{}" --format full
```

鍘熷洜锛?
CLI 褰撳墠缁熶竴瑕佹眰姣忔璋冪敤鎭板ソ鎻愪緵涓€涓弬鏁拌緭鍏ユ簮锛屽嵆浣胯宸ュ叿娌℃湁涓氬姟鍙傛暟锛屼篃闇€瑕佷娇鐢?`--json "{}"`銆乣--file empty.json` 鎴?`--stdin`銆?
## 2. PowerShell here-string 鏍囬琛屽悗涓嶈兘鐩存帴璺?JSON 鍐呭

閿欒鍐欐硶锛?
```powershell
@'{"target":{"asset_path":"/Game/BP"}}'@
```

鐜拌薄锛?
```text
here-string 鏍囬鍚庨潰鍜岃灏句箣鍓嶄笉鍏佽鍖呭惈浠讳綍瀛楃銆?```

绋冲畾鍐欐硶锛?
```powershell
@'
{"target":{"asset_path":"/Game/BP"}}
'@ | Set-Content -LiteralPath params.json -Encoding utf8
```

鏇存帹鑽愮殑 no BOM 鍐欐硶锛?
```powershell
$enc = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($p, '{"target":{"asset_path":"/Game/BP"}}', $enc)
```

## 3. 鏃х増 Windows PowerShell 涓嶆敮鎸?`ConvertFrom-Json -AsHashtable`

閿欒鍐欐硶锛?
```powershell
$profile = $json | ConvertFrom-Json -AsHashtable
```

鐜拌薄锛?
```text
鎵句笉鍒颁笌鍙傛暟鍚嶇О鈥淎sHashtable鈥濆尮閰嶇殑鍙傛暟銆?```

绋冲畾绛栫暐锛?
1. 闇€瑕佷繚鐣欐湭鐭ュ瓧娈靛苟鍐?JSON 鏃讹紝浼樺厛鐢?Node.js 璇诲啓銆?2. 濡傛灉蹇呴』鐢?Windows PowerShell 5.1锛岄伩鍏嶄緷璧?`-AsHashtable`锛屾敼鐢?`PSCustomObject` 鎴栨墜鍐欏璞°€?
鎺ㄨ崘 Node.js 鍐欐硶锛?
```powershell
@'
const fs = require('fs');
const path = 'D:/UEProjects/Template/.blueprinthelper/agent-profile.json';
const profile = JSON.parse(fs.readFileSync(path, 'utf8'));
profile.active_profile = profile.active_profile || {};
profile.active_profile.safety_profile = 'AutoRepair';
fs.writeFileSync(path, JSON.stringify(profile, null, 2) + '\n', 'utf8');
'@ | node -
```

## 4. 涓嶈鍦?PowerShell 涓娇鐢?`$profile` 浣滀负鏅€氬彉閲忓悕

闂锛?
PowerShell 鍙橀噺鍚嶅ぇ灏忓啓涓嶆晱鎰燂紝`$profile` 浼氱鍒板唴缃?`$PROFILE` 鑷姩鍙橀噺璇箟锛屽鏄撴妸鐢ㄦ埛 PowerShell profile 璺緞瀵硅薄璇啓杩?JSON銆?
鏈疆鐜拌薄锛?
`agent-profile.json` 鏇捐姹℃煋涓?PowerShell profile 璺緞瀵硅薄锛屽悗缁凡鐢?Node.js 鎭㈠銆?
绋冲畾鍐欐硶锛?
```powershell
$agentProfile = @{}
$agentProfile['schema'] = 'BlueprintHelper.AgentProfile.v1'
```

## 5. `--select` 浼氳鍒囩粨鏋滐紝鎺掓煡閿欒鏃跺簲鏀圭敤 `--format full`

闂锛?
`--select status,summary,artifacts.full_result` 閫傚悎姝ｅ父鏌ョ湅闃舵缁撴灉锛屼絾鍙兘闅愯棌 CLI 瑙ｆ瀽閿欒鍜?Bridge 閿欒缁嗚妭銆?
鎺掓煡鍐欐硶锛?
```powershell
bh.cmd <tool_name> --json "{}" --format full
```

## 6. CLI 鍙傛暟鏂囦欢寤鸿浣跨敤 UTF-8 no BOM

鎺ㄨ崘鍐欐硶锛?
```powershell
$enc = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($p, $jsonText, $enc)
bh.cmd <tool_name> --file $p --format full
```

鍘熷洜锛?
澶嶆潅 JSON銆佷腑鏂囧瓧娈点€佷互鍙婂悗缁法 Node/PowerShell/UE Bridge 瑙ｆ瀽鏃讹紝UTF-8 no BOM 鏇寸ǔ瀹氾紝鑳藉噺灏戠紪鐮佸拰闈炴硶 JSON 椋庨櫓銆?
## 7. 閬垮厤鐢?PowerShell here-string 鐩存帴绠￠亾澶嶆潅涓枃 Markdown 鍒?Node

闂锛?
PowerShell 鍛戒护瀛楃涓蹭腑鍚屾椂鍖呭惈涓枃銆丮arkdown 鍙嶅紩鍙峰拰 JavaScript template string 鏃讹紝鍙兘鍦ㄤ紶缁?Node 鍚庡嚭鐜颁贡鐮佹垨鑴氭湰瑙ｆ瀽閿欒銆?
鏈疆鐜拌薄锛?
```text
SyntaxError: Unexpected token '??'
```

绋冲畾绛栫暐锛?
1. 淇敼 Markdown 鏂囨。鏃朵紭鍏堜娇鐢?`apply_patch`銆?2. 濡傛灉蹇呴』鐢?Node 鑴氭湰鍐欐枃妗ｏ紝灏介噺浠庡閮ㄦ枃浠惰鍙栨鏂囷紝閬垮厤鍦ㄥ懡浠よ鍐呭祵澶嶆潅涓枃 Markdown銆?3. Markdown 姝ｆ枃閲屾湁鍙嶅紩鍙锋椂锛屼笉瑕佺洿鎺ュ杩?JavaScript template string锛涙敼鐢ㄦ暟缁勮鎷兼帴鎴?JSON 瀛楃涓叉枃浠躲€?
## 8. PowerShell 涓鏉?`--json` 瀹规槗琚敊璇浆涔夋垚鍛戒护鍚?
閿欒鍐欐硶锛?
```powershell
bh.cmd blueprint_get_asset_info --json "{\"asset_path\":\"/Game/BP\"}" --format full
```

鏈疆鐜拌薄锛?
```text
Unsupported BlueprintHelper CLI command: blueprint_get_asset_info --json ...
```

绋冲畾绛栫暐锛?
1. PowerShell 涓嬪鏉?JSON 浼樺厛鍐欏叆 UTF-8 no BOM 鏂囦欢锛屽啀鐢?`--file`銆?2. 鍙湪闈炲父绠€鍗曚笖宸查獙璇佺殑鍦烘櫙浣跨敤 `--json "{}"`銆?
鎺ㄨ崘鍐欐硶锛?
```powershell
$enc = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($p, '{"asset_path":"/Game/BP"}', $enc)
bh.cmd blueprint_get_asset_info --file $p --format full
```

## 9. 涓嶈鍋囪鎵€鏈?Bridge 鍛戒护閮芥湁鐩存帴 CLI 鏆撮湶

鏈疆鐜拌薄锛?
```text
Unsupported BlueprintHelper CLI command: blueprint_get_asset_info --file ...
```

鍘熷洜锛?
`get_asset_info` 鏄?Bridge 鍐呴儴鍛戒护锛屼絾褰撳墠 CLI 瀵瑰绋冲畾鍏ュ彛涓嶄竴瀹氭毚闇插搴旂殑 `blueprint_get_asset_info` 鐩存帴鍛戒护銆?
绋冲畾绛栫暐锛?
1. 鏅€?Agent 鑾峰彇璧勪骇涓婁笅鏂囧簲浣跨敤 `blueprinthelper_read_task_context`銆?2. 鍙湁 `tool-registry` 鏄庣‘鍒楀嚭鐨?CLI 鍛戒护鎵嶄綔涓?Agent 鍙敤 surface銆?3. 濡傛灉蹇呴』娴嬭瘯 Bridge 鍐呴儴鍛戒护锛屼娇鐢ㄤ笓闂?Debug/Bridge 鑴氭湰锛屼笉瑕佹妸瀹冨綋浣滄櫘閫?CLI surface銆?
## 10. Windows PowerShell 鐨?`Set-Content -Encoding utf8` 鍙兘鍐欏叆 BOM

閿欒鍐欐硶锛?```powershell
$taskSpec | ConvertTo-Json -Depth 40 | Set-Content -Path task.json -Encoding utf8
bh.cmd blueprinthelper_preview_task --file .\task.json --format full
```

鏈疆鐜拌薄锛?```text
Unexpected token '锘?, "锘縶 ... is not valid JSON
```

鍘熷洜锛?Windows PowerShell 5.1 鐨?`-Encoding utf8` 榛樿浼氬啓 UTF-8 BOM锛屽綋鍓?CLI JSON parser 涓嶆帴鍙楁枃浠跺紑澶寸殑 BOM銆?
绋冲畾鍐欐硶锛?```powershell
$json = $taskSpec | ConvertTo-Json -Depth 40
$enc = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText((Resolve-Path '.\task.json'), $json, $enc)
```

## 11. `read_context` 鐨?`target_type=data_table` 鍘嗗彶闂宸蹭慨澶?
鍘嗗彶鐜拌薄锛氭棫鐗?`ReadSpec.target.target_type` 涓嶆帴鍙?`data_table`锛屼笖 `read_type=data_table_context` / `object_property_context` 鏇捐繑鍥?`unsupported_read_type`銆?
褰撳墠鐘舵€侊細2026-05-15 宸蹭慨澶嶃€俙blueprinthelper_read_context` 鍙洿鎺ヨ鍙?DataTable銆丏ataTable row銆丱bjectProperty銆丏ataAsset銆乄idgetTree銆乄idgetProperty銆丆omponent銆乂ariable 鍜?Graph context銆?
绋冲畾鍐欐硶锛?```powershell
$path = 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexTaskSpecs\read_datatable.json'
$json = @{
  schema = 'BlueprintHelper.ReadSpec.v1'
  read_type = 'data_table_context'
  target = @{ asset_path = '/Game/Path/DT_Test'; target_type = 'data_table'; target_name = 'RowName' }
  format = 'summary'
} | ConvertTo-Json -Depth 16
[System.IO.File]::WriteAllText($path, $json, [System.Text.UTF8Encoding]::new($false))
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js blueprinthelper_read_context --file $path --format full
```

娉ㄦ剰锛歚schema` 蹇呴』涓?`BlueprintHelper.ReadSpec.v1`锛涘鏉?JSON 浠嶅缓璁蛋 UTF-8 no BOM `--file`銆?
## 12. 涓嶈鐢ㄧ洿鎺?Bridge 璇诲懡浠ゆ浛浠?`blueprinthelper_read_context`

鍘嗗彶鐜拌薄锛歚blueprint_get_datatable_rows`銆乣blueprint_get_object_properties` 涓嶆槸绋冲畾 Agent-facing CLI 鍛戒护锛岀洿鎺ヨ皟鐢ㄥ彲鑳借繑鍥?unsupported command銆?
褰撳墠绛栫暐锛欴ataTable/ObjectProperty/DataAsset 鐨勭ǔ瀹氳鍥炲叆鍙ｅ凡缁忔敹鏁涘埌 `blueprinthelper_read_context`銆傛櫘閫?Agent 娴嬭瘯涓嶅簲鍋囪鍐呴儴 Bridge 鍛戒护鏆撮湶銆?
绋冲畾鍐欐硶锛?1. DataTable锛歚read_type=data_table_context`锛宍target_type=data_table`銆?2. Object property锛歚read_type=object_property_context`锛宍target_type=property` 鎴?`object_property`銆?3. DataAsset锛歚read_type=data_asset_context`锛宍target_type=data_asset`銆?4. Widget锛歚read_type=widget_context`锛屾棤 `target_name` 璇诲彇鏍戯紝鏈?`target_name` 璇诲彇 widget property銆?
## 11. 寮€鍙戦獙璇佹椂涓嶈榛樿浣跨敤鍏ㄥ眬 `bh.cmd`

鏈疆鐜拌薄锛?宸ヤ綔鍖?TypeScript 缂栬瘧鍣ㄥ凡缁忚緭鍑?`logic_spec`锛屼絾鐩存帴璋冪敤鍏ㄥ眬 `bh.cmd` 浠嶅彲鑳藉鐜版棫琛屼负锛屽師鍥犳槸鍏ㄥ眬鍛戒护鍙兘鎸囧悜宸插畨瑁呮彃浠剁紦瀛樼増鏈紝鑰屼笉鏄綋鍓嶆簮鐮佸伐浣滃尯銆?
绋冲畾绛栫暐锛?1. 楠岃瘉鏈彂甯冪殑宸ヤ綔鍖烘簮鐮佹敼鍔ㄦ椂锛屽厛鏋勫缓宸ヤ綔鍖?CLI銆?2. 浣跨敤宸ヤ綔鍖?CLI 鍏ュ彛鎵ц娴嬭瘯锛岃€屼笉鏄叏灞€ `bh.cmd`銆?
鎺ㄨ崘鍐欐硶锛?```powershell
npm.cmd --prefix AgentFaceService\cli run build
node .\AgentFaceService\cli\build\cli\index.js blueprinthelper_preview_task --file .\task.json --format full
```
## 12. TaskSpec 缂栬瘧/淇濆瓨绛栫暐瀛楁鏄?`validation`锛屼笉鏄?`validation_policy`

閿欒鍐欐硶锛?```json
{
  "validation_policy": {
    "should_compile": true,
    "should_save": false
  }
}
```

鏈疆鐜拌薄锛?TaskSpec 鍐欏叆鎴愬姛锛屼絾鎵ц缁撴灉椤堕儴鏄剧ず `validation.should_compile=false`锛屾病鏈夋墽琛?post compile銆傚悗鏉ユ敼鎴?`validation.should_compile=true` 鍚庯紝缁撴灉涓嚭鐜?`post_operations.compile_blueprint_asset`銆?
绋冲畾鍐欐硶锛?```json
{
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

鍘熷洜锛?褰撳墠 AgentFace TaskSpec schema 浣跨敤 `validation` 瀛楁锛沗validation_policy` 浼氫綔涓烘湭鐭ュ瓧娈?passthrough锛屼絾涓嶄細鍙備笌 TaskPlan execution policy銆?## 13. PowerShell 璋冪敤 .NET WriteAllText 鏃朵笉瑕佹妸 -replace 琛ㄨ揪寮忕洿鎺ュ杩涘弬鏁板垪琛?
鐜拌薄锛歚[System.IO.File]::WriteAllText($path, $text -replace 'a','b', $encoding)` 浼氳 PowerShell 瑙ｆ瀽鎴?4 涓弬鏁帮紝鎶モ€滄壘涓嶅埌 WriteAllText 鐨勯噸杞斤紝鍙傛暟璁℃暟涓?4鈥濄€?
寤鸿锛氬厛鐢ㄤ腑闂村彉閲忎繚瀛樻浛鎹㈢粨鏋滐紝鍐嶈皟鐢ㄤ笁鍙傛暟閲嶈浇锛歚$new = $text -replace 'a','b'; [System.IO.File]::WriteAllText($path, $new, $encoding)`銆?
## 14. 闇€瑕佽法缂栬緫鍣ㄩ噸鍚獙璇佺殑璧勪骇蹇呴』璁剧疆 `validation.should_save=true`

鐜拌薄锛歍askSpec 鎵ц鎴愬姛鍚庡叧闂苟閲嶆柊鍚姩缂栬緫鍣紝鍐嶆墽琛屽悗缁?TaskSpec 鏃跺嚭鐜?`target_blueprint_not_found`銆?
鏈疆鍘熷洜锛氭祴璇曡祫浜т娇鐢ㄤ簡 `validation.should_save=false`锛岃祫浜у彧瀛樺湪浜庡綋鍓嶇紪杈戝櫒鍐呭瓨/鏈繚瀛樺寘鐘舵€佷腑锛涢噸鍚紪杈戝櫒鍚庡悗缁?TaskSpec 鎵句笉鍒扮洰鏍?Blueprint銆?
绋冲畾鍐欐硶锛?
```json
{
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

寤鸿锛?
1. 鍗曡疆涓存椂鍐欏叆銆佸悓涓€缂栬緫鍣ㄤ細璇濆唴椹笂楠岃瘉锛屽彲浠ヤ娇鐢?`should_save=false`銆?2. 浠讳綍闇€瑕佸叧闂?閲嶅惎缂栬緫鍣ㄣ€佸娴嬫寔涔呭寲銆佹垨浜ょ粰涓嬩竴杞?TaskSpec 缁х画鍐欏叆鐨勮祫浜э紝閮藉簲浣跨敤 `should_save=true`銆?
## 15. Windows PowerShell 5.1 涓嶆敮鎸?`Set-Content -Encoding utf8NoBOM`

鐜拌薄锛?```text
Set-Content : 鏃犳硶缁戝畾鍙傛暟鈥淓ncoding鈥濄€傛棤娉曞皢鍊尖€渦tf8NoBOM鈥濊浆鎹负绫诲瀷鈥淢icrosoft.PowerShell.Commands.FileSystemCmdletProviderEncoding鈥濄€?```

鍘熷洜锛?Windows PowerShell 5.1 鐨?`Set-Content -Encoding` 鏋氫妇娌℃湁 `utf8NoBOM`锛岃鍐欐硶鍙€傜敤浜庤緝鏂扮殑 PowerShell 鐗堟湰銆?
绋冲畾鍐欐硶锛?```powershell
$json = $object | ConvertTo-Json -Depth 40
$encoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText((Join-Path (Get-Location) 'task.json'), $json, $encoding)
```

寤鸿锛?1. BlueprintHelper CLI 鐨勫鏉?JSON 浠嶄紭鍏堣蛋 `--file`銆?2. 鍦?Windows PowerShell 5.1 涓嬪啓 `--file` JSON 鏃朵娇鐢?.NET `UTF8Encoding($false)`锛屼笉瑕佷娇鐢?`utf8NoBOM` 鏋氫妇鍚嶃€?
## 16. `blueprinthelper_export_debug_bundle` 鍙帴鍙?`debug_case_id`

閿欒鍐欐硶锛?```powershell
bh blueprinthelper_export_debug_bundle --json '{"asset_path":"/Game/Asset","reason":"manual"}'
```

鐜拌薄锛?```json
{"status":"cli_error"}
```

鍘熷洜锛?AgentFace schema 浣跨敤 `DebugCaseInputSchema`锛岃鍛戒护鍙帴鍙?`debug_case_id: string`銆俙asset_path` 鍜?`reason` 涓嶆槸鍚堟硶瀛楁銆?
姝ｇ‘鍐欐硶锛?```powershell
bh blueprinthelper_export_debug_bundle --json '{"debug_case_id":"<debug_case_id>"}'
# 鎴栧啓鍏?UTF-8 no BOM 鏂囦欢鍚庯細
bh blueprinthelper_export_debug_bundle --file .\debug_case.json
```

寤鸿锛?1. 鍏堢敤 `blueprinthelper_list_debug_cases` 鎴?`blueprinthelper_get_debug_case` 鑾峰彇鏈夋晥 `debug_case_id`銆?2. 濡傛灉鍙湁 asset path锛屽厛瑙﹀彂/鏌ヨ鑳戒骇鐢?DebugCase 鐨勬祦绋嬶紝涓嶈鐩存帴璋冪敤 export銆?
## 17. Windows PowerShell 涓嬪鏉?`--json` 浠嶅彲鑳芥薄鏌?CLI command name

鐜拌薄锛?```text
Unsupported BlueprintHelper CLI command: blueprinthelper_list_debug_cases --json {\ limit\:5} --format full
```

鍘熷洜锛?Windows PowerShell 鐨勫紩鍙峰拰鍙嶆枩鏉犺鍒欏彲鑳借澶嶆潅 JSON 娌℃湁浣滀负鍗曠嫭鍙傛暟浼犲叆 CLI锛屽鑷?CLI 鎶婂悗缁弬鏁版嫾杩?command name銆?
绋冲畾鍐欐硶锛?```powershell
$json = @'
{
  "limit": 5
}
'@
[System.IO.File]::WriteAllText((Join-Path (Get-Location) 'list_debug_cases_limit5.json'), $json, [System.Text.UTF8Encoding]::new($false))
bh blueprinthelper_list_debug_cases --file .\list_debug_cases_limit5.json --format full
```

寤鸿锛?澶嶆潅 JSON銆佸甫寮曞彿 JSON銆佹垨闇€瑕佽法 shell 绋冲畾澶嶇幇鐨勫懡浠ょ粺涓€浣跨敤 UTF-8 no BOM `--file`銆?
## 15. UE 婧愮爜澶嶅埗蹇収鍓嶅厛纭 Public / Private 瀹為檯璺緞

鐜拌薄锛欰5 鍘熺敓闈㈡澘婧愮爜蹇収鏃讹紝鏈€鍒濆亣璁?`SReadOnlyHierarchyView.h` 浣嶄簬 `UMGEditor/Private/Hierarchy`锛孭owerShell `Copy-Item` 鍓嶇疆鏍￠獙杩斿洖 missing source file銆?
鍘熷洜锛歎E 5.6 涓?`SReadOnlyHierarchyView.cpp` 浣嶄簬 `UMGEditor/Private/Hierarchy`锛屼絾 `SReadOnlyHierarchyView.h` 浣嶄簬 `UMGEditor/Public/Hierarchy`銆?
绋冲畾鍋氭硶锛氬鍒?UE 鍘熺敓婧愮爜鍓嶅厛鐢?`rg -n "ClassOrFileName" E:\UE_5.6\Engine\Source\Editor -g "*.h" -g "*.cpp"` 纭鐪熷疄璺緞锛屽啀鎵ц `Copy-Item -LiteralPath <source> -Destination <snapshot>`銆備笉瑕佺洿鎺ユ妸鏈€傞厤 `.cpp` 澶嶅埗鍒版彃浠?`Source` 缂栬瘧鐩綍銆?## 2026-05-15 PowerShell rg 姝ｅ垯寮曞彿瑙勯伩

鐜拌薄锛氬湪 PowerShell 涓洿鎺ュ啓鍖呭惈 `"`銆乣.`銆乣|`銆佹嫭鍙风殑澶嶆潅 `rg` 姝ｅ垯鏃讹紝PowerShell 鍙兘鍏堟寜鑷韩杞箟/鎴愬憳璁块棶瑙勫垯瑙ｆ瀽锛屾姤 `寮曠敤杩愮畻绗﹀悗闈㈢己灏戝睘鎬у悕绉癭锛屽懡浠よ繕娌¤繘鍏?`rg`銆?
绋冲畾鍋氭硶锛歅owerShell 閲屼紭鍏堢敤鍗曞紩鍙峰寘浣忓鏉?`rg` pattern锛屼緥濡傦細

```powershell
rg -n 'Make.*StringArray|FJsonValueString|SetArrayField' 'D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\BlueprintHelperTaskRuntimeService.cpp'
```

鍒嗙被锛氭湰鍦?shell 璋冪敤閿欒锛屼笉鏄?BlueprintHelper 鎻掍欢 Bug銆?## 2026-05-15 PowerShell ExecutionPolicy 鎷︽埅 bh.ps1

鐜拌薄锛氬湪 PowerShell 涓墽琛?`bh ...` 鏃讹紝鍛戒护瑙ｆ瀽鍒?npm shim `bh.ps1`锛屽鏋滃綋鍓嶇郴缁熸墽琛岀瓥鐣ョ姝㈣剼鏈紝浼氭姤 `鏃犳硶鍔犺浇鏂囦欢 ...\bh.ps1锛屽洜涓哄湪姝ょ郴缁熶笂绂佹杩愯鑴氭湰`銆?
绋冲畾鍋氭硶锛氭湰鍦颁笉璋冩暣 ExecutionPolicy 鏃讹紝鐩存帴璋冪敤 `bh.cmd ...`锛屼緥濡傦細

```powershell
bh.cmd blueprint_get_runtime_profile --json "{}" --select status,summary
```

鍒嗙被锛歅owerShell/npm shim 璋冪敤闄愬埗锛屼笉鏄?BlueprintHelper 鎻掍欢 Bug銆?## 2026-05-15 PowerShell 鏂囨。鑴氭湰鍐呭祵 --json 澶ф嫭鍙疯閬?
鐜拌薄锛氬湪 PowerShell 鑴氭湰鐨勬櫘閫氬弻寮曞彿瀛楃涓蹭腑鐩存帴鍐?``--json "{}"``锛宍{}` 鍜岃浆涔夊紩鍙峰彲鑳借 PowerShell 瑙ｆ瀽涓鸿〃杈惧紡鐗囨锛屾姤鈥滆〃杈惧紡鎴栬鍙ヤ腑鍖呭惈鎰忓鐨勬爣璁?`{`鈥濄€?
绋冲畾鍋氭硶锛氭枃妗ｆ浛鎹?杩藉姞鑴氭湰涓寘鍚?JSON 绀轰緥鏃讹紝浣跨敤 here-string 淇濆瓨鏁存鏂囨湰锛屾垨鎶婂懡浠ょず渚嬫媶鎴愬崟寮曞彿瀛楃涓诧紝涓嶈鍦ㄦ櫘閫氬弻寮曞彿瀛楃涓查噷宓屽 `"{}"`銆?
鍒嗙被锛歅owerShell 鑴氭湰鏂囨湰杞箟閿欒锛屼笉鏄?BlueprintHelper 鎻掍欢 Bug銆?## 2026-05-15 `task preview/execute` 闇€瑕佽８ TaskSpec

鐜拌薄锛氭妸鏃ф枃妗ｉ噷鐨?`{ "task_spec": { ... } }` 鐩存帴浼犵粰 `bh.cmd task preview --file ...`锛屼細瑙﹀彂 Zod union 鏍￠獙閿欒锛屾彁绀烘牴璺緞缂哄皯 `schema/task_type/target/behavior`銆?
鍘熷洜锛氬綋鍓?CLI 鐨?`task preview` / `task execute` 鍏ュ弬鏄８ `BlueprintHelper.TaskSpec.v1`锛屼笉鏄甫 `task_spec` 鍖呰鐨勫璞°€傛棫 runbook 涓殑鍖呰绀轰緥涓嶈兘鐩存帴浣滀负 CLI 鏂囦欢浼犲叆銆?
绋冲畾鍋氭硶锛氭枃浠舵牴瀵硅薄鐩存帴鍐欙細

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "create_asset",
  "target": { "asset_path": "/Game/...", "target_type": "asset" },
  "behavior": {}
}
```

## 2026-05-15 PowerShell 鍐?TaskSpec 鏂囦欢闇€瑕?UTF-8 no BOM

鐜拌薄锛氫娇鐢?`Set-Content -Encoding utf8` 鍐欏嚭鐨?TaskSpec 鍦ㄩ儴鍒?PowerShell 鐜涓嬩細甯?UTF-8 BOM锛孋LI 璇诲彇 `--file` 鏃跺彲鑳芥姤 `Unexpected token '锘? ... is not valid JSON`銆?
绋冲畾鍐欐硶锛氫娇鐢?.NET no BOM 缂栫爜鍐欏叆鏂囦欢銆?
```powershell
$json = $object | ConvertTo-Json -Depth 32
[System.IO.File]::WriteAllText($path, $json, [System.Text.UTF8Encoding]::new($false))
```

## 2026-05-15 鏈湴 build CLI 鐨?TaskSpec 鍛戒护鍏ュ彛

鐜拌薄锛氱洿鎺ヨ繍琛?`node ...\AgentFaceService\cli\build\cli\index.js blueprinthelper_execute_task_spec --file ...` 浼氳繑鍥?`Unsupported BlueprintHelper CLI command`銆?
鍘熷洜锛氭湰鍦?build CLI 鐨?TaskSpec 鍏ュ彛鏄垎缁勫懡浠わ紝涓嶆槸鏃у伐鍏峰悕鍛戒护銆?
绋冲畾鍋氭硶锛?
```powershell
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js task execute --file D:\Path\task.json --format full --fields status,summary,artifacts.full_result
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js task preview --file D:\Path\task.json --format full
```

璇存槑锛欱ridge 宸ュ叿绫诲懡浠や粛鍙敤 `<tool_name> --file params.json`锛屼緥濡?`blueprinthelper_query_review_records`銆?
## 2026-05-15 PowerShell 鍦烘櫙閬垮厤澶嶆潅 `--json`

鐜拌薄锛歅owerShell 涓紶鍏?`--json '{"..."}'` 瀹规槗鍥犱负寮曞彿/杞箟瀵艰嚧 CLI 瑙ｆ瀽閿欒鎴?`cli_error`銆?
绋冲畾鍋氭硶锛氫笉鎵╁ぇ `--json` 杞箟瀹瑰繊锛屽鏉傚弬鏁扮粺涓€鍐欏叆 UTF-8 no BOM JSON 鏂囦欢锛屽啀浣跨敤 `--file`銆?
```powershell
$path = 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexTaskSpecs\params.json'
$json = @{ task_run_id='task_xxx'; pending_only=$true } | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText($path, $json, [System.Text.UTF8Encoding]::new($false))
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js blueprinthelper_query_review_records --file $path --format full
```
## 2026-05-15 Graph TaskSpec 璇彞浣跨敤鐭悕

鐜拌薄锛歚append_new_owned_graph` 涓户缁娇鐢ㄦ棫 `kind="call_function"` 浼氬湪 preview 闃舵杩斿洖 `statement_kind_unsupported`銆?
绋冲畾鍋氭硶锛欸raphStatementFramework 涓婚摼璺娇鐢ㄧ煭鍚嶏紝渚嬪 `kind="call"` + `target="PrintString"`銆?
```json
{
  "kind": "call",
  "target": "PrintString",
  "args": {
    "InString": { "kind": "literal", "value_type": "string", "value": "hello" }
  }
}
```
## 2026-05-15 `blueprinthelper_read_context` 鏂囦欢蹇呴』鏄?ReadSpec 鏍瑰璞?
鐜拌薄锛氬彧鍐?`read_type/target/format`锛岀己灏?`schema` 鏃讹紝CLI 杩斿洖 `Invalid literal value, expected "BlueprintHelper.ReadSpec.v1"`銆?
绋冲畾鍐欐硶锛氭枃浠舵牴瀵硅薄蹇呴』鍖呭惈锛?```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "object_property_context",
  "target": { "asset_path": "/Game/Asset", "target_type": "property" },
  "format": "summary"
}
```

鍒嗙被锛欳LI 鍙傛暟鏂囦欢鏍煎紡閿欒锛屼笉鏄彃浠?Bug銆?
## 2026-05-15 PowerShell 澶栭儴鍛戒护鍙傛暟涓笉瑕佺洿鎺ュ祵鍏?`(Join-Path ...)`

鐜拌薄锛氬湪澶栭儴 CLI 鍛戒护鍙傛暟浣嶇疆鐩存帴鍐?`--file (Join-Path $base 'x.json')` 鍙兘涓嶄細鎸夐鏈熸眰鍊间负鍗曚釜璺緞鍙傛暟銆?
绋冲畾鍐欐硶锛氬厛璧嬪€煎啀浼犲弬銆?```powershell
$file = Join-Path $base 'read_object_bp.json'
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js blueprinthelper_read_context --file $file --format full
```

鍒嗙被锛歅owerShell 澶栭儴鍛戒护鍙傛暟姹傚€奸棶棰橈紝涓嶆槸鎻掍欢 Bug銆?
## 2026-05-15 `review_baseline_dirty_asset_policy` 鍚堟硶鍊?
鐜拌薄锛歍askSpec 涓啓 `review_baseline_dirty_asset_policy=allow_auto_save` 浼氳Е鍙?Zod enum 閿欒銆?
褰撳墠鍚堟硶鍊硷細
1. `block`
2. `save_before_archive`
3. `allow_stale_disk_snapshot`

绋冲畾鍐欐硶锛氶渶瑕佽嚜鍔ㄤ繚瀛樺綋鍓嶈剰璧勪骇鍐嶅綊妗?baseline 鏃朵娇鐢?`save_before_archive`銆?
鍒嗙被锛歍askSpec 鍙傛暟鍊奸敊璇紝涓嶆槸鎻掍欢 Bug銆?
## 2026-05-15 PowerShell 闀垮懡浠ゅ啓鍏ラ檺鍒?- 鐜拌薄锛氫竴娆℃€х敤 PowerShell here-string 鍐欏叆澶ч噺鏂囦欢鏃讹紝Windows 杩涚▼鍒涘缓鍙兘澶辫触骞惰繑鍥?CreateProcessAsUserW failed: 206銆?- 鍘熷洜锛氬懡浠よ杩囬暱锛屼笉鏄?BlueprintHelper 鎻掍欢鎴?CLI 鍗忚閿欒銆?- 绋冲畾澶勭悊锛氭敼鐢?pply_patch銆佸垎鎵圭煭鍛戒护锛屾垨鍏堝啓涓存椂鑴氭湰鏂囦欢鍐嶆墽琛岋紱涓嶈鎶婂ぇ閲忔枃浠跺唴瀹瑰杩涘悓涓€涓?powershell -Command銆?## 2026-05-15 ReviewPanel UI 楠岃瘉 CLI Tips

- 鍦?PowerShell 涓洿鎺ヨ皟鐢?h 鍙兘瑙ｆ瀽鍒?h.ps1锛屽苟琚?ExecutionPolicy 闃绘鎵ц銆傜ǔ瀹氬仛娉曪細鍦?Codex/PowerShell 鑷姩鍖栬剼鏈腑鏄惧紡璋冪敤 h.cmd銆?- 鏈澶嶇幇鍛戒护锛?h.cmd blueprinthelper_preview_task --file <TaskSpec.json> --select status,summary,artifacts.full_result銆?- 鑻ュ垱寤虹被 TaskSpec 杩斿洖 sset_already_exists锛屼笉瑕佽鍒や负鎻掍欢宕╂簝锛涜繖鏄?preview 闃绘柇锛屽悗缁彲鏀圭敤鍞竴璧勪骇鍚嶆垨鎵ц edit/update 绫?TaskSpec銆?- DataTable 琛?add 杩斿洖鈥?<RowName>' 宸插瓨鍦ㄢ€濇椂锛屾敼鐢ㄥ敮涓€琛屽悕锛屾垨鏄庣‘浣跨敤 update 琛岀瓥鐣ャ€