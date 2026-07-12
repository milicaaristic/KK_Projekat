# Projekat iz predmeta Konstrukcija kompilatora — LLVM optimizacioni prolaz

Projekat sadrži LLVM optimizacioni
prolaz (pass) **Tail Call Elimination**, sa test primerima i pratećom
prezentacijom koja objašnjava algoritam korak po korak.

| Prolaz | Direktorijum | LLVM Pass Manager | Šta radi |
|---|---|---|---|
| **Tail Call Elimination (TCE)** | [`TailCallElimination/`](./TailCallElimination) | Legacy PM (`FunctionPass`) | Pretvara repno-rekurzivne pozive u petlju, čime rekurzija troši konstantan umesto linearnog prostora na steku. |

---

## Tail Call Elimination

**Fajlovi:** [`TailCallEliminationPass.cpp`](./TailCallElimination/TailCallEliminationPass.cpp) ·
[`tests/`](./TailCallElimination/tests) ·
[`presentation.md`](./TailCallElimination/presentation.md)

Prolaz pronalazi repne rekurzivne pozive — pozive funkcije samoj sebi čiji se
rezultat odmah vraća, bez ijedne operacije posle njih — i pretvara ih u petlju
umesto pravog poziva. Time rekurzija koja bi trošila O(n) stek okvira radi u
konstantnom prostoru (O(1)), bez rizika od preopterećenja steka.

Tok transformacije (nad jednom funkcijom):

1. **Prepoznavanje** — pronađi poziv samom sebi čiji rezultat vodi pravo u `ret`
   (obrazac `call → store → br`), bez međuoperacija.
2. **Zaglavlje petlje** — podeli `entry` blok posle inicijalnih upisa argumenata
   na `entry` + `header`; `header` je meta budućeg skoka.
3. **Slotovi parametara** — zapamti u kom stek-slotu (`%x.addr`) živi svaki parametar.
4. **Poziv → skok** — umesto poziva, upiši nove vrednosti argumenata u slotove
   parametara i skoči nazad na `header`; obriši stari poziv, upis rezultata i granu.

> Prolaz pretpostavlja neoptimizovani IR (`clang -O0` uz `-disable-O0-optnone`),
> gde repni poziv sa povratnom vrednošću ima oblik `call → store → br`.

Korak-po-korak objašnjenje sa primerima nalazi se u
[prezentaciji](./TailCallElimination/presentation.md).

### Build

Prolaz je in-tree `FunctionPass` (legacy PM). Direktorijum `TailCallElimination/`
smešta se u `llvm/lib/Transforms/`, a u `llvm/lib/Transforms/CMakeLists.txt`
dodaje se:

```cmake
add_subdirectory(TailCallElimination)
```

Zatim, iz build direktorijuma LLVM-a:

```bash
ninja LLVMTailCallElimination
```

### Pokretanje

```bash
# C -> LLVM IR (neoptimizovan)
clang -S -emit-llvm -Xclang -disable-O0-optnone tests/sum.c -o sum.ll

# pokretanje passa (iz build direktorijuma LLVM-a)
opt -load lib/LLVMTailCallElimination.so -enable-new-pm=0 \
    -tail-call-elimination -S sum.ll -o sum.opt.ll
```

Prolaz na `stderr` ispisuje svaku funkciju u kojoj je repni poziv pretvoren u
petlju (`TCE: repni poziv pretvoren u petlju u funkciji ...`).

### Test primeri

Primeri se nalaze u [`tests/`](./TailCallElimination/tests):

| Fajl | Šta proverava |
|---|---|
| `sum.c` | osnovni slučaj — dva argumenta, repna rekurzija sa akumulatorom |
| `countdown.c` | jedan argument |
| `three_args.c` | tri argumenta (opštost mapiranja argument → slot) |

## Zahtevi

- LLVM / Clang 17 (uključujući `opt` i `clang`) — pass koristi LLVM IR API, pa je
  potrebna ista verzija LLVM-a s kojom se gradi i s kojom se pokreće `opt`.
- Kompajler sa podrškom za C++17.

## Autor

- Milica Ristić
