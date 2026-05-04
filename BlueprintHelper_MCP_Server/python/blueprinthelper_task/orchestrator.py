from __future__ import annotations

import json
import sys
from typing import Any, Dict, List

from .errors import TaskSpecCompileError
from .graph_write_append import (
    TASK_COMPILER_RESULT_SCHEMA,
    TASK_PLAN_SCHEMA,
    compile_graph_write_append,
    summarize_task_plan,
    task_plan_to_append_bridge_payload,
)


__all__ = [
    "TASK_COMPILER_RESULT_SCHEMA",
    "TASK_PLAN_SCHEMA",
    "TaskSpecCompileError",
    "compile_graph_write_append",
    "main",
    "summarize_task_plan",
    "task_plan_to_append_bridge_payload",
]


def main(argv: List[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    command = args[0] if args else ""
    if command != "compile-graph-write-append":
        _write_json({
            "ok": False,
            "error": {
                "code": "unsupported_python_task_command",
                "message": f"Unsupported command: {command}",
                "issues": [],
            },
        })
        return 2

    try:
        request = json.load(sys.stdin)
        result = compile_graph_write_append(
            request["task_spec"],
            bool(request.get("dry_run", True)),
        )
        _write_json({"ok": True, "result": result})
        return 0
    except TaskSpecCompileError as exc:
        _write_json({"ok": False, "error": exc.to_dict()})
        return 0
    except Exception as exc:
        _write_json({
            "ok": False,
            "error": {
                "code": "python_task_internal_error",
                "message": str(exc),
                "issues": [],
            },
        })
        return 0


def _write_json(value: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(value, ensure_ascii=False, separators=(",", ":")))
    sys.stdout.write("\n")
