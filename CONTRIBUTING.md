# Contributing (company / lab)

## Standards

- C++17, clang-format (repo root `.clang-format`)
- No mass infection, network C2, or automatic spread
- Every semantic claim needs a test or scorecard metric
- Prefer honest bounds over marketing language

## Workflow

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./scripts/format.sh --check
```

Before merge:

1. All unit tests green
2. `prove-shock` green when touching morph/equiv
3. No new stubs presented as finished features
4. Update `CHANGELOG.md` under `[Unreleased]` or next version

## Commit messages

Conventional style: `feat:`, `fix:`, `docs:`, `test:`, `chore:`.

## Company forks

Replace `LICENSE` with your corporate license after legal review.
Keep infection single-file constraint unless compliance explicitly signs off.
