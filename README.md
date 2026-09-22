# snip ✂️

A zero-overhead CLI token optimizer for AI coding agents (Claude Code, Cursor, Aider). 

Built in modern C++20, `snip` intercepts noisy terminal outputs, strips ANSI escape sequences, collapses progress bars, removes non-essential CLI hints, and isolates test failures—saving **50% to 90%** of context window tokens.

---

## Benchmark Highlights

| Command | Raw Output | `snip` Output | Token Reduction |
| :--- | :--- | :--- | :--- |
| `git status` | 24 lines (verbose hints) | 12 lines (clean state) | **~50%** |
| `pytest` (passing) | 180 lines (dots, timing) | 1 line summary | **~98%** |
| `cargo test` (failure) | 400 lines (all tests) | Stack traces & assertion diffs | **~85%** |
| `docker build` (spinners) | Thousands of `\r` tokens | Final status lines | **~90%** |

---

## Installation

### One-Line Install (macOS & Linux)
```bash
curl -sSL [https://raw.githubusercontent.com/](https://raw.githubusercontent.com/)<your-username>/snip/main/scripts/install.sh | bash