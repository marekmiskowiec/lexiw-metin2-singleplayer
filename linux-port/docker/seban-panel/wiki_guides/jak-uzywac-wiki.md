---
title: Jak używać wiki
summary: Wyszukiwarka, strony przedmiotów, potworów i map oraz skąd wiki bierze swoje dane.
order: 10
---

Wiki to **encyklopedia tego serwera**: mówi, co istnieje, gdzie to znaleźć i z czego powstaje.
Nie mówi, co jest teraz w sklepach — od tego jest [[page:gielda|Giełda]].

## Wyszukiwarka

Na stronie [[page:wiki|Wiki serwera]] wpisz co najmniej dwie litery nazwy albo numer VNUM.
Wyniki pojawiają się w czterech grupach:

| Grupa | Co znajdziesz |
|---|---|
| Poradniki | pliki z tego folderu, szukane po tytule, opisie i treści |
| Przedmioty | nazwa lub VNUM; na górze te, które skądś wypadają albo leżą w szkatułkach |
| Mapy | całe rodziny map („trzy królestwa razem”) i pojedyncze mapy |
| Potwory, Metiny i bossowie | zwykłe potwory, Metiny i bossowie, z poziomem i rodzajem |

Szukasz pod nazwą z gry? Wpisz fragment („Sejmitar”, „Dziki Pies”). Znasz numer przedmiotu albo potwora (np. z komend GM)? Wpisz VNUM.

## Co jest na stronie przedmiotu

- **Skąd się bierze:** potwory, które go upuszczają (z szansą), szkatułki, które go zawierają, oraz zwykły drop z rangi potworów.
- **Ulepszanie u kowala:** kolejne poziomy +1…+9 z kosztem w yang, szansą i potrzebnymi materiałami. Pod tabelą widać też, jak szły ulepszenia botom, i jak zmieniają się bonusy z poziomem.
- **U kogo kupić:** jeśli sprzedaje go NPC, jest podana cena.
- **Bonusy wrodzone** i kto może go używać (klasa, płeć).

## Co jest na stronie potwora i mapy

- Na stronie potwora: poziom, ranga, odporności, co upuszcza i **gdzie się pojawia**.
- Na stronie mapy: wszystkie potwory, Metiny i bossowie z liczbą miejsc respawnu, tym, ilu stoi naraz, i czasem odnowienia. Szczegóły czytania tej tabeli są w poradniku [[guide:mapy-i-krolestwa]].

## Skąd wiki bierze dane

Wszystko pochodzi z plików działającego serwera: bazy przedmiotów, tabel dropu, plików `regen.txt`, `stone.txt`, `boss.txt` i `npc.txt` z map. Dlatego wiki **nie ma własnej wiedzy** — gdy zmienisz drop albo respawn w panelu, wiki to odzwierciedli.

## Pisanie własnych poradników

Poradnik to zwykły plik `.md` w folderze `wiki_guides` panelu (tam, gdzie leży ten). Początek pliku:

```
---
title: Tytuł poradnika
summary: Jedno zdanie na listę i do wyszukiwarki.
order: 40
---
```

Dalej zwykły Markdown: nagłówki, listy, tabele, bloki kodu. Odnośniki do wiki pisze się w podwójnych nawiasach:

| Zapis | Wynik |
|---|---|
| `[[item:50011]]` | link do przedmiotu (nazwa z bazy) |
| `[[mob:101]]` | link do potwora |
| `[[map:63]]` | link do mapy |
| `[[region:m1]]` | link do rodziny map (m1, m2, m3, loch) |
| `[[guide:nazwa-pliku]]` | link do innego poradnika |
| `[[item:50011|własny tekst]]` | link z innym napisem |

Nieistniejący numer zostanie pokazany na czerwono, więc literówkę widać od razu. Po dopisaniu pliku trzeba przebudować obraz panelu (`docker compose build seban-panel`), bo folder jest jego częścią.
