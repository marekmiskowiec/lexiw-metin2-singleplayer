# Lexiw Metin2 Singleplayer — changelog

Moje zmiany na MT2009 Classic. Changelog autora paczki to `CHANGELOG.md`
i przy aktualizacji paczki go nie ruszam — ten plik jest tylko mój.

Wersja to wersja paczki autora i numer moich zmian na niej:
`2.21.1-lexiw.1`, `2.21.1-lexiw.2`, … — po aktualizacji paczki do np.
2.22.0 liczę od nowa: `2.22.0-lexiw.1`.

Każdy wpis zaczyna się od części, której dotyczy: `[serwer]`, `[klient]`,
`[panel]`, `[launcher]`, `[repo]`. Szczegóły zmian są w historii Gita.

---

## 2.21.1-lexiw.4 — 2026-10-09 — Pulpit: ostatnie 24 godziny

- [panel] Na Pulpicie, pod kafelkami, wiersz „Ostatnie 24 godziny” z czterema mini-wykresami: boty online, yang w obiegu (ze zmianą w %), ulepszenia u kowala na godzinę (z odsetkiem udanych) i zgony na godzinę (z liczbą zgonów w PvP). Po najechaniu na wykres widać wartość dla danej chwili.
- [panel] Dane wczytują się po otwarciu strony (Pulpit nie otwiera się wolniej) i są odświeżane co 5 minut.
- [panel] Poprawka: informacja o pochodzeniu na dole paska bocznego znowu jest mała i szara (błąd w stylach z poprzedniej zmiany).

## 2.21.1-lexiw.3 — 2026-10-09 — Panel WWW: nazwa i logo Lexiw Metin2

- [panel] Nazwa panelu LEXIW METIN2 — w pasku bocznym, na banerze Pulpitu i w tytułach kart przeglądarki (zamiast MT2009 PLUS / Seban Panel); pod nazwą „Singleplayer · by Lexiw”. Nazwę dalej można zmienić w Panel webowy.
- [panel] Logo: złota tarcza z literą L (branding\lexiw-mark.svg) obok nazwy w pasku bocznym i jako ikona karty przeglądarki; pełny znak LEXIW METIN2 (branding\lexiw-logo.svg) na stronie logowania.
- [panel] Linki w pasku bocznym: Repozytorium na GitHubie zamiast Discorda i metin2sp.pl autora, bez przycisku „Postaw kawę”; na dole paska informacja o pochodzeniu: MT2009 Classic (ZAXEP), Metin2 Playerbots (Tieru), Seban Panel (Seban).

## 2.21.1-lexiw.2 — 2026-10-09 — Launcher Lexiw Metin2

- [launcher] Nowa nazwa: LEXIW METIN2 (okno „Lexiw Metin2 Singleplayer”, podtytuł „Singleplayer / na bazie MT2009 Classic / by Lexiw”), stopka z informacją o pochodzeniu (MT2009 Classic — ZAXEP, Metin2 Playerbots — Tieru).
- [launcher] Tło bez wtopionego logo MT2009 Singleplayer Plus (launcher\lexiw-background.png); oryginalny plik autora zostaje nietknięty.
- [launcher] Przycisk w nagłówku otwiera Twój panel WWW, a przycisk w pasku bocznym — repozytorium na GitHubie (zamiast metin2sp.pl i Discorda autora); w zakładce COOP nie ma już linku do wsparcia autora.
- [launcher] „Raport błędu (ZIP)” zapisuje paczkę logów (bez haseł) w folderze support-bundles — nic nie jest wysyłane do autora.
- [launcher] Aktualizacje od autora wyłączone: launcher nie łączy się z repozytorium autora ani z jego serwerem zapasowym (bez sprawdzania wersji, pobierania paczek serwera i klienta, adresu zgłoszeń). Nową wersję autora wgrywam przez Gita (upstream → main).
- [launcher] Cała marka w jednym pliku launcher\branding.json — bez niego launcher działa jak u autora.

## 2.21.1-lexiw.1 — 2026-10-09 — Panel: dymki, rankingi, klasy, changelog

- [panel] Pełne dymki przedmiotów na stronie gracza — po najechaniu na broń, hełm czy naszyjnik widać znowu nazwę z plusem (np. Stożkowy Miecz+6) i VNUM; motyw Empire ucinał górę dymka.
- [panel] Postacie GM (Admin, AdminNinja, AdminSura, AdminSzaman) nie stoją już na górze listy Postacie; wyszukiwanie po nicku lub ID dalej je znajduje.
- [panel] Lista Postacie ma kolumnę # — miejsce postaci w rankingu poziomu, także dla postaci znalezionej wyszukiwarką (GM: „—”).
- [panel] Filtr klas (Wojownik, Ninja, Sura, Szaman) w Rankingach — w każdej kategorii — i na liście Postacie; miejsca liczone w obrębie klasy.
- [panel] Pulpit i strona Changelog pokazują wersję Classic z tej paczki zamiast edycji Plus z GitHuba (było 2.28.0 Plus); kafelek „Najnowsza zmiana” już nie rozciąga rzędu kafelków.
- [panel] Zakładka „Moje zmiany (Lexiw)” na stronie Changelog — ten plik.
- [repo] Prywatne repozytorium marekmiskowiec/lexiw-metin2-singleplayer: gałąź upstream z czystą paczką autora (tag upstream-2.21.1), gałąź main z moimi zmianami. Hasła (.env, .env.last-good), kopie zapasowe i stan launchera są poza repo.
