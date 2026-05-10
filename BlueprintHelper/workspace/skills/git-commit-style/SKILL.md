---
name: git-commit-style
description: Write Git commit messages in this project's Chinese categorized format. Use when drafting, reviewing, or creating commit messages from staged changes, unstaged changes, git diff, git status, pull request changes, or any user request to commit code.
---

# Git Commit Style

## Workflow

When asked to draft a commit message, inspect the relevant diff before writing. Prefer staged changes with `git diff --cached`; if nothing is staged, inspect `git diff` and `git status --short`.

Do not stage files or create a commit unless the user explicitly asks for that action.

## Format

Write commit messages in this exact categorized format:

```text
新增内容:
1. xxx
2. xxx

修复内容:
1. xxx

优化内容:
1. xxx

杂项内容:
1. xxx

已知问题:
1. xxx
```

## Rules

- Write in Chinese.
- Keep each item concise and action-focused.
- Include every category by default.
- Put `无` under a category when there is no matching change.
- Use `新增内容` for new files, features, APIs, tests, docs, or exposed capabilities.
- Use `修复内容` for bug fixes, correctness fixes, build fixes, crash fixes, and regression fixes.
- Use `优化内容` for refactors, performance improvements, cleanup, ergonomics, readability, and behavior-preserving improvements.
- Use `杂项内容` for dependency, config, formatting, generated file, housekeeping, or uncategorized changes.
- Use `已知问题` for unverified behavior, skipped tests, partial work, migration risks, or known follow-up work.
- Do not invent changes that are not supported by the diff.
- Mention skipped verification in `已知问题`.
