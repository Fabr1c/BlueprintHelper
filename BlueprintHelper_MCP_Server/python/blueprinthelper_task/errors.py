from __future__ import annotations

from typing import Any, Dict, List


class TaskSpecCompileError(Exception):
    def __init__(self, code: str, message: str, issues: List[Dict[str, Any]]):
        super().__init__(message)
        self.code = code
        self.issues = issues

    def to_dict(self) -> Dict[str, Any]:
        return {
            "code": self.code,
            "message": str(self),
            "issues": self.issues,
        }
