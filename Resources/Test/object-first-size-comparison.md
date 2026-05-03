# BlueprintHelper Object-First Export Size Comparison Report

**Test Asset:** `Bp_PlayerCharacterBase` (33 nodes, 12 exec + 17 data links, 3 graphs)
**Date:** 2026-05-01

---

## 1. RawJson Export Size Comparison

| Metric | Size (chars) | vs Old Format | Notes |
|--------|-------------|---------------|-------|
| Pure JSON body (baseline) | 4,618 | — | Zero escape overhead |
| Old `json:"<string>"` | 5,494 | 100% | String-first: double JSON.stringify |
| New `payload:<object>` | 5,137 | -6.5% | Object-first: single JSON.stringify |
| **MCP `resource_ref` metadata** | **258** | **-95.3%** | Tool response: metadata + rawUri |
| MCP text field (resource read) | 5,484 | -0.2% | Single-escaped, not double-escaped |
| `structuredContent` + `json` | 5,134 | -6.6% | json field = native object (0 escape) |

### Escape Overhead Analysis

| Layer | Characters | Escape Overhead |
|-------|-----------|-----------------|
| Pure JSON object | 4,618 | 0 |
| Single JSON.stringify -> MCP text | 5,484 | +866 chars (+18.8%) |
| Old double JSON.stringify | — | Double-escaped (would need 3x JSON.parse) |
| **structuredContent.json** | **4,618** | **0 escape overhead** |

### Context Window Impact (per tool call)

| Scenario | Chars | AI Token Est. | Savings |
|----------|-------|---------------|---------|
| Old format (inline body) | 5,494 | ~1,373 | — |
| resource_ref (metadata only) | 258 | ~64 | **-95.3%** |
| resource_ref + on-demand read | 5,742 | ~1,435 | +4.5% |

---

## 2. LogicJson Export Size Comparison

| Metric | Size (chars) | vs Old | Notes |
|--------|-------------|--------|-------|
| Logic body (baseline) | 3,131 | — | Semantic summary, strips GUIDs/coords |
| Old `logic:"<string>"` | 3,246 | 100% | logic as escaped JSON string |
| New `logic:<object>` | 3,331 | +2.6% | logic as native object + metadata wrapper |

> New LogicJson adds metadata (+200 chars) but the logic body has 0 escape overhead. Net +2.6% is the cost of metadata.

### LogicJson vs RawJson Compression

| Metric | Chars | Ratio |
|--------|-------|-------|
| RawJson body | 4,618 | 100% |
| LogicJson body | 3,131 | 67.8% |
| **Savings** | 1,487 chars | **32.2% smaller** |

---

## 3. LogicMd Export Size Comparison

| Metric | Size (chars) | vs RawJson | vs LogicJson | Notes |
|--------|-------------|------------|--------------|-------|
| Markdown text | 1,517 | -67.2% | -51.5% | Human-readable, tables |
| Metadata only | 184 | -96.0% | -94.1% | Stats only, no body |

---

## 4. Master Comparison Matrix

| Export Mode | Delivery | Size | vs Old | Escape-Free | AI-Friendly |
|------------|----------|------|--------|-------------|-------------|
| RawJson old | text (string-first) | 5,494 | — | No (double) | Poor |
| RawJson new | object (object-first) | 5,137 | -6.5% | Yes | Good |
| **RawJson resource_ref** | **metadata + link** | **258** | **-95.3%** | N/A | **Best** |
| RawJson resource read | text (single-escaped) | 5,484 | -0.2% | No (MCP protocol) | OK |
| RawJson structuredContent | object | 5,134 | -6.6% | **Yes** | Excellent |
| LogicJson old | text (string-first) | 3,246 | — | No | Poor |
| LogicJson new | object (structuredContent) | 3,331 | +2.6% | **Yes** | Excellent |
| LogicMd | markdown text | 1,517 | — | N/A (md) | **Best** |
| LogicMd metadata | json (stats only) | 184 | — | N/A | Good |

---

## 5. Size Waterfall (from most verbose to most compact)

```
RawJson old (string-first):  5,494 chars  ████████████████████████████████████████████████████
RawJson MCP text (escaped):  5,484 chars  ███████████████████████████████████████████████████
RawJson new (object-first):  5,137 chars  █████████████████████████████████████████████████
RawJson structuredContent:   5,134 chars  █████████████████████████████████████████████████
Pure JSON body (baseline):   4,618 chars  ████████████████████████████████████████████
LogicJson new:                3,331 chars  █████████████████████████████
LogicJson body:               3,131 chars  ██████████████████████████
LogicMd text:                 1,517 chars  ███████████
LogicMd metadata:             184 chars  █
resource_ref metadata:        258 chars  █
```

---

## 6. Recommendations by Use Case

| Use Case | Best Mode | Response Size | Rationale |
|----------|-----------|---------------|-----------|
| AI code review / understanding | `logic_md` | ~1,517 chars | Native markdown, token-efficient, human-readable |
| Structural analysis / refactoring | `logic_json` | ~3,331 chars | Semantic graph, no implementation noise |
| Blueprint import/export between projects | `resource_ref` | 258 chars | Metadata in tool, 4,618 char body on-demand |
| Debugging / compatibility / replay | `legacy_text_json` | ~5,134 chars | Full raw JSON with clean `structuredContent.json` |
| High-frequency tool calls | `resource_ref` | 258 chars | Minimal context window usage (95.3% reduction) |

---

## 7. Summary of Achievements

| Achievement | Metric |
|-------------|--------|
| **resource_ref** context window savings | **-95.3%** (5,494 → 258 chars) |
| **object-first** bridge payload overhead reduction | -6.5% (5,494 → 5,137 chars) |
| **structuredContent.json** escape character elimination | **100%** (4,618 chars, 0 escape overhead) |
| LogicMd compression ratio vs RawJson | -67.2% (4,618 → 1,517 chars) |
| LogicJson compression ratio vs RawJson | -32.2% (4,618 → 3,131 chars) |
