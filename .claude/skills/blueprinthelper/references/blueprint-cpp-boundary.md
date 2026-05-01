# Blueprint And C++ Boundary

Blueprints are best for presentation flow, orchestration, small gameplay behaviors, editor-visible tuning, simple UMG behavior, and prototype logic.

C++ is preferred for core systems, performance-sensitive loops, networking, save/load systems, heavy math, engine integration, and large reusable architecture.

When a Blueprint function exceeds the setup profile thresholds, suggest refactoring, splitting the graph, or moving the responsibility to C++.

BlueprintHelper MCP must not modify C++ source. Use normal repository tools when the user explicitly asks for source changes.
