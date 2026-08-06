# Air-gap / offline build

## One-time vendor (machine with network)

```bash
./scripts/vendor_zydis.sh
```

This clones Zydis `v4.1.1` (+ Zycore) into `third_party/zydis/`.

Copy the entire repository (including `third_party/`) to the offline host.

## Offline configure

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DAETHER_ZYDIS_ROOT="$PWD/third_party/zydis"
cmake --build build -j
./scripts/bootstrap.sh --skip-configure
```

## Notes

- Do not rely on CMake FetchContent offline.
- Pin stays at Zydis v4.1.1 unless you deliberately upgrade and re-test gates.
