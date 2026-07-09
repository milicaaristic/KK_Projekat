# Projekat iz predmeta Konstrukcija kompilatora — LLVM optimizacioni prolaz

Projekat sadrži LLVM optimizacioni
prolaz (pass) **Tail Call Elimination**, sa test primerima i pratećom
prezentacijom koja objašnjava algoritam korak po korak.

| Prolaz | Direktorijum | LLVM Pass Manager | Šta radi |
|---|---|---|---|
| **Tail Call Elimination (TCE)** | [`TailCallElimination/`](./TailCallElimination) | _TODO_ | _TODO: kratak opis_ |

---

## Tail Call Elimination

**Fajlovi:** [`TailCallEliminationPass.cpp`](./TailCallElimination/TailCallEliminationPass.cpp) ·
[`test.c`](./TailCallElimination/test.c) ·
[`presentation.md`](./TailCallElimination/presentation.md)

> TODO: opis šta prolaz radi i kako (popunjavamo kasnije).

### Build

> TODO: komande za build.

### Pokretanje

> TODO: komande za pokretanje nad `.ll` fajlom.

---

## Zahtevi

- LLVM / Clang (uključujući `opt`, `llvm-config` i razvojne headere) — pass
  koristi LLVM IR API, pa je potrebna ista major verzija LLVM-a s kojom se
  gradi i s kojom se pokreće `opt`.
- CMake ≥ 3.20.
- Kompajler sa podrškom za C++17.

## Autori

- _TODO: Ime Prezime_
