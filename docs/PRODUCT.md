# Aether — product brief (company internal)

## What this is

**Aether** is a **metamorphic research platform** for authorized labs:

- Educational + real (Zydis) IR morph pipelines
- Equivalence oracle with multi-domain 0-break gate
- Crypto onion cascade (research init model)
- Controlled single-file ELF infection demo
- Machine-readable proof (`prove-shock`, JSON)

## What this is not

| Not | Why |
|-----|-----|
| Production EDR bypass product | Not designed or warranted for that |
| Auto-spreading worm kit | Explicitly forbidden |
| Full commercial metamorphic engine | Restricted morph domain; honest scorecard |
| Undetectable implant | Live sensors still observe init |

## Value for the company

1. **Training** — engineers learn real morph / poly / cascade mechanics safely.
2. **Detection R&D** — dual-use: model offense to build detectors (see SCORECARD surfaces).
3. **Sales/lab demos** — `aether_shock` + JSON gates for credible demos.
4. **Audit trail** — CI + scorecard prevent overselling.

## Positioning language (approved)

Use:

- “**Lab morph + proof** — equivalence-enforced CI”
- “0 semantic breaks on ≥1k pure + ≥200 ELF-extracted functions under analysis-gated morph”
- “Lab dual oracle: interpret + native EAX on x86-64”
- “Generate equivalent mutants to **stress scanners / detections** in authorized labs”

Avoid:

- “Undetectable”, “FUD-proof”, “APT-grade implant”, “state-level”, “bypass any AV”

## Roadmap hooks

See `docs/BRAINSTORMING.md` §13 and `docs/IMPLEMENTATION_PLAN.md` for scale,
corpus, and safe-permute expansion. Product managers: scope **corpus scale**
and **lab VM fleet** next, not stub C2 features.

## Ownership checklist

- [ ] Legal: LICENSE accepted or replaced with corporate license
- [ ] SecOps: lab hosts allowlisted; no customer production installs
- [ ] Eng: CI green (`ci.yml`) + `prove-shock` on release tags
- [ ] Comms: SCORECARD linked in any external talk / Medium post
