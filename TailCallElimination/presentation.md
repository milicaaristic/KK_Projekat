# Tail Call Elimination — objašnjenje algoritma

Tail Call Elimination (eliminacija repnih poziva) je optimizacija koja se bavi
rekurzivnim funkcijama i načinom na koji one troše stek. Prepoznaje posebnu
vrstu poziva — onaj koji je poslednja radnja funkcije pre nego što ona vrati
vrednost — i pretvara ga u skok umesto pravog poziva. Kod repne rekurzije to
znači da se cela rekurzija svodi na običnu petlju: program daje isti rezultat,
ali umesto da za svaki nivo rekurzije zauzme novi okvir na steku, koristi samo
jedan. Time se štedi memorija i izbegava preopterećenje steka (stack overflow)
kod duboke rekurzije, a uklanja se i trošak svakog poziva.

## Ideja

Repni poziv je poziv funkcije koji je poslednja radnja koju funkcija izvrši —
njegov rezultat se vraća direktno pozivaocu, bez ijedne dodatne operacije posle
njega. Na primer, u `return g(x);` poziv funkcije g je repni: čim se g vrati, i
naša funkcija se odmah vraća sa istom vrednošću. Nasuprot tome, `return g(x) + 1;`
nije repni poziv, jer se posle povratka iz g još izvršava sabiranje — funkcija
ima još posla, pa njen okvir mora da sačeka.

Kada funkcija repno poziva samu sebe, radi se o repnoj rekurziji, i to je glavna
meta ove optimizacije.

Suština ideje je sledeća: kada je poziv repni, okvir tekuće funkcije na steku
posle tog poziva više ničemu ne služi — u njemu nema nijedne naredbe koja bi se
izvršila nakon povratka. Zato nema potrebe čuvati ga: umesto da se napravi novi
okvir za rekurzivni poziv, može se ponovo iskoristiti postojeći — samo se prepišu
vrednosti argumenata i skoči na početak funkcije.

Posledica je značajna: obična rekurzija dubine n troši n okvira na steku (O(n)
prostora) i za veliko n dovodi do preopterećenja steka. Posle optimizacije troši
se samo jedan okvir (O(1)).

### Kako repni poziv izgleda u LLVM IR-u
Rekurzivna grana `return sum(n - 1, acc + n);` prevede se u blok `if.end`, koji izgleda ovako (izostavljene
su pomoćne `load`/`store` linije):

```llvm
if.end:
  %sub  = sub nsw i32 %2, 1                        ; n - 1
  %add  = add nsw i32 %3, %4                       ; acc + n
  %call = call i32 @sum(i32 %sub, i32 %add)        ; rekurzivni poziv
  store i32 %call, ptr %retval
  br label %return

return:
  %5 = load i32, ptr %retval
  ret i32 %5
```
Ono što ovaj isečak čini repnim pozivom vidi se ako se prati sudbina rezultata
poziva `%call`: on se ne koristi ni u kakvoj operaciji — samo se prosledi ka
povratnoj vrednosti funkcije i odmah vraća preko `ret`.

## Algoritam

### isTailRecursiveCall — prepoznavanje repnog poziva

Proverava da li je dati poziv `CI` repni rekurzivni poziv funkcije `F`. Razlikuju
se dva slučaja: poziv koji vraća vrednost i `void` poziv.

```cpp
bool isTailRecursiveCall(CallInst *CI, Function &F) {
  // Poziv mora biti funkciji samoj sebi (rekurzija).
  if (CI->getCalledFunction() != &F)
    return false;

  Instruction *Next = CI->getNextNode();

  // void slučaj: call void @F(...) pa odmah skok ka bloku sa 'ret void'.
  if (CI->getType()->isVoidTy()) {
    BranchInst *Br = dyn_cast_or_null<BranchInst>(Next);
    if (!Br || !Br->isUnconditional())
      return false;
    ReturnInst *Ret =
        dyn_cast<ReturnInst>(Br->getSuccessor(0)->getTerminator());
    return Ret && Ret->getReturnValue() == nullptr;
  }

  // Slučaj sa povratnom vrednošću: rezultat poziva se upisuje u slot ...
  StoreInst *SI = dyn_cast_or_null<StoreInst>(Next);
  if (!SI || SI->getValueOperand() != CI)
    return false;

  // ... zatim ide bezuslovni skok ...
  BranchInst *Br = dyn_cast_or_null<BranchInst>(SI->getNextNode());
  if (!Br || !Br->isUnconditional())
    return false;

  // ... ka bloku koji vraća tačno vrednost iz tog istog slota, bez ijedne
  // operacije nad njom (ovo odbacuje npr. 'return f(n-1) + 1').
  ReturnInst *Ret =
      dyn_cast<ReturnInst>(Br->getSuccessor(0)->getTerminator());
  if (!Ret)
    return false;
  LoadInst *LI = dyn_cast_or_null<LoadInst>(Ret->getReturnValue());
  return LI && LI->getPointerOperand() == SI->getPointerOperand();
}
```

- `getCalledFunction() != &F` → poziv mora biti funkciji samoj sebi (rekurzija).
- **`void` slučaj** (`CI->getType()->isVoidTy()`): nema rezultata pa nema ni
  `store`-a — posle poziva mora odmah da stoji bezuslovni skok ka bloku koji se
  završava sa `ret void` (`getReturnValue() == nullptr`).
- **Slučaj sa povratnom vrednošću:** odmah posle poziva mora stajati
  `store <rezultat poziva>, <slot>` (`getValueOperand() != CI` odbacuje sve
  ostalo), pa bezuslovni skok.
- Nije dovoljno da postoji `store` i `br` — blok u koji se skače mora da se
  završava sa `ret`, i to `ret` koji vraća `load` iz **istog slota** u koji je
  rezultat upisan (`LI->getPointerOperand() == SI->getPointerOperand()`). Tek to
  garantuje da rezultat poziva stiže do povratne vrednosti netaknut.
- Ova poslednja provera odbacuje pozive koji samo liče na repne, npr.
  `x = f(n - 1); ... return x + 1;` — tamo se rezultat upisuje u lokalnu
  promenljivu, a blok posle skoka radi `add` umesto `ret`.

Ako bilo koji uslov padne, poziv nije u repnoj poziciji i preskačemo ga.

### findTailRecursiveCalls — pronalaženje svih repnih poziva

Prolazi kroz celu funkciju i skuplja **sve** repne rekurzivne pozive, ne samo
prvi na koji naiđe:

```cpp
std::vector<CallInst *> findTailRecursiveCalls(Function &F) {
  std::vector<CallInst *> Calls;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (CallInst *CI = dyn_cast<CallInst>(&I))
        if (isTailRecursiveCall(CI, F))
          Calls.push_back(CI);
  return Calls;
}
```

- Prolazi kroz sve bazične blokove funkcije, a unutar svakog kroz sve instrukcije.
- Za svaku instrukciju proverava da li je poziv (`dyn_cast<CallInst>`), pa da li
  je baš repni rekurzivni poziv (`isTailRecursiveCall` iz prethodnog koraka).
- Svaki pronađeni poziv dodaje u vektor; ako u funkciji nema nijednog, vektor
  ostaje prazan.
- Funkcija može imati više repnih poziva u različitim granama, npr.
  `if (n > 0) return f(n - 1); if (n < 0) return f(n + 1);` — svi se skupljaju i
  kasnije odmotavaju, a ne samo prvi.
- Time razdvajamo funkcije koje treba optimizovati (imaju bar jedan repni poziv)
  od onih koje preskačemo (npr. `main`).

### createLoopHeader — pravljenje zaglavlja petlje

Priprema teren za petlju tako što deli `entry` blok na dva dela: inicijalni upisi
argumenata ostaju u `entry` (izvršavaju se jednom), a telo funkcije prelazi u novi
blok `header`.

```cpp
BasicBlock *createLoopHeader(Function &F) {
  BasicBlock &Entry = F.getEntryBlock();

  Instruction *SplitPoint = nullptr;
  for (Instruction &I : Entry)
    if (StoreInst *SI = dyn_cast<StoreInst>(&I))
      if (isa<Argument>(SI->getValueOperand()))
        SplitPoint = SI->getNextNode();

  return Entry.splitBasicBlock(SplitPoint, "header");
}
```

- `getEntryBlock()` vraća prvi (ulazni) blok funkcije — u LLVM-u to je blok od
  kog izvršavanje počinje.
- Petlja traži `store` čija je upisana vrednost argument funkcije
  (`isa<Argument>`), tj. inicijalne upise argumenata. Time ih razlikuje od ostalih `store`-ova u telu.
- `SplitPoint` se prepisuje na svakom takvom upisu (bez `break`), pa na kraju
  pokazuje na instrukciju odmah posle **poslednjeg** upisa argumenta — tj. na
  prvu instrukciju pravog tela. Radi za bilo koji broj argumenata.
- `splitBasicBlock(SplitPoint, "header")` seče blok tačno pre te instrukcije:
  `alloca` i `store` argumenata ostaju u `entry`, ostatak prelazi u novi blok
  `header`, a LLVM automatski dodaje `br label %header` na kraj `entry`-ja.

Rezultat na IR-u — `entry` je sada podeljen, a ponašanje nepromenjeno:

```llvm
entry:
  %retval  = alloca i32
  %n.addr  = alloca i32
  %acc.addr = alloca i32
  store i32 %n,   ptr %n.addr
  store i32 %acc, ptr %acc.addr
  br label %header          ; automatski dodat skok

header:                     ; novi blok — telo funkcije
  %0 = load i32, ptr %n.addr
  %cmp = icmp eq i32 %0, 0
  br i1 %cmp, label %if.then, label %if.end
```

### findArgumentSlots — pamćenje stek-slotova parametara

Pravi mapu koja za svaki argument funkcije pamti u kom stek-slotu (`%x.addr`)
on živi. Ta informacija je potrebna da bi se pri skoku znalo *gde* upisati nove
vrednosti argumenata.

```cpp
void findArgumentSlots(Function &F) {
  for (Instruction &I : F.getEntryBlock())
    if (StoreInst *SI = dyn_cast<StoreInst>(&I))
      if (Argument *A = dyn_cast<Argument>(SI->getValueOperand()))
        Slots[A] = SI->getPointerOperand();
}
```

- Prolazi kroz `entry` blok, gde se nalaze inicijalni upisi argumenata.
- Bira samo `store`-ove čija je upisana vrednost argument funkcije
  (`dyn_cast<Argument>`), tj. upise oblika `store i32 %n, ptr %n.addr`.
- `getPointerOperand()` je odredište upisa (slot `%n.addr`); pamti se u mapi kao
  „argument `A` → njegov slot".
- Rezultat za `sum`: `{ n → %n.addr, acc → %acc.addr }`. Mapa je član strukture,
  pa se na početku `runOnFunction` čisti (`Slots.clear()`).

### replaceCallWithJump — pretvaranje poziva u skok

Srce transformacije: repni poziv zamenjuje upisom novih vrednosti argumenata u
slotove parametara i skokom nazad na `header`.

```cpp
void replaceCallWithJump(Function &F, CallInst *TailCall, BasicBlock *Header) {
  Instruction *Next = TailCall->getNextNode();

  // U void slučaju nema upisa rezultata; skok ide odmah posle poziva.
  StoreInst *RetStore = dyn_cast<StoreInst>(Next);
  BranchInst *OldBr = RetStore ? cast<BranchInst>(RetStore->getNextNode())
                               : cast<BranchInst>(Next);

  IRBuilder<> Builder(OldBr);
  for (unsigned i = 0; i < TailCall->arg_size(); ++i)
    Builder.CreateStore(TailCall->getArgOperand(i), Slots[F.getArg(i)]);
  Builder.CreateBr(Header);

  OldBr->eraseFromParent();
  if (RetStore)
    RetStore->eraseFromParent();
  TailCall->eraseFromParent();
}
```

- `RetStore` je `store <rezultat>` koji sledi odmah posle poziva. Kod `void`
  funkcija njega nema, pa `dyn_cast` vrati `nullptr` — zato se stara grana traži
  ili posle `store`-a, ili odmah posle poziva.
- Koristi se `cast<>` (a ne `dyn_cast<>`) za granu jer je oblik već potvrđen u
  `isTailRecursiveCall` — poznato je da tu stoji bezuslovni skok.
- `IRBuilder<> Builder(OldBr)` ubacuje nove instrukcije tačno pre stare grane.
- Petlja upisuje `i`-tu vrednost poziva (`%sub`, `%add`) u slot `i`-tog parametra
  (`%n.addr`, `%acc.addr`) i radi za bilo koji broj argumenata.
- `CreateBr(Header)` dodaje skok nazad na početak petlje.
- Brisanje ide redom `OldBr` → `RetStore` → `TailCall`: vrednost poziva se ne sme
  obrisati dok je `RetStore` još koristi. Upis rezultata se briše samo ako postoji
  (`if (RetStore)`), zbog `void` slučaja.

### runOnFunction — spajanje koraka

Povezuje sve: skupi repne pozive, i ako ih ima, izvede transformaciju.

```cpp
bool runOnFunction(Function &F) override {
  Slots.clear();

  // Nema šta da se prepisuje između iteracija ako funkcija nema argumente.
  if (F.arg_empty())
    return false;

  std::vector<CallInst *> TailCalls = findTailRecursiveCalls(F);
  if (TailCalls.empty())
    return false;

  findArgumentSlots(F);
  BasicBlock *Header = createLoopHeader(F);
  for (CallInst *TailCall : TailCalls)
    replaceCallWithJump(F, TailCall, Header);

  return true;
}
```

- `Slots.clear()` — mapa je član strukture, a `runOnFunction` se poziva za svaku
  funkciju posebno, pa se čisti da se podaci ne preliju iz prethodne funkcije.
- Funkcije bez argumenata se preskaču: kod njih ne postoji nijedan
  `store <arg>, <slot>`, pa nema slotova koje bi petlja prepisivala.
- Funkcije bez repnog poziva se preskaču (`return false`) — npr. `main`.
- Redosled je bitan: `findArgumentSlots` mora pre `createLoopHeader`, da se
  slotovi pročitaju dok je `entry` još ceo.
- Zaglavlje petlje se pravi **jednom**, a zatim se svaki pronađeni repni poziv
  zamenjuje skokom na to isto zaglavlje.
- Povratna vrednost `true` znači da je IR promenjen, `false` da funkcija nije
  dirana.

### Rezultat transformacije

Telo rekurzivne grane pre i posle prolaza pokazuje šta pass radi:

**Pre** — rekurzivni poziv:
```llvm
if.end:
  %sub  = sub nsw i32 %2, 1
  %add  = add nsw i32 %3, %4
  %call = call i32 @sum(i32 %sub, i32 %add)
  store i32 %call, ptr %retval
  br label %return
```

**Posle** — upis novih vrednosti i skok nazad (petlja):
```llvm
if.end:
  %sub = sub nsw i32 %2, 1
  %add = add nsw i32 %3, %4
  store i32 %sub, ptr %n.addr
  store i32 %add, ptr %acc.addr
  br label %header
```

Rekurzivnog poziva više nema; `header` se sada dostiže i iz `entry` (prvi ulaz)
i iz `if.end` (svaka iteracija), što čini petlju. Rezultat programa je nepromenjen.
Ako funkcija ima više repnih poziva, svaki od njih se na isti način zamenjuje
skokom na isto zaglavlje. Kod `void` funkcija razlika je samo u tome što nema
upisa rezultata — poziv prati skok, koji se zamenjuje skokom na `header`.

## Primeri

### Ulazni program

```c
int sum(int n, int acc) {
    if (n == 0)
        return acc;
    return sum(n - 1, acc + n);  // repni poziv
}
```

### Posle — ekvivalentna petlja (koju TCE efektivno proizvodi):

```c
int sum(int n, int acc) {
    while (n != 0) {
        acc = acc + n;
        n = n - 1;
    }
    return acc;
}
```

Rekurzivni poziv je nestao; `sum` se izvršava kao petlja, uz nepromenjen
rezultat (`sum(100, 0)` = 5050).
