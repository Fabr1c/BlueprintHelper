# BlueprintHelper Graph Patterns / 图表模式

## 中文

本目录保存 BlueprintHelper statement framework 使用的内置 graph pattern binding 数据。

`Resources/GraphPatterns` 中的插件级文件定义默认 aliases、pin aliases 和简单值转换。

项目级覆盖保留给：

```text
Config/BlueprintHelper/GraphPatterns
```

复杂节点生命周期、多节点 fragments、latent behavior 和依赖 schema 的 graph mutation 必须保留在 C++ patterns 中。

## English

This directory stores built-in graph pattern binding data for the BlueprintHelper statement framework.

Plugin-level files in `Resources/GraphPatterns` define default aliases, pin aliases, and simple value conversions.

Plugin-level defaults must stay generic for the whole pattern. Do not add
function-specific aliases, pin aliases, or defaults to built-in pattern files.
Project-specific shorthand belongs under the project override directory, and
function-scoped defaults require an explicit function-scoped binding contract.

Project-level overrides are reserved for:

```text
Config/BlueprintHelper/GraphPatterns
```

Complex node lifecycle, multi-node fragments, latent behavior, and schema-dependent graph mutation must remain in C++ patterns.
