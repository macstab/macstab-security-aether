# License acceptance (first product)

**Not legal advice.** Have counsel review `LICENSE` before you ship commercially.

## What users must accept

The full agreement is the repository root file:

```text
LICENSE
```

Using the software (download, build, run, link) means the user **accepts** those terms (LICENSE §0).

## How acceptance works in practice

| Channel | Mechanism |
|---------|-----------|
| **Human / lab** | Read `LICENSE`; proceed only if you agree |
| **CI / automation** | `export AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE` |
| **C API** | `aether_accept_license()` after operator review; or env as above |
| **Source files** | Short header points to root `LICENSE` (not a second contract) |

## Per-file notices

Source under `lib/` and public headers under `include/` carry a **short** SPDX-style notice:

- Copyright  
- `SPDX-License-Identifier: LicenseRef-Aether-Research`  
- Pointer to root `LICENSE`  
- Authorized / lawful use only  

They do **not** duplicate the full license text (maintenance nightmare). The **binding** text is always `LICENSE`.

## Product / installer (optional later)

For a GUI or installer, show `LICENSE` and require an **I Accept** checkbox before install. That is stronger UX; the env/API path covers headless labs.

## Recommended release checklist

- [ ] Legal review of `LICENSE`  
- [ ] README links to `LICENSE` + this file  
- [ ] CI sets `AETHER_LICENSE_ACCEPTED`  
- [ ] Headers present on shipped sources  
- [ ] No claim of “public domain” or open free-for-all if terms are restricted  

## Dual-use note

Aether is dual-use research software. Acceptance does not legalize unauthorized testing. Operators remain responsible for authorization and law.
