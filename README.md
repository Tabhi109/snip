# snip

A low-latency (< 5ms) C++20 token pruning engine and Model Context Protocol (MCP) server for AI coding agents (Claude Code, Cursor, Aider, Devin).

`snip` intercepts verbose terminal outputs, build logs, and test runs, pruning repetitive formatting noise, ANSI bloat, and boilerplate while preserving line numbers, file paths, error codes, and stack traces. This reduces context window token consumption by 50% to 90%.

---

## Key Highlights

- **Low Latency (< 5ms Overhead)**: C++20 implementation using precompiled regular expressions, zero-copy `std::string_view` slicing, and memory footprint under 4MB.
- **Two-Tier Architecture**:
  - **Tier 1 (Declarative Rules)**: Custom TOML rules supporting 4 strategies (`columnar`, `filter_replace`, `hierarchy`, `test_runner`).
  - **Tier 2 (Universal Fallback)**: Strips ANSI and OSC 8 hyperlinks, rewinds progress bars, deduplicates sequential logs, and contextually isolates errors on non-zero exit codes.
- **Model Context Protocol (MCP)**: Native stdio JSON-RPC 2.0 server providing `snip_exec`, `snip_read_file` (structural skeleton mode), and `snip_search` (capped hierarchical search).
- **Dollar-Denominated Financial ROI (`snip gain`)**: Embedded BPE tokenizer calculates token savings and estimated cost reduction based on frontier model blended input pricing ($3.00 / 1M tokens).
- **Failure Safety Invariant**: Never alters non-zero exit codes; falls back to raw sanitized output if a custom rule or regex fails.

---

## Benchmark Token Reductions

| Command | Raw Output | `snip` Output | Token Reduction |
| :--- | :--- | :--- | :--- |
| `git status` | 24 lines (verbose tracking hints) | 5 lines (concise porcelain state) | **~81%** |
| `pytest` (passing) | 180 lines (dots, progress, timing) | 1 line summary (`✓ All tests passed`) | **~98%** |
| `pytest` (failure) | 250 lines (passing tests + trace) | Isolated failure assertions & stack traces | **~85%** |
| `docker ps` | Verbose multiline wrapped table | Aligned core columns (`ID, IMAGE, STATUS, PORTS`) | **~50%** |
| `kubectl get pods -o wide` | Wide table with IPs, nodes, gates | Compacted core status columns | **~45%** |
| `build` (errors) | Hundreds of compilation lines | Focal error anchor context window (`[...]`) | **~80%** |

---

## Quick Start

### 1. Installation

```bash
git clone https://github.com/Tabhi109/snip.git
cd snip
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cp build/snip /usr/local/bin/
```

Or via Homebrew:
```bash
brew install Tabhi109/snip/snip
```

Or via install script:
```bash
curl -sSL https://raw.githubusercontent.com/Tabhi109/snip/main/install.sh | bash
```

### 2. Shell Integration (`snip init`)

Activate transparent aliases in your terminal:

```bash
# For Zsh (add to ~/.zshrc):
eval "$(snip init zsh)"

# For Bash (add to ~/.bashrc):
eval "$(snip init bash)"
```

### 3. Agent Integration (MCP)

#### Claude Desktop
Register the `snip` MCP server:
```bash
snip init --claude
```

#### Cursor
View Cursor MCP configuration instructions:
```bash
snip init --cursor
```

Or add directly to `.cursor/mcp.json`:
```json
{
  "mcpServers": {
    "snip": {
      "command": "snip",
      "args": ["mcp"]
    }
  }
}
```

---

## Dollar ROI Ledger (`snip gain`)

Track cumulative token and dollar savings across all command interceptions:

```bash
$ snip gain

┌─────────────────────────────────────────────────────────────┐
│                    snip Token ROI Ledger                    │
├─────────────────────────────────────────────────────────────┤
│  Commands Intercepted : 42                                  │
│  Raw Input Tokens     : 125,480                             │
│  Pruned Output Tokens : 31,210                              │
│  Net Tokens Saved     : 94,270 (75.1% reduction)            │
│  Estimated Cost Saved : $0.2828 USD                         │
├─────────────────────────────────────────────────────────────┤
│  Benchmark : $3.00 / 1M tokens (Claude 3.5 Sonnet / GPT-4o) │
│  Ledger    : ~/.snip/stats.json                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Community Rule Contributions

Adding support for new developer tools is done through declarative TOML rules without writing C++ code.

See [CONTRIBUTING.md](CONTRIBUTING.md) for strategy guides and the golden snapshot fixture workflow.

For architecture details and design invariants, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## License

MIT (c) 2026 Abhinav Tripathi