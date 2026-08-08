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
