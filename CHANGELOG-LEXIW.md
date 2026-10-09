# Lexiw Metin2 Singleplayer — changelog

Moje zmiany na MT2009 Classic. Changelog autora paczki to `CHANGELOG.md`
i przy aktualizacji paczki go nie ruszam — ten plik jest tylko mój.

Wersja to wersja paczki autora i numer moich zmian na niej:
`2.21.1-lexiw.1`, `2.21.1-lexiw.2`, … — po aktualizacji paczki do np.
2.22.0 liczę od nowa: `2.22.0-lexiw.1`.

Każdy wpis zaczyna się od części, której dotyczy: `[serwer]`, `[klient]`,
`[panel]`, `[repo]`. Szczegóły zmian są w historii Gita.

---

## 2.21.1-lexiw.1 — 2026-10-09 — Panel: dymki, rankingi, klasy, changelog

- [panel] Pełne dymki przedmiotów na stronie gracza — po najechaniu na broń, hełm czy naszyjnik widać znowu nazwę z plusem (np. Stożkowy Miecz+6) i VNUM; motyw Empire ucinał górę dymka.
- [panel] Postacie GM (Admin, AdminNinja, AdminSura, AdminSzaman) nie stoją już na górze listy Postacie; wyszukiwanie po nicku lub ID dalej je znajduje.
- [panel] Lista Postacie ma kolumnę # — miejsce postaci w rankingu poziomu, także dla postaci znalezionej wyszukiwarką (GM: „—”).
- [panel] Filtr klas (Wojownik, Ninja, Sura, Szaman) w Rankingach — w każdej kategorii — i na liście Postacie; miejsca liczone w obrębie klasy.
- [panel] Pulpit i strona Changelog pokazują wersję Classic z tej paczki zamiast edycji Plus z GitHuba (było 2.28.0 Plus); kafelek „Najnowsza zmiana” już nie rozciąga rzędu kafelków.
- [panel] Zakładka „Moje zmiany (Lexiw)” na stronie Changelog — ten plik.
- [repo] Prywatne repozytorium marekmiskowiec/lexiw-metin2-singleplayer: gałąź upstream z czystą paczką autora (tag upstream-2.21.1), gałąź main z moimi zmianami. Hasła (.env, .env.last-good), kopie zapasowe i stan launchera są poza repo.
