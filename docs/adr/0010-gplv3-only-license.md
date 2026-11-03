# 0010: License future Phantasm versions under GPLv3 only

## Status

Accepted. Supersedes the MIT license used before this decision.

## Context

Phantasm was initially published under MIT. The project owner now wants distributed engine
derivatives to remain available under copyleft terms. GPLv3 was chosen with the understanding
that it permits commercial use and resale, including by companies, and cannot prohibit them.

## Decision

Project-owned Phantasm code and documentation are licensed under GNU GPL version 3 only
(`GPL-3.0-only`), with no linking exception for games. The repository carries the unmodified
official GPLv3 text in
`LICENSE` and an explicit project license notice in `README.md`. Third-party dependencies
retain their own licenses.

## Consequences

People may develop games with Phantasm. When a game linked with the engine is distributed, the
combined program generally must be available under GPLv3 terms, with corresponding source
obligations. Private use does not trigger those distribution duties. Companies may use or sell
Phantasm if they comply with GPLv3. Previously distributed MIT-licensed copies remain available
under MIT terms; this decision applies to future project releases. Game projects in the separate
demo repository must review their licenses before updating to a GPLv3-only Phantasm commit.
