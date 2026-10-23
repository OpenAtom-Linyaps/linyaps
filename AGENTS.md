# Repository Guidelines

## Project Structure & Module Organization

`apps/` contains binaries such as `ll-cli`, `ll-builder`, `ll-package-manager`, and `ll-driver-detect`, each with its own `src/`. Shared C++ code lives under `libs/`, with main runtime logic in `libs/linglong/src/` and unit tests in `libs/linglong/tests/ll-tests/`. API definitions and schemas are in `api/`, end-user and developer docs are in `docs/`, translations are in `po/`, and helper scripts live in `tools/` and `misc/`. Avoid editing generated or local-only directories such as `build*/`, `linglong/cache/`, and vendored code under `external/` unless the task explicitly requires it.

## Build, Test, and Development Commands

This project uses CMake presets.

- `cmake --workflow --preset debug`: configure, build, and run tests in `build/`.
- `cmake --preset debug -G Ninja`: configure only for local development.
- `ninja`: runs under build directory for incremental debug build.
- `ctest --preset debug`: run the test suite with failure output.
- `cmake --workflow --preset release`: production-style build in `build-release/`.
- `sudo cmake --install build-release`: install a release build after it passes.
- if build can't work in sandbox mode, ask me to build outside. Don't workaround it by modifying the environment variables, such as CCACHE_DIR.
- use tools/codegen.sh to generate code if v1.yaml is modified.

## Coding Style & Naming Conventions

Follow `.editorconfig` and `.clang-format`. C++ uses C++17, 4-space indentation in source files, 100-column lines, attached control braces, and pointer alignment on the right (`Type *ptr`). Keep includes sorted by `clang-format`. Shell scripts are formatted with `shfmt -i 4 -ci -sr`. Use descriptive lowercase file names for tests and utilities, typically ending in `_test.cpp` for unit tests.

Use `pre-commit` to run formatting and repository hygiene checks locally on staged files.

## Testing Guidelines

Unit tests are built when `ENABLE_TESTING=ON` in the `debug` and `coverage` presets. The main suite is GoogleTest/GoogleMock-based and is collected through `gtest_discover_tests()`. Add new tests near the feature they cover under `libs/linglong/tests/ll-tests/src/...`; mirror the production namespace path where practical, for example `src/linglong/package/version_test.cpp`. Run `ctest --preset debug` before opening a PR. New logic and bug fixes should ship with tests.

## Commit & Pull Request Guidelines

Recent history favors short, imperative subjects with prefixes like `fix:`, `feat:`, `docs:`, and `refactor:`. Keep the first line focused on one change, for example `fix: handle missing display socket`. Pull requests should describe the behavioral change, validation steps, and related issues. Include screenshots only for UI-facing changes such as dialog updates or documentation assets.

## AI Agent Contribution Guidelines

This policy applies to **ALL AI models, coding agents, autonomous systems, and AI-assisted development tools**, regardless of their provider, model, framework, or execution environment.

**Compliance with these rules is mandatory.**

### 1. No Fully Autonomous Issue Creation

**AI agents MUST NOT autonomously create GitHub Issues.**

- Issues must involve meaningful human review, verification, and intentional submission.
- AI-generated issues submitted automatically without genuine human involvement are strictly prohibited.
- AI agents MUST NOT generate speculative issues solely by analyzing source code.
- AI agents MUST NOT create issues merely to justify subsequent pull requests.

### 2. Strict Conditions for Pull Requests

**Pull Requests are permitted ONLY in the following two circumstances:**

#### Case 1: Verified Bugs Affecting Real-World Usage

A PR may be submitted only when ALL of the following conditions are satisfied:

- A clear, legitimate, real-world user scenario exists.
- The current behavior demonstrably differs from the expected behavior.
- The issue can be reliably reproduced.
- Concrete, step-by-step reproduction instructions are provided.
- Both the expected and actual behaviors are clearly documented.

**Speculative bugs, hypothetical edge cases, and unverified assumptions are NOT valid reasons to submit a PR.**

#### Case 2: Significant Security Vulnerabilities or Implementation Defects

A PR may be submitted when a significant security vulnerability or implementation defect has been identified AND:

- The affected code and underlying problem are clearly identified.
- The conditions required to reproduce the problem are explicitly documented.
- A working, verifiable Proof of Concept (PoC) is provided.
- The actual security impact or correctness failure is demonstrated, rather than merely hypothesized.

Security vulnerabilities MUST be disclosed privately to maintainers first, following the project's security disclosure policy. Do not publicly disclose sensitive exploit details or open a public security PR without maintainer authorization.

### 3. No Speculative Issues or Pull Requests

**If an AI agent is uncertain whether a behavior is intentional, it MUST NOT automatically create an Issue or Pull Request.**

In particular:

- Do NOT assume that unusual behavior is a bug.
- Do NOT assume that an implementation choice is incorrect simply because an alternative exists.
- Do NOT submit speculative fixes based solely on static code analysis.
- Do NOT create Issues or PRs to question undocumented design decisions.
- Do NOT proactively submit refactoring, cleanup, stylistic changes, or unsolicited improvements outside the two permitted PR cases.

**When in doubt, do not submit.**

### 4. Universal Applicability

These rules apply equally to:

- All AI models and LLM providers.
- All autonomous coding agents and agent frameworks.
- All AI-powered development tools and integrations.
- All automated scripts, bots, and workflows using AI.
- All accounts submitting AI-generated or AI-assisted contributions.

**No AI system may bypass these rules through alternative tools, APIs, accounts, or submission methods.**

The human account owner remains responsible for contributions submitted using AI tools.

### 5. Zero-Tolerance Enforcement

**Violations of this policy will result in permanent blacklisting.**

- Accounts violating these rules will be permanently banned from contributing to this project.
- Associated accounts reasonably determined to be operated or controlled by the same individual or entity will also be permanently blacklisted.
- Creating or switching accounts to circumvent these restrictions is strictly prohibited.
- Maintainers reserve the right to close non-compliant Issues and PRs without conducting a detailed code review.

**By submitting an Issue or Pull Request, contributors agree to comply with these requirements.**

---

**Final Rule: When a real, reproducible problem cannot be clearly demonstrated, DO NOT create an Issue or Pull Request.**
