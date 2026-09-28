# Contributing to snip

Adding a new tool parser to `snip` does not require writing C++ code. You can define declarative rules in TOML and verify them using golden snapshot fixtures.

---

## Adding a Declarative Rule

### Step 1: Create a Rule File (`rules/<tool>.toml`)

Create a new configuration file under `rules/` (for example, `rules/npm_test.toml` or `rules/terraform_plan.toml`).

Select one of the four transformation strategies:

#### Strategy 1: `columnar` (Tabular CLI Outputs)
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

#### Strategy 2: `filter_replace` (Pattern Matching and Line Dropping)
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

#### Strategy 3: `hierarchy` (Grouped Path Outputs)
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

#### Strategy 4: `test_runner` (Test Suites)
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

## Seeding Golden Test Fixtures

Every rule is protected by golden snapshot tests to ensure determinism and prevent regressions.

1. **Provide Raw CLI Output**:
   Save raw command output into `tests/fixtures/raw/<tool_name>.txt`.

2. **Define Execution Metadata**:
   Create `tests/fixtures/meta/<tool_name>.toml` specifying arguments and expected process exit code:
   ```toml
   args = ["my_tool", "subcommand"]
   exit_code = 0
   ```

3. **Generate Golden Snapshot**:
   Run the test runner with the update flag to generate or update `tests/fixtures/expected/<tool_name>.txt`:
   ```bash
   ./build/tests/snip_tests --update-fixtures
   ```
   Or via ctest:
   ```bash
   SNIP_UPDATE_FIXTURES=1 ctest --test-dir build -R "Golden Fixtures"
   ```

4. **Verify Snapshot**:
   Inspect `tests/fixtures/expected/<tool_name>.txt` to verify that essential information was preserved and formatting noise was removed.

---

## Running the Test Suite

```bash
# Build the test executable
cmake --build build -j

# Execute all tests
ctest --test-dir build --output-on-failure
```

---

## Architectural Invariants

All contributions must adhere to the following invariants:

1. **Never Break Agent Execution**:
   - If a rule or regex fails, `snip` falls back to sanitized raw output.
   - Non-zero process exit codes are never modified or swallowed.
2. **Signal Over Silence**:
   - Prune formatting, progress bars, and repetitive logs.
   - Never prune file paths, line numbers, error codes, failed assertions, or exception traces.
3. **Zero Latency Overhead (< 5ms)**:
   - Avoid catastrophic regex backtracking.
   - Use string views and minimize heap allocations.
4. **First-Class Testability**:
   - Every rule must be accompanied by fixture test pairs in `tests/fixtures/`.

---

## Building From Source

### Prerequisites
- Modern C++20 compiler (`clang++` >= 14 or `g++` >= 11)
- CMake >= 3.22
- Ninja or Make

```bash
git clone https://github.com/Tabhi109/snip.git
cd snip
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
