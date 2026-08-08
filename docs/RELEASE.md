# Release pack (v1.2.0)

## Checklist before tag

- [ ] `VERSION` and `aether.h` version macros match
- [ ] `CHANGELOG.md` has section for this version
- [ ] `./scripts/bootstrap.sh` green
- [ ] `docker build -f Dockerfile.lab-x86_64` green with `native_checked > 0`
- [ ] `docs/SCOPE.md` reviewed (no “undetectable” language)
- [ ] Legal: LICENSE reviewed by counsel for your org (**you** must sign off)
- [ ] Tag: `git tag -a v1.2.0 -m "Aether 1.2.0 final lab release"`

## Artifacts to publish

| Artifact       | How                                            |
|----------------|------------------------------------------------|
| Source tarball | `git archive`                                  |
| Bench JSON     | `artifacts/bench_report.json` from prove-bench |
| Equiv JSON     | `artifacts/equiv_report.json` from prove-equiv |
| Install        | `cmake --install build --prefix /opt/aether`   |

## One-command offline (air-gap)

```bash
# Online machine once:
./scripts/vendor_zydis.sh
# Copy tree offline, then:
cmake -S . -B build -DAETHER_ZYDIS_ROOT=$PWD/third_party/zydis
cmake --build build -j
./scripts/bootstrap.sh --skip-configure   # or full bootstrap after configure
```

## Marketing line (approved)

Lab morph engine with equivalence-enforced CI and regenerable bench.  
**Not** an undetectable implant product.

## GitHub Actions (Macstab pipeline)

CI and release workflows follow the proven **chaos-testing-libraries** shape:

| Workflow | Trigger | Role |
|----------|---------|------|
| `.github/workflows/ci.yml` | push / PR | format, matrix build+test, prove-industry, lab Docker |
| `.github/workflows/release.yml` | version tags | package tarball + SHA256SUMS + GitHub Release |
| `.github/workflows/codeql.yml` | main / schedule | CodeQL C/C++ |

### Cut a release

```bash
# ensure VERSION matches tag
git tag v1.0.0
git push origin v1.0.0
```

Requires `contents: write` (default `GITHUB_TOKEN`).  
Optional bot PR for changelog: `BOT_GITHUB_TOKEN`, `BOT_USER_NAME`, `BOT_USER_EMAIL` (same as chaos).

License: CI sets `AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE`.
