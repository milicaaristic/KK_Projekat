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
**Primer:**

Repno-rekurzivna funkcija koja sabira brojeve od 1 do `n` (uz akumulator `acc`):

```c
int sum(int n, int acc) {
    if (n == 0)
        return acc;              
    return sum(n - 1, acc + n);  
}
```

**Posle:**

TCE ovaj repni poziv pretvara u petlju — isti rezultat, ali sa jednim stek okvirom umesto `n`:

```c
int sum(int n, int acc) {
    while (n != 0) {
        acc = acc + n;
        n = n - 1;
    }
    return acc;
}
```

## Algoritam

> TODO

## Primeri

> TODO
