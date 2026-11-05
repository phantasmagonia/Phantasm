# Contributing to Phantasm

Thanks for your interest in Phantasm. Contributions are welcome under the rules below.

## Contributor License Agreement (required)

Phantasm is published under the GNU GPL version 3 only. The project owner also uses Phantasm
under other terms (for example, in games that are not GPL-licensed). To keep that possible,
**every contribution must come with a Contributor License Agreement (CLA)** before it can be
merged.

By submitting a pull request, patch, or any other contribution, you agree that:

1. You wrote the contribution yourself, or otherwise have the right to submit it.
2. You keep the copyright in your contribution.
3. You grant the Phantasm project owner a perpetual, worldwide, non-exclusive, royalty-free,
   irrevocable license to use, modify, sublicense, and distribute your contribution under
   any license terms, including proprietary ones, in addition to the GPLv3.
4. Your contribution is provided "as is", without warranties.

State your agreement in your first pull request with this line:

> I agree to the Phantasm Contributor License Agreement in CONTRIBUTING.md.

Contributions without this statement cannot be merged. If you are contributing on behalf of
an employer, make sure you are allowed to agree to these terms.

*This CLA text is a plain-language summary, not reviewed legal advice; it may be replaced by a
formal CLA document. Contributions will then be asked to accept that version.*

## How to contribute

- Read [AGENTS.md](AGENTS.md) for the build, test, and code conventions, including the scale
  and platform principles.
- Before submitting: build with zero warnings, run the full test suite
  (`cmake --build --preset debug && ctest --preset debug`), and include benchmark numbers
  (with hardware) for any performance claim.
- Use conventional commit messages (`feat:`, `fix:`, `perf:`, `docs:`, ...).
- Keep pull requests focused: one feature or fix per pull request.
- Do not add art or other assets to this repository; demos live in
  [phantasm-demo-projects](https://github.com/phantasmagonia/phantasm-demo-projects).
