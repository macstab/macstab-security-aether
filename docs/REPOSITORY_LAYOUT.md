# Repository layout (first product)

```
aether/                          # repository root
├── include/aether/              # PUBLIC headers only
│   ├── aether.h                 # C API (preferred include)
│   └── api/aether.h             # compatibility shim → aether.h
├── lib/aether/                  # library implementation + private headers
│   ├── api/aether_api.cpp
│   ├── meta/                    # morph engine, SSA, rewrite, sim, …
│   ├── common/ noise/ mutate/
│   ├── infect/ runtime/         # research extras (still in core for demos)
│   └── …
├── apps/                        # CLI entrypoints
├── stubs/                       # empty advanced-technique placeholders (include path: stubs/)
├── tests/                       # unit / gate tests
├── docs/                        # product + legal + layout
├── scripts/                     # bootstrap, corpus, industry_end
├── corpus/                      # test binaries (ELF/PE)
├── cmake/                       # future modules
├── CMakeLists.txt
├── LICENSE                      # binding terms
└── VERSION
```

## Include paths

| Consumer | Include |
|----------|---------|
| External app | `#include "aether/aether.h"` (link `aether_core` + Zydis) |
| In-tree apps/tests | same + private `aether/meta/...` via `lib/` |
| Legacy | `#include "aether/api/aether.h"` still works (shim) |

## Build

```bash
export AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE
cmake -S . -B build
cmake --build build --target prove-industry
```
