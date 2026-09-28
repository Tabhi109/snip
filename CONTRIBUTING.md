# Contributing to snip ✂️

Thank you for helping make `snip` the fastest, most effective token pruning engine for AI coding agents!

Contributing a new tool parser to `snip` does **not** require writing C++ code. You can define declarative rules in TOML and verify them using golden snapshot fixtures in under 2 minutes.

---

## ⚡ Quick Start: Add a Rule in Under 2 Minutes

### Step 1: Create a Declarative Rule (`rules/<tool>.toml`)

Create a new file under `rules/` (for example, `rules/npm_test.toml` or `rules/terraform_plan.toml`).

Select one of the 4 transformation strategies:

#### Strategy A: `columnar` (Tabular CLI Outputs)
For commands that output tabular text where columns should be pruned or aligned (e.g. `docker ps`, `kubectl get`, `ps`, `git branch -v`).

```toml
schema_version = "1.0"
name = "docker-ps"
description = "Prunes docker container listings to high-signal columns"
strategy = "columnar"

[match]
binary = "docker"
subcommands = ["ps", "container ls"]
exit_codes = [0]

[columnar]
header_row = 0
keep_columns = ["CONTAINER ID", "IMAGE", "STATUS", "PORTS"]
drop_empty_columns = true
max_rows = 50
truncation_indicator = "[...+containers omitted...]"
```

#### Strategy B: `filter_replace` (Regex / Line Stripping)
For commands with verbose headers, decorative borders, or boilerplate noise (e.g. `git diff`, `git log`, `terraform plan`).

```toml
schema_version = "1.0"
name = "terraform-plan"
strategy = "filter_replace"

[match]
binary = "terraform"
subcommands = ["plan"]

[filter_replace]
drop_lines_matching = [
  '^Refreshing state...',
  '^\s*Acquiring state lock',
  '^\s*Releasing state lock'
]

[[filter_replace.transforms]]
pattern = '(Plan: \d+ to add, \d+ to change, \d+ to destroy\.)'
replace = '✓ ${1}'
```

#### Strategy C: `hierarchy` (Grouped Path Outputs)
For search, linting, or file-listing commands (e.g. `ripgrep`, `grep`, `eslint`, `flake8`).

```toml
schema_version = "1.0"
name = "ripgrep"
strategy = "hierarchy"

[match]
binary = "rg"

[hierarchy]
pattern = '^([^:]+):(\d+):(.*)$'
header_template = "${file}"
entry_template = "  ${line}: ${content}"
max_matches_per_file = 10
max_total_matches = 25
truncation_template = "[+${remaining} more matches suppressed]"
```

#### Strategy D: `test_runner` (Test Suites)
For test harnesses where passing tests should collapse into a single line while failures isolate assertions and stack traces.

```toml
schema_version = "1.0"
name = "pytest"
strategy = "test_runner"

[match]
binary = "pytest"

[test_runner.on_success]
summary_pattern = '(=+ \d+ passed.*in [\d\.]+s =+)'
summary_template = "✓ ${1}"
fallback_summary = "✓ All tests passed."

[test_runner.on_failure]
failure_section_start = '=== FAILURES ==='
failure_section_end = '=== short test summary info ==='
keep_patterns = ['^E\s+.*', '^FAILED .*']
summary_info_patterns = ['^FAILED .*']
```

---

### Step 2: Seed Golden Test Fixtures

Every rule in `snip` is protected by golden snapshot tests to prevent regressions.

1. **Provide Raw CLI Output**:
   Save realistic raw output from your tool into `tests/fixtures/raw/<tool_name>.txt`:
   ```text
   tests/fixtures/raw/my_tool.txt
   ```

2. **Define Execution Metadata**:
   Create `tests/fixtures/meta/<tool_name>.toml` specifying simulated arguments and exit code:
   ```toml
   args = ["my_tool", "subcommand"]
   exit_code = 0
   ```

3. **Generate Golden Snapshot**:
   Run the test runner with the update flag to generate `tests/fixtures/expected/<tool_name>.txt`:
   ```bash
   ./build/tests/snip_tests --update-fixtures
   ```
   *Or with ctest:*
   ```bash
   SNIP_UPDATE_FIXTURES=1 ctest --test-dir build -R "Golden Fixtures"
   ```

4. **Verify Snapshot**:
   Inspect `tests/fixtures/expected/<tool_name>.txt` to verify that high-signal information was preserved and noise was effectively pruned.

---

### Step 3: Run the Test Suite

```bash
# Build the test executable
cmake --build build -j

# Execute all tests
ctest --test-dir build --output-on-failure
```

---

## 🛡️ Core Engine Invariants

All contributions must honor `snip`'s 4 core architectural invariants:

1. **Never Break Agent Execution**:
   - If a custom regex or rule fails, `snip` gracefully falls back to sanitized raw output.
   - Non-zero process exit codes are **never** swallowed or modified.
2. **Signal Over Silence**:
   - Strip formatting bloat, decorative borders, and verbose progress bars.
   - **Never** prune file paths, line numbers, error codes, failed assertions, or exception traces.
3. **Zero Overhead (< 5ms)**:
   - Rules must avoid catastrophic regex backtracking.
   - All string transformations operate with string views and minimal heap allocations.
4. **First-Class Testability**:
   - Every rule must be accompanied by fixture test pairs in `tests/fixtures/`.

---

## 🛠️ Building From Source

### Prerequisites
- Modern C++20 compiler (`clang++` $\ge 14$ or `g++` $\ge 11$)
- CMake $\ge 3.20$
- Ninja or Make

```bash
git clone https://github.com/Tabhi109/snip.git
cd snip
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
