# snip Architecture & System Design

`snip` is an ultra-low-latency, zero-overhead C++20 token pruning engine and Model Context Protocol (MCP) server. It intercepts verbose command-line outputs, build logs, and test runs to prune non-essential formatting noise, saving **50% to 90%** of context window tokens for autonomous AI coding agents (Claude Code, Cursor, Aider, Devin).

---

## 1. System Topology

```
                  ┌──────────────────────────────────────────────┐
                  │ AI Coding Agent (Cursor, Claude Code, Aider) │
                  └──────────────────────┬───────────────────────┘
                                         │
                 ┌───────────────────────┴───────────────────────┐
                 │ Interception Vector                           │
                 ├───────────────────────┬───────────────────────┤
                 │ Shell Aliases / Shims │ stdio MCP Server      │
                 │ (`snip <cmd>`)        │ (`snip mcp`)          │
                 └───────────┬───────────┴───────────┬───────────┘
                             │                       │
                             ▼                       ▼
                     ┌───────────────┐       ┌───────────────┐
                     │ ProcessRunner │       │  MCPServer    │
                     └───────┬───────┘       │ (snip_exec,   │
                             │               │ snip_read_file│
                             ▼               │ snip_search)  │
                     ┌───────────────┐       └───────┬───────┘
                     │  Dispatcher   │◄──────────────┘
                     └───────┬───────┘
                             │
            ┌────────────────┴────────────────┐
            ▼                                 ▼
   ┌───────────────────┐             ┌───────────────────┐
   │      Tier 1       │   Mismatch  │      Tier 2       │
   │ Declarative Rules │────────────►│ Universal Fallback│
   │  (`RuleEngine`)   │             │ (`FallbackEngine`)│
   └────────┬──────────┘             └─────────┬─────────┘
            │                                  │
            └────────────────┬─────────────────┘
                             ▼
                    ┌──────────────────┐
                    │   RecoveryCache  │  (Raw output persisted)
                    └────────┬─────────┘
                             ▼
                    ┌──────────────────┐
                    │   StatsManager   │  (BPE metrics & $ ROI)
                    └──────────────────┘
```

---

## 2. Two-Tier Pruning Pipeline

### Tier 1: Declarative Rule Engine (`RuleEngine`)
The first layer inspects command arguments (`binary`, `subcommand`) and process exit codes against a registry of declarative rules.

- **Embedded Defaults (`.rodata`)**: Critical rules (such as `docker-ps` and `kubectl-get`) are compiled directly into the binary as C++ string literals, enabling instant execution with zero external file dependencies.
- **Dynamic Configuration**: At startup, `RuleEngine` defensively loads user-defined rules from `$XDG_CONFIG_HOME/snip/rules/`, `~/.config/snip/rules/`, and local project directories (`.snip/rules/`).
- **Core Transformation Strategies**:
  1. `columnar`: Parses tabular data, extracts desired column keys, drops empty columns, and realigns table boundaries.
  2. `filter_replace`: Eliminates boilerplate headers, drops matched line patterns, and executes regex substitutions.
  3. `hierarchy`: Groups scattered regex matches (e.g. `file:line:content`) under clean filename headers with per-file and total result caps.
  4. `test_runner`: Compresses passing test runs into a single checkmark summary line; on failure, extracts isolated failed assertions and stack traces while discarding passing noise.

### Tier 2: Universal Fallback Engine (`FallbackEngine`)
Any command lacking a dedicated rule automatically routes through the Tier 2 universal fallback pipeline:

1. **Terminal Sanitization (`Sanitizer`)**:
   - Strips ANSI escape sequences (colors, cursor positioning).
   - Strips OSC 8 hyperlink sequences (`\033]8;;...\007`).
   - Rewinds carriage-return (`\r`) progress bars, leaving only the terminal state.
2. **Consecutive Deduplication (`Deduplicator`)**:
   - Collapses identical sequential log lines and polling loops into `[repeated xN]`.
3. **Whitespace Compaction**:
   - Reduces consecutive blank lines while preserving indentation and block structure.
4. **Contextual Error Isolation (Non-Zero Exit Codes)**:
   - When a command fails ($exit\_code \neq 0$), scans for universal error anchors (`error:`, `fatal:`, `panic:`, `Traceback`, `FAILED`, `Exception`).
   - Extracts a focal window (`[anchor - 2, anchor + 3]`) for each match, inserting `[...]` markers over unrelated compile progress.
   - If no error anchors match, safely preserves the entire sanitized output to prevent breaking agent execution.

---

## 3. Byte-Pair Encoding (BPE) & Dollar ROI

`snip` includes an embedded BPE tokenizer simulation ([src/bpe.cpp](file:///Users/abhi/Desktop/Projects/snip/src/bpe.cpp)) that calculates exact token counts before and after pruning:

- **Token Cost Model**: Computes financial savings based on frontier model blended input pricing ($3.00 per 1M tokens / $0.000003 per token).
- **Persistent Ledger**: Appends metrics to `~/.snip/stats.json` across all executions.
- **Dashboard Reporting**: `snip gain` displays a formatted ASCII ledger detailing commands intercepted, raw input tokens, pruned tokens, percentage reduction, and estimated dollar savings.

---

## 4. Model Context Protocol (MCP) Server

`snip mcp` exposes a standards-compliant JSON-RPC 2.0 Model Context Protocol server over standard I/O:

| Tool Name | Parameters | Description |
| :--- | :--- | :--- |
| `snip_exec` | `command` (string) | Executes arbitrary CLI commands with token pruning and BPE metrics. |
| `snip_read_file` | `path` (string) | Reads source code in structural skeleton mode (extracts signatures, collapses function bodies). |
| `snip_search` | `query` (string), `path` (string) | Performs ripgrep/grep search with hierarchical file grouping and result capping. |

---

## 5. Architectural Invariants

1. **Never Break Agent Execution**:
   - Syntax errors or malformed regexes in user TOML rules emit debug warnings to `/tmp/snip_last_run.log` and gracefully fall back to raw sanitized output.
   - Process exit codes are preserved with 100% fidelity.
2. **Signal Over Silence**:
   - Prunes formatting, spinners, and repetitive logs.
   - Strictly preserves file paths, line numbers, error diagnostics, and exception traces.
3. **Zero Latency Overhead (< 5ms)**:
   - Written in modern C++20 with precompiled regular expressions, `std::string_view` zero-copy slicing, and a memory footprint under 4MB.
4. **First-Class Testability**:
   - Golden snapshot fixture runner (`tests/test_fixtures.cpp`) executes declarative before/after comparisons across Catch2 and CTest.
