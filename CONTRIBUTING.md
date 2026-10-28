# Contributing to Contradef

Submit all changes through a pull request to `main`, including documentation and workflow changes. Explain the problem, the resulting behavior, and how the change was validated. Keep instrumentation and countermeasure changes explicit so that they can be reviewed separately from maintenance work.

## Review and merge

The repository owner, `@henriqueborges88`, owns all paths through `.github/CODEOWNERS`. Contributions require owner approval; new reviewable commits invalidate previous approvals. Resolve review conversations before merging.

The owner's administrative exception is limited to the review-approval ruleset and to pull requests. A separate ruleset requires the `Safety regressions` check and the pull request workflow for everyone, including administrators. Direct pushes, force pushes, and deletion of `main` are blocked.

Merged pull request branches are deleted automatically. Security updates also go through pull requests; automatic merging is disabled.

## Validation

Run the isolated safety regression tests with Python 3 and a C++17 compiler:

```sh
python tests/run_regressions.py --compiler g++
```

The runner also supports `clang++` and Zig through `--compiler`. These tests use a simulated Pin API and do not build the pintool DLL. For changes involving Pin integration, provide relevant Windows build and runtime results, or clearly state that those checks were not performed. See the README for build dependencies.

## Workflows and dependencies

Workflow changes are subject to owner review. Use GitHub-owned Actions pinned to full commit SHAs, minimal token permissions, and GitHub-hosted runners. Workflows submitted from external forks require maintainer approval before running.

Dependabot proposes GitHub Actions updates. Dependabot security updates are enabled for supported dependencies detected by GitHub. Vendored YARA source and the separately downloaded Intel Pin kit also require manual dependency review.

Do not commit credentials, private logs, generated analysis output, or malware samples. Report vulnerabilities through the private process described in [SECURITY.md](SECURITY.md).
