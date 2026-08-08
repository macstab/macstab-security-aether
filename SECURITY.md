# Security policy

## Supported use

Aether is a **research and lab** metamorphic engine. It is **not** a general-purpose
offensive implant and is not supported for unauthorized access, mass infection,
or production deployment against systems you do not own or have written permission
to test.

## Reporting vulnerabilities

If you find a vulnerability in Aether itself (build system, library crash, path
traversal in lab helpers, unsafe defaults):

1. Prefer private disclosure to your security contact / maintainer.
2. Include: version/commit, host OS/arch, repro steps, impact.
3. Do **not** file public issues that include live exploit chains against third parties.

## Known residual risk (research honesty)

| Surface                   | Notes                                                      |
|---------------------------|------------------------------------------------------------|
| Controlled ELF infection  | Only the single path named on CLI; operator responsibility |
| Native code exec in tests | Lab/x86-64 only; restricted pure functions                 |
| Stubs                     | Empty advanced techniques — do not treat as implemented    |

See `docs/SCORECARD.md` residual detection surfaces.

## Supply chain

- CMake FetchContent: Zydis `v4.1.1` (pinned tag)
- Prefer offline mirror / vendored Zydis for air-gapped company builds
