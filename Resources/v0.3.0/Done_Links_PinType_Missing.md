# Links_PinType_Missing

## 结论

当前 Links 若只包含 PinName，不包含 PinType，会影响 CLI Agent 对蓝图进行可靠编辑。它不一定导致所有读取场景失败，但会明显降低自动重连、插入节点、修复连线和生成导入 JSON 的确定性。

## 影响范围

1. Agent 只能看到 PinName 时，无法判断该连线是执行流、对象引用、布尔、枚举、结构体、数组、Map、Set、委托还是软引用。
2. 同一节点可能存在同名或显示名相同的 Pin，例如 ReturnValue、Then、Completed、Value、Index、Item、Target。
3. 重建节点后 Pin 顺序、显示名或动态 Pin 可能变化，仅靠 PinName 不能稳定定位端点。
4. Agent 无法在编辑前做类型兼容性校验，也无法判断是否需要插入 Cast、Conv、Break Struct、Make Struct、Array Get 等中间节点。
5. 对函数重载、宏实例、Knot、Select、Promotable Operator、容器节点和委托节点，PinName 缺失类型上下文会显著增加误连概率。

## 最小修复要求

Links 的每个端点至少应包含：

```json
{
  "NodeId": "...",
  "PinId": "...",
  "PinName": "...",
  "Direction": "Input|Output",
  "PinType": {
    "PinCategory": "exec|bool|int|real|object|struct|enum|delegate|interface|name|string|text|wildcard|...",
    "PinSubCategory": "...",
    "PinSubCategoryObject": "/Script/... 或资产路径",
    "ContainerType": "None|Array|Set|Map",
    "ValueTerminalType": "Map value 类型，可选",
    "bIsReference": false,
    "bIsConst": false,
    "bIsWeakPointer": false,
    "bIsUObjectWrapper": false
  }
}
```

如果需要保持 Links 紧凑，可以采用引用式结构：

```json
{
  "Pins": {
    "NodeGuid:PinGuid": {
      "NodeId": "...",
      "PinId": "...",
      "PinName": "...",
      "Direction": "Output",
      "PinType": { "PinCategory": "exec" }
    }
  },
  "Links": [
    {
      "FromPinRef": "NodeGuid:PinGuid",
      "ToPinRef": "NodeGuid:PinGuid"
    }
  ]
}
```

## 推荐修复策略

优先修复导出层，而不是让 CLI Agent 在消费端反查。

1. 在蓝图 JSON 导出时，为 Link endpoint 附带完整 PinType 快照。
2. Link endpoint 必须包含稳定 PinId 或 PinGuid；PinName 只能作为可读标签，不应作为唯一定位键。
3. 导入或编辑前校验两端 Direction 与 PinType：Output -> Input，exec 只能连 exec，数据 Pin 必须类型兼容。
4. 对通配符和 Promotable 节点保留 `wildcard` 原始类型，并在连线后重新读取节点类型。
5. 对 compact logic JSON 或 Markdown 摘要，可以显示精简类型，但机器可消费 JSON 必须保留完整类型。

## 验收标准

1. 导出的 Links 端点同时包含 PinName、PinId、Direction、PinType。
2. CLI Agent 可在不打开 UE 编辑器 UI 焦点的情况下判断连线类型兼容性。
3. 对两个同名 Pin，Agent 能通过 PinId 和 PinType 区分目标。
4. 对 exec 与数据 Pin 混连，导入/编辑请求会被校验层拒绝并返回明确错误。
5. 对已有蓝图导出后再导入，不应因为 PinName 相同而连接到错误端点。

## 风险等级

中高。读取展示可暂时容忍，但只要 CLI Agent 使用 Links 执行自动蓝图编辑，该问题就会变成结构性可靠性问题。
