# Lexiw Metin2 Singleplayer — changelog

Moje zmiany na MT2009 Classic. Changelog autora paczki to `CHANGELOG.md`
i przy aktualizacji paczki go nie ruszam — ten plik jest tylko mój.

Wersja to wersja paczki autora i numer moich zmian na niej:
`2.21.1-lexiw.1`, `2.21.1-lexiw.2`, … — po aktualizacji paczki do np.
2.22.0 liczę od nowa: `2.22.0-lexiw.1`.

Każdy wpis zaczyna się od części, której dotyczy: `[serwer]`, `[klient]`,
`[panel]`, `[launcher]`, `[repo]`. Szczegóły zmian są w historii Gita.

---

## 2.21.1-lexiw.10 — 2026-10-10 — Giełda: bonusy, okazje, ceny i indeks; Wiki: ulepszanie u kowala

- [panel] Giełda → Okazje: przycisk „🔥 Okazje” (oferty co najmniej 30% poniżej mediany, od największego zysku) i filtr „Taniej od mediany o (%)”; sortowanie po zysku z odsprzedaży i po zniżce. Mediana jest liczona w grupie tych samych przedmiotów z tą samą liczbą dodanych bonusów (kolczyki z pięcioma liniami nie są porównywane z gołymi) i dla Księgi Umiejętności osobno dla każdej umiejętności; wymaga co najmniej 3 ofert. Przy ofercie widać różnicę względem mediany i zysk z odsprzedaży stosu.
- [panel] Giełda → strona ceny przedmiotu (klik w nazwę): najtaniej, mediana, najdrożej i liczba ofert, wykres ceny za sztukę w czasie (godzinowe punkty), przełącznik grupy porównania i 20 najtańszych ofert.
- [panel] Gospodarka → Indeks cen i obieg yang: indeks cen z godzinnych median przedmiotów mających co najmniej 10 ofert (100 = pierwsza zmierzona godzina) z listą najbardziej podrożałych i potaniałych, yang w obiegu, wartość ofert w sklepach i dzienny obrót sprzedaży botów (liczba i yang).
- [panel] Poprawka: tabela doświadczenia w panelu (static/exp_levels.json) była tabelą oficjalnego Metin2, a nie tej z silnika (constants.cpp, exp_table_common) — pasek „Doświadczenie” na kartach botów, w listach i rankingach pokazywał zawyżone wartości (ok. 31% botów miało ponad 100%; np. poziom 20 wymaga 472 000, nie 280 000). Plik jest teraz wygenerowany z tabeli silnika; z nim żaden gracz nie przekracza wymaganego expa.
- [repo] Test dymny panelu (smoke_test.py): otwiera wszystkie strony, które da się otworzyć bez zmian, na prawdziwych danych i zgłasza błędy serwera; uruchomienie: `docker compose exec seban-panel python smoke_test.py`.
- [panel] Przegląd wizualny nowych stron (zrzuty headless na komputerze i pomiar szerokości na telefonie): naprawione wykresy na stronie Indeks cen (cudzysłowy w skrypcie zamieniane na encje HTML — żaden wykres się nie rysował), ikona na stronie ceny przedmiotu, wiek oferty w jednej linii, długa nazwa sklepu ucinana wielokropkiem, liczby w tabeli ulepszeń wiki nie łamią się, wąskie kolumny historii w Aktywności botów.
- [panel] Wiki, ulepszanie: w tabeli poziomów widać też bonusy własne przedmiotu, które rosną z plusem (np. Skórzane Buty: Szybkość ruchu +2% … +15%, obrona stała 1 — to dane gry, nie błąd).
- [panel] Pulpit na telefonie: filtry mapy świata, plakietka z liczbą botów, lista trybów i przyciski karuzeli rankingu nie wychodzą już poza ekran; mapa ma jedną kolumnę zamiast kolumny bocznej 300 px.
- [panel] Narzędzia → Rdzenie gry: każdy proces gry (rdzenie kanałów, db, auth) z liczbą botów, CPU (100% = jeden procesor w pełni zajęty; rdzenie są jednowątkowe), pamięcią i wiekiem pliku statusu, z oznaczeniem „obciążony” (od 60%), „na granicy” (od 90%) i „nieaktualny” (status starszy niż 25 s = zawieszony rdzeń) oraz wykresami CPU i botów z 24 godzin. Collector dopasowuje procesy hosta (/host/proc, NSpid) do katalogów rdzeni przez ich pliki pid; nowa tabela web_seban_core_snapshot (30 dni).
- [panel] Gracze i boty → Rozwój botów: poziomy botów teraz, przedziały poziomów z 7 dni, tempo awansu według poziomu (mediana poziomu ułamkowego na godzinę, ile godzin do następnego poziomu, ilu botów stoi) oraz najszybciej i najwolniej awansujące boty. Collector zapisuje raz na godzinę poziom i exp każdego bota online (web_seban_bot_progress, 30 dni); tempo pojawia się po dwóch migawkach.
- [panel] Gracze i boty → Śmierci botów: z logu zgonów — śmierci w czasie, co zabija najczęściej (potwór, poziom, zgonów na bota, średni poziom ofiar), na których mapach, według poziomu ofiary i boty giną najczęściej; okna 6 godzin, 24 godziny i 7 dni.
- [panel] Karta bota: w zakładce Statystyki sekcja „Rozwój i zgony” (wykres poziomu ułamkowego w czasie i tempo, zgony z 24 godzin i 7 dni z najczęstszymi zabójcami) oraz link z osobowości i persony do ich opisu w wiki.
- [panel] Gospodarka → Majątek botów: yang w rękach botów od 10. poziomu (mediana, średnia, rekord), nierówność (wskaźnik Giniego, udział najbogatszych 1% i 10%), ilu botów ma ile yang i jaką część całego yang, majątek według poziomu, najbogatsze boty (yang plus wartość ofert na straganie) i najwięksi sprzedawcy z 7 dni (liczba sprzedaży i obrót z logu). Pomija tożsamości, które nie zaczęły grać (poziom poniżej 10 i startowa sakiewka zaniżały medianę).
- [panel] Przyspieszenie wolnych stron (pomiar z przeglądarki, pierwsze wejście → kolejne): Sezon i rekordy 3,2 s → 24–100 ms, Przedmioty 1,4 s → 85 ms (lista pokazuje pierwsze 500, jak wyszukiwarka), trendy pulpitu 0,95 s → 6 ms, czat na żywo i wiadomości ze świata 0,75 s → 18 ms, Diagnostyka 1,1 s → 0,16 s, Podsumowanie dnia 3 s → 16 ms po pierwszym wejściu. Metody: wspólna pamięć podręczna z czasem ważności dla zapytań do logu gry (nie ma indeksu po czasie i nie dostanie go), wątek rozgrzewający te odpowiedzi, zanim wygasną, oraz skanowanie logów czatu botów najwyżej co 3 s i do 1,5 MB naraz zamiast przy każdym żądaniu.
- [launcher] Porządki: z Pulpitu zniknęły dwa wyłączone przyciski („Sprawdź aktualizacje”, „Aktualizuj klienta”), a siatka Pulpitu ma trzy rzędy zamiast czterech; strona COOP i karta „Serwer na VPS” są schowane (launcher\branding.json: hideCoop, hideVps — kod zostaje, więc aktualizacje od autora nie kolidują), menu numeruje się od tego, co zostało (01–04); przycisk „Zbierz / wyślij logi” nazywa się „Zbierz logi (ZIP)”, bo nic nie jest wysyłane.
- [launcher] Zmiana języka (PL / EN) działa od razu, bez restartu launchera: każdy tekst zrobiony z pary PL/EN (UI-Text) albo z tabeli (T) jest zapamiętany i po przełączeniu podmieniany na ten sam w drugim języku, razem z menu, podtytułem (nowe pole subtitleEn w branding.json), stopką, przyciskiem języka i polem „Wersje”; teksty tworzone w trakcie pracy (log, statusy) odświeżają się jak zwykle. Samo-test przełącza język tam i z powrotem i sprawdza menu.
- [launcher] COOP zostaje widoczny (hideCoop: false), schowany jest tylko „Serwer na VPS” (hideVps: true).
- [launcher] Kopia bazy po każdym zatrzymaniu („Zatrzymaj i zapisz”, z Dockerem i bez): po zamknięciu serwera launcher zapisuje ZIP z bazami (db-backup-…-auto.zip w folderze backups), zachowuje 5 najnowszych kopii automatycznych (ręczne nigdy nie są usuwane) i nie zostawia rozpakowanego folderu z SQL. Ustawienia w launcher.config.json: autoBackup (domyślnie włączone), autoBackupKeep (5), autoBackupLog (false pomija bazę log — historię — i skraca zatrzymanie). Niepowodzenie kopii tylko ostrzega: świat jest w wolumenie Dockera.
- [launcher] Zatrzymanie mówi, jak się skończyło: „Gra zamknęła się czysto” albo ostrzeżenie, że po 90 s Docker zatrzymał ją siłą (kod 137) i ostatnie minuty postępu botów mogły nie trafić do bazy. Okno pytania o zatrzymanie i podpis przycisku mówią o kopii; pole „Wersje” pokazuje datę i rozmiar ostatniej kopii.
- [launcher] Pasek statusu serwera pokazuje prawdziwe dane: „Serwer: DZIAŁA · 2485 botów · CPU 72%” (liczba botów w świecie i obciążenie najbardziej zajętego rdzenia; z nowego endpointu panelu /api/launcher-status, dane z migawek collectora). Gdy panel jeszcze nie działa, zostaje samo „DZIAŁA”.
- [launcher] Strona „Świat i boty” ma skróty do stron panelu: „Aktywność botów”, „Giełda” i „Rdzenie gry” (otwierają się od razu na tej podstronie; przy widocznej karcie VPS zostają dwa pierwsze). Opis karty rat skrócony, żeby mieścił się w wąskim oknie.
- [launcher] Test dymny: tools/Test-Launcher.ps1 sprawdza składnię wszystkich skryptów, branding.json, eksporty modułu, samo-test okna po polsku i angielsku oraz (gdy panel działa) endpoint statusu; kod wyjścia 0 = wszystko w porządku.
- [repo] README (PL/EN), UPDATING.md i docs/LAUNCHER.md mają krótką sekcję o forku Lexiw: kanał autora wyłączony, jak wgrywać nowe wersje autora przez Gita, co dodał launcher i panel.
- [launcher] Panel „Wersje” pokazuje teraz wersję tego egzemplarza zamiast numerów autora i „najnowszy”: wersję Lexiw (z najnowszego nagłówka CHANGELOG-LEXIW.md), commit, wersję paczki MT2009 i klienta. Samo-test układu (-UiSelfTest) uwzględnia schowane strony i karty.
- [panel] Collector: tabele web_seban_price_now (ceny teraz per przedmiot i grupa) i web_seban_price_history (godzinna historia, 120 dni); w płaskiej tabeli ofert mediana, zniżka i zysk liczone przy każdej migawce. Historia cen i indeks zbierają się od uruchomienia tej funkcji — wykresy ruszą po kilku godzinach.

- [panel] Wiki, strona przedmiotu → Poziomy ulepszeń: przy każdym plusie szansa ulepszenia na następny plus, koszt u kowala (yang) i potrzebne materiały (z linkami do ich stron) — z tabeli ulepszeń serwera; „U botów” to rzeczywiste wyniki botów na tym przedmiocie z logu ulepszeń (od 20 prób, np. Miecz+5: 56% z 312 prób przy 60% w tabeli); „Bez porażki od +0” to szansa dojścia do danego plusa bez jednej porażki.
- [panel] Giełda: bonusy przedmiotu jako kolorowe „pigułki” w wierszu oferty — kolor zależy od rodzaju bonusu: niebieski obrona i odporności, zielononiebieski atak (Silny przeciw, obrażenia, cios krytyczny), fioletowy życie i statystyki (Maks. PŻ, Siła, regeneracja, kradzież), pomarańczowy szybkość i bonusy (yang, exp, drop), szary reszta; czerwone dla wartości ujemnych. Bonusy wbudowane w przedmiot są tylko obrysowane, dodane — wypełnione.

## 2.21.1-lexiw.9 — 2026-10-10 — Giełda: oferty ze wszystkich sklepów

- [panel] Gospodarka → Giełda (oferty): wszystkie oferty ze sklepów offline w jednej liście — ikona, nazwa z plusem, bonusy, ilość, cena za sztukę i stosu, sprzedawca z linkiem do karty, mapa i nazwa sklepu. Kategorie po lewej (miecze, sztylety, łuki, zbroje, hełmy, buty, kamienie, księgi, materiały…) z liczbą ofert; filtry: nazwa/VNUM, klasa, poziom, plus, bonus (i jego minimalna wartość), cena za sztukę; sortowanie po cenie i wieku; 50 ofert na stronę.
- [panel] Kolumna „vs rynek”: cena za sztukę względem mediany tego przedmiotu wśród wszystkich ofert (zielona — wyraźnie taniej, czerwona — drożej). U góry liczby całego rynku (oferty, sklepy, wartość), pod listą najczęściej oferowane i najdroższe oferty.
- [panel] Wiek oferty: collector zapisuje, kiedy zobaczył ofertę po raz pierwszy z obecną ceną (nowa tabela web_seban_offer_seen; zmiana ceny zaczyna wiek od nowa). Wiek liczy się od teraz, nie wstecz.
- [panel] Tooltip jak w grze po najechaniu na ikonę przedmiotu: nazwa z plusem, wymagany poziom, wartość ataku/obrony, bonusy (wbudowane i dodane), kamienie duszy w gniazdach z ich bonusami i cena oferty.
- [panel] Szybkość: Giełda czyta z własnej płaskiej, zindeksowanej tabeli ofert (web_seban_offer_flat), którą collector przebudowuje co 5 minut w jednej transakcji, więc strona odpowiada w kilkadziesiąt milisekund; dane są ze zrzutu (tak jak na stronie Rynek), z godziną zrzutu w opisie.

## 2.21.1-lexiw.8 — 2026-10-10 — Wiki: potwory, mapy i osobowości botów; Aktywność botów; strony listy Postacie

### Wiki — osobowości botów
- [panel] Wiki → Osobowości botów: opis każdego z 13 charakterów (m.in. sześć „Dropków” z ich poziomami blokady expa) i 20 person z systemu Iwakury — zwykłych, rzadkich (Metinolog, Nałogowiec, Szalony Naukowiec, Egzekutor, Szalony Wędkarz) i czterech hazardzistów. Co bot robi, kiedy persona go zajmuje i jak długo, szansa i przerwa świata rzadkich; np. Pogromca metinów to krótki stan sytuacyjny (Metin do 10 poziomów od bota, odpuszcza przy 3 botach z królestwa lub 35% życia), a długą wyprawę prowadzi Metinolog. Przy każdej osobowości liczba botów online.
- [panel] Nastroje i zasady ich zmiany, tiery blokady poziomu Grindera i tabela Prawa Awansu (jaki plus broni, zbroi, tarczy i hełmu bot musi mieć); strona „Osobowości botów” w grupie Gracze i boty linkuje do tych opisów.

### Panel — Aktywność botów
- [panel] Nowa strona Gracze i boty → Aktywność botów: co robią boty online teraz — czynności, cele, ambicje, persony i nastroje — kafelki (boty online, w drużynach, stojące, najczęstsza czynność), tabela „Gdzie są boty” (mapa, liczba, poziomy, najczęstsza czynność) i lista botów stojących w miejscu ze statusem.
- [panel] Historia czynności z doby (kolumna na godzinę): collector zapisuje co 5 minut liczbę botów na każdą czynność w istniejącej tabeli metryk. Wykres wypełni się po kilku godzinach działania serwera.

### Wiki — potwory
- [panel] Strona każdego potwora, Metina i bossa: rodzaj, ranga, poziom, PŻ, doświadczenie, obrażenia, obrona, yang z zabicia; rasa i bonus, który na niego działa (np. „Silny przeciw zwierzętom”), odporności i specjalne ataki.
- [panel] „Gdzie się pojawia”: mapy, liczba miejsc, ile stoi naraz (średnio) i co ile się odradza — z plików map działającej gry; gdy strona Respawny przyspiesza respawn, obok czasu z pliku widać czas po przyspieszeniu.
- [panel] Pełny drop potwora z szansą na zabójstwo (zmiany z edytora dropu zaznaczone), wspólny drop dla jego rangi i drop spoza tabel (księgi, Cor Draconis, szkatułki Blasku Księżyca…), z linkami do stron przedmiotów i do edytora dropu.
- [panel] Potwory w wyszukiwarce wiki i na stronach przedmiotów prowadzą do stron potworów.

### Wiki — mapy
- [panel] Wiki → Mapy: lista map działających w tym świecie z poziomami potworów, liczbą rodzajów potworów, Metinów i bossów; strona każdej mapy z potworami, Metinami, bossami i NPC — poziom, ranga, jak się pojawiają, liczba miejsc, ile stoi naraz i co ile się odradzają (z czasem po przyspieszeniu ze strony Respawny).
- [panel] Pierwsze wioski (M1), drugie wioski (M2), trzecie mapy (M3) i Loch Małp trzech królestw jako jedna strona każda: te same potwory, Metiny i bossowie są w każdym królestwie, więc jeden wiersz na potwora; gdzie królestwa się różnią (liczba miejsc respawnu — inny kształt mapy, np. Metin Ciemności 6 · 5 · 5), liczby stoją po kolei Shinsoo · Chunjo · Jinno, wyróżnione na złoto. NPC — w każdym królestwie inne, o tych samych nazwach — jeden wiersz z linkiem do wersji każdego królestwa. Osobne strony królestw zostają; lista map i strony potworów pokazują je jako jeden wiersz. Na górze każdej strony krótko, czym królestwa się różnią (w Lochu Małp tylko NPC).
- [panel] Ten sam potwór pojawiający się tak samo na jednej mapie to jeden wiersz z zakresem czasu respawnu (pliki map dają czasy różniące się o sekundę).
- [panel] Wyszukiwarka wiki znajduje też mapy (także „wioski”); mapy na stronach potworów prowadzą do stron map.
- [serwer] Nowy skrypt m2-wiki-export: gra przy każdym starcie kopiuje pliki map (regen, Metiny, bossowie, NPC) i grupy potworów do wspólnego folderu, z którego czyta panel. Działa od następnej przebudowy obrazu gry; tym razem uruchomiony raz ręcznie (sam odczyt i kopia, bez restartu).

### Lista Postacie
- [panel] 25 postaci na stronę z numerami stron (1, 2, 3 … ostatnia, Poprzednia / Następna) i podpisem „Postacie 26–50 z 4 500”; widać wszystkie postacie, nie tylko pierwsze 250. Miejsce w kolumnie # liczy się dla całej listy, wyszukiwanie i filtr klas działają między stronami.

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
