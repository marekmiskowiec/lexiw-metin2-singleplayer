# Lexiw Metin2 Singleplayer — changelog

Moje zmiany na MT2009 Classic. Changelog autora paczki to `CHANGELOG.md`
i przy aktualizacji paczki go nie ruszam — ten plik jest tylko mój.

Wersja to wersja paczki autora i numer moich zmian na niej:
`2.21.1-lexiw.1`, `2.21.1-lexiw.2`, … — po aktualizacji paczki do np.
2.22.0 liczę od nowa: `2.22.0-lexiw.1`.

Każdy wpis zaczyna się od części, której dotyczy: `[serwer]`, `[klient]`,
`[panel]`, `[launcher]`, `[repo]`. Szczegóły zmian są w historii Gita.

---

## 2.21.1-lexiw.8 — 2026-10-09 — Wiki serwera: potwory, Metiny, bossowie i mapy

- [panel] Mapy w wiki (Wiki → Mapy): lista map działających w tym świecie z poziomami potworów, liczbą rodzajów potworów, Metinów i bossów; strona każdej mapy z potworami, Metinami, bossami i NPC — poziom, ranga, jak się pojawiają, liczba miejsc, ile stoi naraz i co ile się odradzają (z czasem po przyspieszeniu ze strony Respawny).
- [panel] Wyszukiwarka wiki znajduje też mapy; mapy na stronach potworów prowadzą do stron map.
- [panel] Ten sam potwór pojawiający się tak samo na jednej mapie to jeden wiersz z zakresem czasu respawnu (pliki map dają czasy różniące się o sekundę).

- [panel] Strona każdego potwora, Metina i bossa w wiki: rodzaj, ranga, poziom, PŻ, doświadczenie, obrażenia, obrona, yang z zabicia; rasa i bonus, który na niego działa (np. „Silny przeciw zwierzętom”), odporności i specjalne ataki.
- [panel] „Gdzie się pojawia”: mapy, liczba miejsc, ile stoi naraz (średnio) i co ile się odradza — z plików map działającej gry; gdy strona Respawny przyspiesza respawn, obok czasu z pliku widać czas po przyspieszeniu.
- [panel] Pełny drop potwora z szansą na zabójstwo (zmiany z edytora dropu zaznaczone), wspólny drop dla jego rangi i drop spoza tabel (księgi, Cor Draconis, szkatułki Blasku Księżyca…), z linkami do stron przedmiotów i do edytora dropu.
- [panel] Potwory w wyszukiwarce wiki i na stronach przedmiotów prowadzą do stron potworów.
- [serwer] Nowy skrypt m2-wiki-export: gra przy każdym starcie kopiuje pliki map (regen, Metiny, bossowie, NPC) i grupy potworów do wspólnego folderu, z którego czyta panel. Działa od następnej przebudowy obrazu gry; tym razem uruchomiony raz ręcznie (sam odczyt i kopia, bez restartu).

## 2.21.1-lexiw.7 — 2026-10-09 — Wiki serwera, etap 1: wyszukiwarka i przedmioty

- [panel] Nowa zakładka Wiki w menu: wyszukiwarka przedmiotów i potworów (nazwa albo VNUM), wyniki od razu przy pisaniu; przedmioty, które dropią albo są w szkatułkach, pokazują się najpierw.
- [panel] Strona każdego przedmiotu: rodzaj, klasy, które mogą go nosić, statystyki i bonusy, wszystkie poziomy ulepszeń od +0 do +9 ze statystykami, ceny (kupno w sklepie NPC, sprzedaż do NPC liczona jak w grze — cena / 5 minus podatek, średnia cena w sklepach offline) i skąd go zdobyć: z jakich potworów wypada i z jaką szansą, w jakich szkatułkach jest, wspólny i dodatkowy drop.
- [panel] Dane z tych samych plików dropu i szkatułek, które działają w grze, razem z Twoimi zmianami z edytorów; odświeżają się same po zmianie pliku.
- [panel] Karty w Bazie przedmiotów otwierają stronę przedmiotu w wiki.
- [panel] Potwory w wynikach prowadzą na razie do ich dropu w edytorze — ich strony będą w etapie 2.

## 2.21.1-lexiw.6 — 2026-10-09 — Wiadomości ze świata: ulepszenia +8 i +9

- [panel] Wiadomości ze świata pokazują ulepszenia na +8 i +9 z właściwym poziomem. Gra zapisuje w tabeli ulepszeń przedmiot sprzed ulepszenia, więc dotąd ulepszenie na +8 pojawiało się jako „+7” (np. „Sejmitar+7” zamiast „Sejmitar+8”), ulepszenie na +7 nie pojawiało się wcale, a +9 pojawiłoby się dwa razy. Teraz przedmiot po ulepszeniu jest brany z głównego logu gry, a sposób (kowal, zwój, kuźnia gildii) dalej z tabeli ulepszeń.
- [panel] Ulepszenia na +7 celowo nie trafiają do wiadomości (około 250 dziennie).
- [panel] 13 zapisanych już wiadomości „+7” poprawione na „+8”; teraz są wyróżnione jak rzadkie ulepszenia.

## 2.21.1-lexiw.5 — 2026-10-09 — Panel WWW: wspólny wygląd tabel

- [panel] Sortowanie każdej tabeli kliknięciem nagłówka: malejąco, rosnąco, z powrotem do kolejności strony (strzałka pokazuje kierunek). Liczby sortują się jak liczby („44 614 744”, „91,3%”, „Lv 18”, „13 h”), daty jak daty; puste pola („—”) zawsze na końcu. Działa też z klawiatury (Enter).
- [panel] Tabele z kolumną „Klasa” dostają przyciski Wszystkie / Wojownik / Ninja / Sura / Szaman (np. Osobowości botów); Rankingi i Postacie zostają przy swoim filtrze, który liczy miejsca w klasie.
- [panel] Tabele od 15 wierszy mają pole „Filtruj tabelę…” z licznikiem („22 z 200”).
- [panel] Liczby wyrównane do prawej, podświetlenie wiersza pod kursorem.
- [panel] Bez zmian: tabele w formularzach (ustawienia, edytory dropu i szkatułek) i tabele z wierszami łączącymi kolumny — sortowanie przestawiłoby pola. Sortowanie i filtr dotyczą wierszy widocznych na stronie.

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
