# RFC 001: Declarative Token-Pruning Pipeline & Extensible MCP Architecture

- **Status**: Proposed
- **Author**: Lead Systems Architect (Antigravity)
- **Target Version**: `snip` 0.2.0 - 0.3.0
- **Scope**: Rule Engine, Universal Fallback Engine, Dispatcher Architecture, MCP Server Integration, Memory & Latency Profiling

---

## 1. Executive Summary & Problem Context

`snip` is an ultra-low-latency, zero-overhead C++20 engine designed to eliminate the single largest operational bottleneck for autonomous AI coding agents: **Context Window Bloat and Token Depletion**.

When AI agents (Claude Code, Cursor, Antigravity, Devin) operate in repositories, 70% to 90% of their context windows are consumed by repetitive, non-essential CLI verbosity:
- Unchanged git diff headers, index hashes, and context lines
- Redundant file path prefixes repeated on every single grep/ripgrep match line
- Routine, boilerplate compiler progress logs and passing test results
- Dependency noise (`node_modules`, `target`, `.venv`) in tree and directory traversals

### The Architectural Inflection Point
Currently, `snip` relies on compiled, hardcoded C++ domain parsers (`GitParser`, `SearchParser`, `FileParser`, `TestParser`). While fast, this pattern creates major problems:
1. **Maintenance Overhead**: Supporting 100+ developer CLIs (`docker`, `kubectl`, `npm`, `pnpm`, `cargo`, `gh`, `ruff`, `eslint`, `vitest`, `go test`) in compiled C++ is unmaintainable.
2. **High Contributor Friction**: Open-source community members cannot easily contribute pruning rules for their favorite tools without writing and compiling C++ code.
3. **Lack of Resiliency for Unknown Tools**: Proprietary company scripts, internal build tools, and unmapped CLIs currently bypass `snip` with zero token pruning.

### The Solution: Two-Tier Pruning Pipeline
We introduce an extensible, declarative, two-tier architecture:
- **Tier 1: Declarative Rule Engine (Data-Driven)**: Expressive TOML/JSON rule definitions specify CLI command matching, regex filters, columnar projections, hierarchy groupings, and test result extractions without C++ recompilation.
- **Tier 2: Dependency-Agnostic Universal Fallback Engine**: When an unmapped binary or custom script executes, `snip` runs a deterministic heuristic pipeline: ANSI stripping, carriage return rewinding, consecutive duplicate line deduplication, whitespace compaction, and exit-code-aware error isolation.

```
                           +---------------------------+
                           |     CLI / MCP Invocation  |
                           |    (cmd_args, exit_code)  |
                           +-------------+-------------+
                                         |
                                         v
                         +---------------+---------------+
                         |          Dispatcher           |
                         +---------------+---------------+
                                         |
                       Rule Exists?      |
                     +-------------------+-------------------+
                     |                                       |
                   YES                                      NO
                     v                                       v
         +-----------------------+              +-----------------------+
         |    Tier 1: Engine     |              |    Tier 2: Engine     |
         | Declarative Rules     |              | Universal Fallback    |
         | (TOML / Embedded)     |              | Heuristics            |
         +-----------+-----------+              +-----------+-----------+
                     |                                       |
                     +-------------------+-------------------+
                                         |
                                         v
                         +---------------+---------------+
                         |      Post-Prune Safety Check  |
                         |  (Size check & error recovery)|
                         +---------------+---------------+
                                         |
                                         v
                         +---------------+---------------+
                         |     BPE Tokenizer & Cache     |
                         |  - Token ROI & Metrics        |
                         |  - Recovery Cache (Raw Log)   |
                         +---------------+---------------+
                                         |
                                         v
                                  Terminal / MCP
```

---

## 2. Declarative Rule Engine Specification

### 2.1 Rule Format Choice: TOML
Rules are specified in **TOML** (Tom's Obvious Minimal Language), with an optional equivalent JSON schema for programmatic generation. TOML is selected as the primary human-contributed format because:
1. **Raw String Literals**: TOML supports single-quoted literal strings (`'(\w+):(\d+): (.*)'`) which eliminate escape-sequence hell (`"\\\\w+:\\\\d+:"`) inherent to JSON.
2. **Comment Support**: Critical for explaining *why* certain CLI noise patterns are pruned.
3. **Ecosystem Familiarity**: Ubiquitous in modern developer tooling (`Cargo.toml`, `pyproject.toml`).

### 2.2 Rule Schema Definition

Each rule file resides in `rules/<tool>.toml` or within the embedded binary table.

```toml
# Schema Version 1.0
schema_version = "1.0"
name = "docker-ps"
description = "Prunes docker container listings to high-signal columns"

# Match Criteria
[match]
binary = "docker"
subcommands = ["ps", "container ls"]
exit_codes = [0]       # Empty or omitted matches any exit code

# Strategy Selection: "columnar" | "filter_replace" | "hierarchy" | "test_runner"
strategy = "columnar"

[columnar]
header_row = 0                  # 0-indexed header row
delimiter = '\s{2,}'           # Delimiter between columns (regex)
keep_columns = ["CONTAINER ID", "IMAGE", "STATUS", "PORTS"]
drop_empty_columns = true
max_rows = 50                  # Cap to avoid context overflow
truncation_indicator = "[...+${count} containers omitted...]"
```

#### Strategy 2: `filter_replace` (Line Filtering, Context, and Transformations)
Used for tools like `git diff`, `git status`, `npm install`, `brew`:

```toml
schema_version = "1.0"
name = "git-diff"
description = "Eliminates unified diff header bloat and limits unchanged context lines"

[match]
binary = "git"
subcommands = ["diff"]

strategy = "filter_replace"

[filter_replace]
# Drop exact metadata lines
drop_lines_matching = [
    '^diff --git',
    '^index [0-9a-f]+\.\.[0-9a-f]+',
    '^--- a/',
    '^new file mode [0-9]+',
    '^deleted file mode [0-9]+',
    '^similarity index [0-9]+%'
]

# Regex transformations with capture groups
[[filter_replace.transforms]]
pattern = '^\+\+\+ b/(.*)$'
replace = 'file: $1'

[[filter_replace.transforms]]
pattern = '^@@ -[0-9]+,[0-9]+ \+[0-9]+,[0-9]+ @@(.*)$'
replace = '@@$1'

# Context control for unchanged lines (' ' prefix)
[filter_replace.context_collapse]
context_prefix = " "
max_consecutive_context = 1
```

#### Strategy 3: `hierarchy` (Grouped Columnar / Search Matching)
Used for `rg`, `grep`, `semgrep`, `eslint`, `ruff`:

```toml
schema_version = "1.0"
name = "ripgrep"
description = "Hierarchically groups search results by file, suppressing repeated path tokens"

[match]
binary = "rg"
fallback_binaries = ["grep"]

strategy = "hierarchy"

[hierarchy]
# Regex pattern extracting named groups: file, line, col (optional), content
pattern = '^(?<file>[^:\n]+):(?<line>[0-9]+):(?<content>.*)$'

# Grouping format
header_template = "${file}"
entry_template = "  ${line}: ${content}"

max_matches_per_file = 10
max_total_matches = 25
truncation_template = "[+${remaining} more matches suppressed]"
```

#### Strategy 4: `test_runner` (Exit-Code-Aware Test Aggregator)
Used for `pytest`, `cargo test`, `vitest`, `go test`, `ctest`:

```toml
schema_version = "1.0"
name = "pytest"
description = "Collapses passing test suites to one line; isolates failing assertions on error"

[match]
binary = "pytest"
subcommands = ["pytest"]

strategy = "test_runner"

[test_runner]
# Successful execution (exit_code == 0)
[test_runner.on_success]
summary_pattern = '.*(=+ [0-9]+ passed.*in [0-9\.]+s =+).*'
summary_template = "✓ ${1}"
fallback_summary = "✓ All tests passed."

# Failure execution (exit_code != 0)
[test_runner.on_failure]
failure_section_start = '=== FAILURES ===|=== ERRORS ==='
failure_section_end = '=== short test summary info ==='

# Lines to strictly retain within failure section
keep_patterns = [
    '^___+ .* ___+$',           # Test failure header
    '^>\s+',                    # Failing code line
    '^E\s+',                    # Assertion error message
    '.*\.py:[0-9]+: in '        # File location
]

# Noise to filter out inside failure section
drop_patterns = [
    '.*site-packages/.*'
]

# Always extract final short summary info
summary_info_patterns = [
    '^FAILED .*',
    '^ERROR .*',
    '.*= [0-9]+ failed.*='
]
```

### 2.3 Rule Loading & Precedence Model
To support both zero-configuration out-of-the-box use and user/project extensibility, `RuleEngine` implements a 3-layer priority lookup:

```
[Layer 3: Project Local]   .snip/rules/*.toml          (Highest Precedence)
        ^
        | overrides
[Layer 2: User Config]     ~/.config/snip/rules/*.toml
        ^
        | overrides
[Layer 1: Embedded]        Compiled-in binary rules    (Default Baseline)
```

1. **Layer 1 (Embedded)**: Compiled directly into the `snip` binary as raw C++20 string literals. Zero filesystem I/O on hot execution path.
2. **Layer 2 (User Config)**: Loaded from `$XDG_CONFIG_HOME/snip/rules/` (or `~/.config/snip/rules/`).
3. **Layer 3 (Project Local)**: Loaded from the working directory's `.snip/rules/`. Allows repositories to customize pruning for internal scripts.

---

## 3. Tier 2: Universal Fallback Engine (Dependency-Agnostic)

When an unknown binary, internal proprietary tool, or custom script runs without a declarative rule, `snip` runs the **Universal Fallback Engine**.

### 3.1 Fallback Processing Pipeline

```
[Raw Process Output]
        |
        v
[1. Stream Sanitization]
  - Strip ANSI CSI sequences: \033[...[A-Za-z~]
  - Strip OSC & OSC 8 hyperlink sequences: \033]...\007
  - Rewind \r (carriage returns) to capture final state of terminal progress bars
        |
        v
[2. Whitespace & Delimiter Compaction]
  - Strip trailing whitespace on each line
  - Collapse 3+ consecutive blank lines down to at most 1 blank line
  - Normalize tabs to 2 spaces
        |
        v
[3. Consecutive Log Line Deduplication]
  - Detect identical consecutive lines
  - Collapse runs into: "message [repeated xN]"
        |
        v
[4. Exit Code Intelligence]
  +----------------------+-----------------------+
  |                                              |
(Exit Code == 0)                               (Exit Code != 0)
  |                                              |
  v                                              v
[Success Path]                                 [Error Isolation Path]
- If lines <= 100: emit full.                  - Scan for error anchors:
- If lines > 100:                                `error:`, `fatal:`, `Traceback`,
  Emit Head (first 15 lines),                    `panicked at`, `Exception:`,
  Emit Tail (last 25 lines),                     `FAILED`, `undefined reference`
  Insert "[... N lines omitted ...]"           - Keep 2 lines before (context)
                                               - Keep 3 lines after (details)
                                               - If isolation yields empty text,
                                                 safely emit full sanitized text.
        |                                        |
        +-------------------+--------------------+
                            |
                            v
                 [Emit Fallback Output]
```

### 3.2 Error Isolation Heuristic Algorithm
Non-zero exit codes signify failure. The AI agent needs the **root cause**, not 10,000 lines of prior compilation progress.
1. The engine scans for **Error Signatures**:
   - `error:` / `ERROR` / `Error:`
   - `fatal:` / `FATAL`
   - `Traceback (most recent call last):`
   - `panicked at`
   - `AssertionError` / `assertion failed`
   - `Segmentation fault`
   - `undefined reference`
   - `syntax error`
2. **Context Preservation Window**:
   For every matching error anchor line $L_i$, the engine captures lines in $[L_{i-2}, L_{i+3}]$. Overlapping windows are automatically merged.
3. **Safety Fallback**: If the error scan finds zero anchors (e.g., an unusual failure output format), the engine **never emits empty text**; it falls back to full sanitized output, obeying Core Invariant #1 (*Never Break Agent Execution*).

---

## 4. Component Interface Architecture

### 4.1 Parser Interface Evolution (`include/snip/parser.hpp`)
We preserve backward compatibility with `DomainParser` while elevating `ParseResult` to carry execution telemetry:

```cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace snip {

enum class PruneStrategyUsed {
    DeclarativeRule,
    HardcodedDomain,
    UniversalFallback,
    Passthrough
};

struct ParseResult {
    std::string text;
    bool was_compressed{false};
    size_t original_bytes{0};
    size_t compressed_bytes{0};
    PruneStrategyUsed strategy_used{PruneStrategyUsed::Passthrough};
    std::string rule_name{};
};

class DomainParser {
public:
    virtual ~DomainParser() = default;
    virtual ParseResult parse(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        int exit_code
    ) = 0;
};

} // namespace snip
```

### 4.2 Universal Fallback Engine (`include/snip/fallback_engine.hpp`)
```cpp
#pragma once

#include "snip/parser.hpp"
#include <string_view>
#include <vector>

namespace snip {

struct FallbackOptions {
    bool strip_ansi{true};
    bool rewind_cr{true};
    bool deduplicate_lines{true};
    size_t max_consecutive_blank_lines{1};
    size_t head_lines_on_success{15};
    size_t tail_lines_on_success{25};
    size_t success_threshold_lines{80};
};

class FallbackEngine {
public:
    static ParseResult process(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        std::string_view stderr_content,
        int exit_code,
        const FallbackOptions& options = {}
    ) noexcept;

    // Sub-algorithms exposed for direct testing
    static std::string isolate_error_context(std::string_view input);
    static std::string compact_whitespace(std::string_view input, size_t max_blank = 1);
};

} // namespace snip
```

### 4.3 Declarative Rule Engine (`include/snip/rule_engine.hpp`)
```cpp
#pragma once

#include "snip/parser.hpp"
#include <filesystem>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace snip {

enum class RuleStrategyType {
    FilterReplace,
    Columnar,
    Hierarchy,
    TestRunner
};

struct RuleMatch {
    std::string binary;
    std::vector<std::string> subcommands;
    std::vector<int> exit_codes; // empty matches any
};

struct RegexTransform {
    std::regex pattern;
    std::string replacement;
};

struct PruneRule {
    std::string name;
    std::string description;
    RuleMatch match;
    RuleStrategyType strategy;

    // FilterReplace configuration
    std::vector<std::regex> drop_patterns;
    std::vector<RegexTransform> transforms;
    size_t max_context_lines{1};

    // Columnar configuration
    size_t header_row{0};
    std::regex column_delimiter;
    std::vector<std::string> keep_columns;
    size_t max_rows{50};

    // Hierarchy configuration
    std::regex hierarchy_pattern;
    std::string header_template;
    std::string entry_template;
    size_t max_matches_per_file{10};
    size_t max_total_matches{25};

    // TestRunner configuration
    std::regex success_pattern;
    std::string success_template;
    std::regex failure_start_pattern;
    std::regex failure_end_pattern;
    std::vector<std::regex> failure_keep_patterns;
    std::vector<std::regex> failure_drop_patterns;
};

class RuleEngine {
public:
    static RuleEngine& instance();

    bool load_embedded_rules();
    bool load_directory(const std::filesystem::path& dir);
    bool register_rule(PruneRule rule);

    const PruneRule* find_rule(
        const std::vector<std::string>& cmd_args,
        int exit_code
    ) const noexcept;

    ParseResult execute_rule(
        const PruneRule& rule,
        std::string_view content,
        int exit_code
    ) const noexcept;

    void clear() noexcept;

private:
    RuleEngine() = default;
    std::unordered_map<std::string, std::vector<PruneRule>> rule_table_;
};

} // namespace snip
```

### 4.4 Central Dispatcher Pipeline (`src/dispatcher.cpp`)
The new dispatcher coordinates Tier 1, legacy domain parsers, Tier 2 fallback, and safety assertions:

```cpp
ParseResult Dispatcher::route_and_parse(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    std::string_view stderr_content,
    int exit_code
) {
    if (stdout_content.empty() && stderr_content.empty()) {
        return { "", false, 0, 0, PruneStrategyUsed::Passthrough, "" };
    }

    try {
        // Step 1: Check Declarative Rule Engine (Tier 1)
        const auto* rule = RuleEngine::instance().find_rule(cmd_args, exit_code);
        if (rule) {
            auto res = RuleEngine::instance().execute_rule(*rule, stdout_content, exit_code);
            if (res.was_compressed && !res.text.empty()) {
                return res;
            }
        }

        // Step 2: Legacy Domain Parsers (during migration transition)
        auto legacy_res = route_legacy_domain_parsers(cmd_args, stdout_content, exit_code);
        if (legacy_res.was_compressed && !legacy_res.text.empty()) {
            return legacy_res;
        }

        // Step 3: Universal Fallback Engine (Tier 2)
        return FallbackEngine::process(cmd_args, stdout_content, stderr_content, exit_code);

    } catch (const std::exception& ex) {
        // Core Invariant 1: Never Break Agent Execution
        return {
            std::string(stdout_content),
            false,
            stdout_content.size(),
            stdout_content.size(),
            PruneStrategyUsed::Passthrough,
            "error_fallback"
        };
    }
}
```

---

## 5. MCP Server Integration & Native Tool Surface

In addition to CLI shimming, `snip` exposes JSON-RPC 2.0 over `stdio` implementing the **Model Context Protocol** (version `2024-11-05`).

### 5.1 Native Tool Specifications

| Tool Name | Parameters | Purpose | Token Pruning Behavior |
|:---|:---|:---|:---|
| `snip_exec` | `command` (string), `timeout_ms` (opt int) | Execute any shell command natively | Evaluates Tier 1 rule or Tier 2 fallback; attaches BPE metrics |
| `snip_read_file` | `path` (string), `max_depth` (opt int) | Inspect source files in skeleton mode | Strips function bodies, keeps signatures, classes, headers |
| `snip_search` | `query` (string), `path` (opt string), `file_pattern` (opt string) | Structured semantic/regex code search | Hierarchically groups by file; prunes repeated path headers; caps results |

### 5.2 JSON-RPC Request & Response Flow

#### Example: `snip_exec`
**Request (`stdio` -> `snip`):**
```json
{
  "jsonrpc": "2.0",
  "id": "req-101",
  "method": "tools/call",
  "params": {
    "name": "snip_exec",
    "arguments": {
      "command": "pytest"
    }
  }
}
```

**Response (`snip` -> `stdio`):**
```json
{
  "jsonrpc": "2.0",
  "id": "req-101",
  "result": {
    "content": [
      {
        "type": "text",
        "text": "✓ ============================== 42 passed in 0.45s ==============================\n[snip: 1842 -> 18 tokens (1824 saved, 99% reduction)]"
      }
    ],
    "meta": {
      "original_tokens": 1842,
      "pruned_tokens": 18,
      "tokens_saved": 1824,
      "reduction_percent": 99,
      "strategy_applied": "test_runner",
      "rule_name": "pytest"
    }
  }
}
```

#### Example: `snip_search`
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": "req-102",
  "method": "tools/call",
  "params": {
    "name": "snip_search",
    "arguments": {
      "query": "route_and_parse",
      "path": "src"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "req-102",
  "result": {
    "content": [
      {
        "type": "text",
        "text": "src/dispatcher.cpp\n  9: ParseResult Dispatcher::route_and_parse(\nsrc/main.cpp\n  50:     auto parse_res = snip::Dispatcher::route_and_parse(\n[snip: 380 -> 48 tokens (332 saved, 87% reduction)]"
      }
    ]
  }
}
```

---

## 6. Memory and Execution Performance Profile

### 6.1 Performance Budget & Guarantees

| Metric | Target Constraint | Design Enforcement |
|:---|:---|:---|
| **Cold Startup Latency** | **< 1.0 ms** | Zero dynamic library loading; embedded rules stored in read-only data segment (`.rodata`). |
| **Hot Path Rule Matching** | **< 0.05 ms** | Binary name hash table lookup; array scan of subcommands. |
| **Output Parsing Overhead** | **< 3.0 ms** (500KB input) | Single-pass string scanners, `std::string_view` slicing, string buffer pre-allocation (`reserve`). |
| **Max Heap Footprint** | **< 4.0 MB** | Memory recycled per process; no unbounded queues or in-memory caches. |
| **BPE Tokenization Overhead** | **< 1.5 ms** (100KB input) | Compact merge table using packed 64-bit pair keys (`uint32_t + uint32_t`). |

### 6.2 Zero-Allocation & Zero-Copy Optimization Strategies
1. **String View Splitting**: Parsing routines avoid creating temporary `std::string` objects for intermediate line and word scanning.
2. **Predictive Output Reserving**:
   - `FilterReplace`: `out.reserve(input.size() / 2)`
   - `Columnar`: `out.reserve(input.size() / 3)`
   - `Hierarchy`: `out.reserve(input.size() / 2)`
3. **Compile-Time Regexes vs String Scanning**: Before executing full `std::regex` matches, fast scalar `find()` or prefix checks verify anchor triggers (e.g. checking for `'['` or `'error'` before invoking regex engine).
