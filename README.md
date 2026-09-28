# snip ✂️

A high-performance, low-latency (< 5ms) C++20 token pruning engine and Model Context Protocol (MCP) server for AI coding agents (**Claude Code**, **Cursor**, **Aider**, **Devin**).

`snip` intercepts verbose terminal outputs, build logs, and test runs, pruning repetitive formatting noise, ANSI bloat, and boilerplate while strictly preserving line numbers, file paths, error codes, and stack traces—slashing **50% to 90%** of context window token consumption.

---

## ⚡ Key Highlights

- **Blazing Performance (< 5ms Overhead)**: Pure modern C++20 engine with precompiled regexes, zero-copy `std::string_view` scanners, and a footprint under 4MB.
- **Two-Tier Resilient Pipeline**:
  - **Tier 1 (Declarative Rules)**: Custom TOML rules with 4 strategies (`columnar`, `filter_replace`, `hierarchy`, `test_runner`).
  - **Tier 2 (Universal Fallback)**: Strips ANSI & OSC 8 hyperlinks, rewinds progress bars, deduplicates logs, and contextually isolates errors on non-zero exit codes.
- **Model Context Protocol (MCP)**: Native stdio JSON-RPC 2.0 server providing `snip_exec`, `snip_read_file` (structural skeleton mode), and `snip_search` (capped hierarchical search).
- **Dollar-Denominated Financial ROI (`snip gain`)**: Embedded BPE tokenizer calculates token savings and estimated dollars saved in frontier models ($3.00 / 1M input tokens).
- **Strict Non-Breaking Invariant**: Never swallows exit codes; seamlessly falls back to raw output on any unexpected syntax or regex error.

---

## 📊 Benchmark Token Reductions

| Command | Raw Output | `snip` Output | Token Reduction |
| :--- | :--- | :--- | :--- |
| `git status` | 24 lines (verbose tracking hints) | 5 lines (concise porcelain state) | **~81%** |
| `pytest` (passing) | 180 lines (dots, progress, timing) | 1 line summary (`✓ All tests passed`) | **~98%** |
| `pytest` (failure) | 250 lines (passing tests + trace) | Isolated failure assertions & stack traces | **~85%** |
| `docker ps` | Verbose multiline wrapped table | Aligned core columns (`ID, IMAGE, STATUS, PORTS`) | **~50%** |
| `kubectl get pods -o wide` | Wide table with IPs, nodes, gates | Compacted core status columns | **~45%** |
| `build` (errors) | Hundreds of compilation lines | Focal error anchor context window (`[...]`) | **~80%** |

---

## 🚀 Quick Start

### 1. Installation

```bash
git clone https://github.com/Tabhi109/snip.git
cd snip
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cp build/snip /usr/local/bin/
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
Auto-configure Claude Desktop with a single command:
```bash
snip init --claude
```

#### Cursor
View Cursor MCP server setup steps:
```bash
snip init --cursor
```

Or add to `.cursor/mcp.json`:
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

## 💰 Dollar ROI Ledger (`snip gain`)

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

## 🛠️ Community Rule Contributions

Adding support for new developer tools takes under 2 minutes using declarative TOML rules without writing any C++.

See [CONTRIBUTING.md](CONTRIBUTING.md) for strategy guides and the golden snapshot fixture workflow.

For in-depth architecture and design invariants, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## 📄 License

MIT © Tabhi109