---
name: cpp-coding-style
description: Enforce the user's C++ code generation style. Use when Codex writes, edits, reviews, or generates normal C++ code, especially new classes, .h/.cpp pairs, structs, utility functions, static helpers, header/source separation, or code that might otherwise use namespaces.
---

# CPP Coding Style

## Core Rules

Apply these rules when generating normal C++ code. If existing project rules conflict, follow the user's explicit rules in this skill first, then match nearby style where possible.

## Class Files

- Put each class in its own file pair.
- Create exactly one `.h` and one `.cpp` for each class.
- Name the files after the class unless the project has a stricter local convention.
- Do not define multiple classes in one class file pair.
- Keep helper classes separate when they represent a separate responsibility.

Example:

```text
PlayerInventory.h
PlayerInventory.cpp
InventoryItemUtils.h
InventoryItemUtils.cpp
```

## Struct Files

- Group structs by purpose when they are simple data types.
- Put related structs in one purpose-named file pair when that improves clarity.
- Avoid placing unrelated structs in the same file.
- Move behavior-heavy structs to their own file pair if they start acting like independent types.

Example:

```text
InventoryTypes.h
InventoryTypes.cpp
CombatTypes.h
CombatTypes.cpp
```

## Header Rules

- Keep `.h` files declaration-only.
- Do not put function bodies in `.h` files.
- Do not inline constructors, destructors, getters, setters, operators, or helper functions in `.h` files.
- Do not place anonymous helper functions, lambdas used as helpers, or static helper implementations in `.h` files.
- Put only includes, forward declarations, class declarations, struct declarations, enum declarations, member declarations, and function declarations in `.h` files.
- If a template, constexpr, or header-only pattern would require header implementation, avoid that design unless the user explicitly asks for it.

## CPP Rules

- Put every implementation in the matching `.cpp` file.
- Define constructors, destructors, member functions, static member functions, operators, and helper methods in `.cpp`.
- Keep the `.cpp` include order aligned with nearby project files.
- Include the matching header first unless local project style says otherwise.

## Namespace Rules

- Do not use `namespace`.
- Do not use anonymous namespace for file-local static helpers.
- Do not place free functions in namespace scope.
- Do not use namespace-scoped `static` functions.

## Utility Class Replacement

When code would normally use namespace helper functions, wrap those helpers into category-specific utility classes.

- Split helpers by purpose instead of making one large catch-all utility class.
- Use only static functions in stateless utility classes.
- Keep each utility class in its own `.h` and `.cpp` file pair.
- Declare static functions in the utility class header.
- Define static functions in the matching utility class `.cpp`.
- Name utility classes by their domain, such as `FilePathUtils`, `StringParseUtils`, `InventoryItemUtils`, or the project's established equivalent.
- For Unreal Blueprint-exposed helpers, prefer a project-appropriate `U...FunctionLibrary` class.

Example declaration:

```cpp
class FilePathUtils
{
public:
    static bool IsValidAssetPath(const FString& Path);
};
```

Example implementation:

```cpp
#include "FilePathUtils.h"

bool FilePathUtils::IsValidAssetPath(const FString& Path)
{
    return !Path.IsEmpty();
}
```

## Final Check

Before finishing generated C++ code, verify:

- Every class has exactly one matching `.h` and `.cpp`.
- Structs are grouped only by clear purpose.
- Headers contain declarations only.
- All function bodies live in `.cpp`.
- No `namespace` or anonymous namespace remains.
- Former namespace/static helpers are represented as categorized utility classes.
