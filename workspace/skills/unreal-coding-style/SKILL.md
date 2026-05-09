---
name: unreal-coding-style
description: Apply this project's Unreal Engine C++ coding and architecture conventions. Use when writing, editing, reviewing, or generating Unreal Engine C++ code, especially .h/.cpp files, UCLASS, USTRUCT, UPROPERTY, UFUNCTION, delegates, logs, component code, data assets, data tables, or module-level architecture.
---

# Unreal Coding Style

## Workflow

Before editing Unreal C++ code, inspect nearby files and follow the existing module style first. Apply the rules below when the local code does not clearly establish a different convention.

Use Unreal Build Tool paths when a build is needed:

- UE 5.6: `F:\UE_5.6\Engine\Build\BatchFiles\Build.bat`
- UE 5.3: `G:\UE_5.3\Engine\Build\BatchFiles\Build.bat`

## Naming

- Use PascalCase for class names by default.
- Use `DA_xxxx` for classes derived from `DataAsset`.
- Use `DT_xxxx` for classes related to `DataTable`.
- Keep event, delegate, and category names clear enough to show their responsibility.

## Comments

- Add a concise Chinese class comment for every class, explaining responsibility and usage.
- Add a concise Chinese function comment for every function, including purpose, inputs, and return value when relevant.
- For member variables, use a direct comment for important notes and `meta = (ToolTip = "...")` for editor-facing purpose.
- Keep comments short and practical.

## Header Layout

Group members by access level in this order:

```cpp
public:
protected:
private:
```

Group related functions with `#pragma region` and `#pragma endregion`. Prefer these common regions when they fit:

- `Init`
- Interface implementation regions such as `Ixxxx`
- `API`
- `Getter & Setter`
- `DELEGATE`
- `Setup`
- `Component`
- `Helper`
- `Runtime`

Use this spacing in headers:

- Leave two blank lines between classes.
- Leave one blank line between different function groups.
- Do not add blank lines between functions in the same group.

## Unreal Macros

- Use `UFUNCTION(BlueprintCallable, Category = "...", meta = (ToolTip = "..."))` for Blueprint-facing public APIs.
- Use `UPROPERTY(BlueprintAssignable, Category = "...")` for assignable delegates.
- Use `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "...", meta = (ToolTip = "..."))` for setup variables.
- Use `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component", meta = (AllowPrivateAccess = "true"))` for private components.
- Use `UPROPERTY(Transient, Category = "...", meta = (ToolTip = "..."))` for runtime-only state.
- Avoid `UFUNCTION` on private helper functions unless Blueprint or reflection access is required.
- Implement simple getters and setters with `FORCEINLINE` in headers. Put complex getters and setters in `.cpp` files.

## Static Helpers

Do not use anonymous namespaces for static helper functions in Unreal gameplay or plugin code. Find or create a module-appropriate `FunctionLibrary`, then place the helper there.

## Logging

- Use `UE_LOGFMT`.
- Define log categories by module, such as `LogGameFlow`.
- Use only `Log`, `Warning`, and `Error` unless nearby code justifies another level.
- Write log messages in concise Chinese.
- Guard editor-only logs with `#if WITH_EDITOR` when they should not appear in release builds.

Example:

```cpp
if (Target == nullptr)
{
#if WITH_EDITOR
    UE_LOGFMT(LogGameFlow, Warning, "UMyClass::RegisterTarget - 注册失败，Target不能为空");
#endif
    return;
}
```

## Architecture

- Split modules by responsibility, with clear interfaces and minimal coupling.
- Prefer componentized design for independent gameplay capabilities.
- Prefer interface, event, or delegate communication between modules instead of direct dependencies.
- Store gameplay data in DataTables or DataAssets when data-driven design is appropriate.
- Reuse code through function libraries, base classes, and interfaces without over-abstracting.
- Consider performance during design, but optimize based on actual needs and bottlenecks.
