## Jak się tu poruszać

- **Menu po lewej** dzieli panel na grupy: Wiki, Świat, Gracze i boty, Wydarzenia, Gospodarka, Ustawienia i Narzędzia. Liczba w nawiasie przy grupie to liczba jej stron.
- **Ctrl+K** (pole „Szukaj stron i sekcji” u góry) znajduje stronę, a na długich stronach ustawień także konkretną sekcję albo pojedyncze pole, np. „raty”, „trudność”, „VIP”.
- **Dzwoneczek** w prawym górnym rogu zbiera powiadomienia: koniec eventu, podsumowanie dnia, nowa wersja Playerbots, alarmy.
- To jedyny panel serwera: działa w kontenerze `seban-panel` (u Ciebie http://127.0.0.1:7794), a przycisk „Panel” w launcherze otwiera go od razu. Dawny „panel klasyczny” został w niego wchłonięty i usunięty (opis niżej).
- Rzeczy, których panel nie robi, robi **launcher** (`Metin2-Launcher.ps1`): start i stop serwera, kopia bazy (automatyczna przy każdym zatrzymaniu), przywracanie kopii, budowanie silnika i paczek klienta.

## Gdzie co ustawić — szybki indeks

| Chcę… | Gdzie |
|---|---|
| zmienić raty EXP / drop / yang | [[ep:manage#raty-i-restart|Gra i serwer → Ustawienia wymagające restartu]] 🔁 |
| zmienić liczbę botów | [[ep:manage#raty-i-restart|Gra i serwer → Liczba botów]] 🔁 |
| przyspieszyć albo zagęścić respawn | [[ep:respawns|Świat → Respawny]] ⚡ (własny czas mapy 🔁) |
| zobaczyć, gdzie teraz stoją Metiny, bossowie, rudy, kwiaty | [[ep:spawn_timers|Świat → Timery spawnów]] |
| zobaczyć punkty spawnu na obrazie mapy | [[ep:wiki_maps|Wiki → Mapy]] → wybrana mapa → „Mapa spawnów” |
| uruchomić event albo ułożyć tygodniowy plan eventów | [[ep:events|Wydarzenia → Planer eventów]] ⚡ |
| zmienić czekanie u Biologa, Stajennego i na księgi | [[ep:manage#trudnosc|Gra i serwer → Poziom trudności]] ⚡ |
| zmienić zachowanie botów (handel, kowal, wojny gildii, noc, PvP…) | [[ep:manage#zachowanie-botow|Gra i serwer → Zachowanie botów]] ⚡ |
| zatrzymać boty przy wejściu / wpuścić je | [[ep:manage#boty-przy-drzwiach|Gra i serwer → Boty przy drzwiach]] ⚡ |
| ustawić, na jakich poziomach boty przechodzą na kolejne mapy | [[ep:bot_progression|Gracze i boty → Progresja botów]] ⚡ |
| włączyć Arezzo, Seon-Hae, alchemię i szarfy, zmienić życie potworów, ustawić Dzieci Kwiaty albo Wielkanoc | [[ep:world_modules|Ustawienia → Moduły świata]] ⚡ |
| sprawdzić, dlaczego bot coś założył albo wystawił za taką cenę | [[ep:decisions_page|Gracze i boty → Decyzje botów]] |
| dać przedmiot, VIP albo Smocze Monety | jednej postaci: [[ep:players|Postacie]] → karta postaci; wielu: [[ep:manage_items|Nadawanie przedmiotów]], [[ep:manage#vip-dla-botow|VIP dla botów]], [[ep:manage#sm-dla-botow|SM dla botów]] |
| dać sobie albo komuś uprawnienia GM | karta postaci → „Nick i uprawnienia” |
| przejąć bota i zagrać nim | karta postaci → „Przejmij bota” |
| założyć konto albo postać | [[ep:accounts|Konta]], [[ep:character_creator|Kreator postaci]] |
| zmienić drop z potwora albo zawartość szkatułki | [[ep:drops|Gospodarka → Drop z potworów]], [[ep:chests|Szkatułki]] (zastosowanie w grze wymaga przycisku „Zastosuj”) |
| sprawdzić ceny i oferty sklepów | [[ep:market|Gospodarka → Giełda]] |
| zrobić pokaz tańca do nagrania | [[ep:dance_show|Wydarzenia → Show tańca]] |
| zmienić wygląd panelu, hasło, odznaki | [[ep:manage_panel|Ustawienia → Panel webowy]] ⚡ |
| zacząć świat od nowa | [[ep:world_reset|Ustawienia → Reset świata]] (najpierw kopia bazy w launcherze) |
| sprawdzić obciążenie | [[ep:system_cores|Rdzenie gry]], [[ep:system|Wydajność serwera]] |

## Pulpit

[[ep:dashboard|Pulpit]] — strona startowa: ostatnie 24 godziny (boty online, zgony, Metiny, ulepszenia), mapa świata botów na żywo, obciążenie Dockera, boty według map, szybkie rankingi i pasek wiadomości. Tu nic się nie ustawia. Codzienne podsumowanie dnia otwiera się z dzwoneczka.

## Wiki

Opis świata dla gracza — wszystko czytane z plików i bazy serwera, więc zgadza się z tym, co jest w grze. Nic się tu nie ustawia.

- [[ep:wiki|Wiki serwera]] — wyszukiwarka przedmiotów, potworów, map i poradników (link „WIKI” na górze stron Wiki).
- [[ep:wiki_guides|Poradniki]] — teksty o grze (np. „Bossowie i Metiny” z wyjaśnieniem respawnu). Pliki `seban-panel/wiki_guides/*.md`.
- [[ep:wiki_bots|Osobowości botów]] — charaktery botów: co robią, kiedy i jak długo.
- [[ep:wiki_maps|Mapy]] — każda mapa z potworami, Metinami, bossami, czasami respawnu i **mapą spawnów** (filtr: Wszystko / Nic / tylko jeden rodzaj).
- [[ep:wiki_equipment?category=bron|Broń]], [[ep:wiki_equipment?category=bizuteria|Biżuteria]], [[ep:wiki_equipment?category=zbroje|Zbroje]] — serie +0…+9 z bonusami i ulepszaczami, filtry klasy, rodzaju i poziomu.

## Świat

- [[ep:maps|Aktywność map]] — natężenie botów na mapach w czasie, boty według przedziałów poziomu, mapa cieplna zdarzeń (zgony, Metiny, umiejętności).
- [[ep:respawns|Respawny]] ✏️ — trzy ustawienia:
  - **Tempo świata** ⚡: co jaki procent zwykłego czasu wracają Metiny i bossowie oraz zwykłe potwory (nigdy szybciej niż co 3 s).
  - **Ile stoi na mapie** ⚡: mnożnik liczby potworów i Metinów/bossów na każdej linii respawnu (NPC, rudy i lochy bez zmian).
  - **Własny czas respawnu** 🔁: sekundy dla jednej mapy (potwory albo Metiny); krótki restart rdzeni.
- [[ep:spawn_timers|Timery spawnów]] — co teraz stoi i gdzie: Metiny, bossowie, minibossy, rudy i kwiaty z dokładnymi współrzędnymi i komendą `/warp`; przy pustych punktach obszar i odliczanie do najbliższego sprawdzenia. Filtry jak na mapie spawnów.
- [[ep:world_feed|Wiadomości ze świata]] — oś czasu zdarzeń (rzadkie ulepszenia, bossowie, legendy).
- [[ep:live_chat|Czat na żywo]] — wiadomości z gry.

## Gracze i boty

- [[ep:players|Postacie]] — lista i wyszukiwarka. **Karta postaci** to najważniejsza strona do pracy z jedną postacią ✏️:
  - podgląd: ekwipunek, magazyn, alchemia, umiejętności, statystyki, kartoteka misji, rozwój i zgony, historia ekwipunku, dziennik zdarzeń bota na żywo;
  - akcje: daj przedmiot lub yang, VIP i Smocze Monety, zmiana nicku, ranga GM, przeniesienie postaci do wioski, teleport siebie do postaci, przejęcie bota, usunięcie postaci (strefa niebezpieczna).
- [[ep:legends_page|Legendy botów]] ✏️ — ranking Czempionów i Chodzących Legend, przełącznik systemu legend.
- [[ep:guilds|Gildie]] — gildie botów i graczy, wojny.
- [[ep:accounts|Konta]] ✏️ — konta serwera, zakładanie kont; karta konta pokazuje jego postacie.
- [[ep:bot_personalities|Osobowości botów]], [[ep:bot_activity|Aktywność botów]], [[ep:bot_progress|Rozwój botów]], [[ep:bot_deaths|Śmierci botów]], [[ep:bot_traps|Pętle śmierci]] — obserwacja: co boty robią, jak szybko rosną, gdzie i od czego giną, kto ginie w kółko.
- [[ep:decisions_page|Decyzje botów]] — co boty zakładały i co wystawiały w sklepach, z pełnym wyjaśnieniem pod „?” (składniki wyniku, skąd przedmiot, jak powstała cena krok po kroku), filtry okresu, rodzaju, oznaczeń i nicku, eksport dla AI. Z karty postaci prowadzi tu link „decyzje i wyjaśnienia →” przy historii ekwipunku.
- [[ep:bot_health|Zdrowie botów]] ✏️ — wyniki osobowości, na co boty wydają czas, powody podróży, migawki „przed i po” zmianie ustawień.
- [[ep:bot_progression|Progresja botów]] ✏️ ⚡ — bramki poziomów: na jakim poziomie bot przechodzi na kolejną mapę, co musi zrobić przed bramką (checklista: Metiny, Biolog, księgi, koń…), postoje w wioskach, limity czasu i wędkowanie.
- [[ep:bot_names|Nazwy botów]] ✏️ — kolejka nicków dla nowych botów, blokady, priorytety.
- [[ep:retired_bots|Emerytury botów]] ✏️ — partie botów, które kończą grę.

## Wydarzenia

- [[ep:events|Planer eventów]] ✏️ ⚡ — eventy od razu (z czasem trwania, bonusem i mapą) albo w tygodniowym kalendarzu; udział botów w Tanaka, Zuo i poszukiwaniu skarbów; lista zakończonych eventów ze statystykami.
- [[ep:rankings|Rankingi]], [[ep:season|Sezon i rekordy]], [[ep:hall_of_fame|Sala chwały]] — tabele rekordów (poziom, umiejętności, +9, Metiny, bossowie, ryby, ruda, zioła).
- [[ep:dance_show|Show tańca]] ✏️ ⚡ — własny zespół tancerzy do nagrań: tancerze z wyglądem i kolorem włosów, szyki, choreografia krok po kroku z efektami i aurami, sceny do wczytania jednym kliknięciem, tryb „Świat bez botów”.
- Battle Pass, Koło Fortuny i ranking tygodniowy istnieją w panelu, ale w MT2009 Classic są ukryte (ta wersja gry ich nie ma).

## Gospodarka

- [[ep:economy|Przedmioty w obiegu]] — yang w obiegu z 7 dni, najczęstsze przedmioty; karta przedmiotu z wykresem ilości.
- [[ep:market|Giełda]] — wszystkie oferty sklepów z filtrami (bonusy, poziom, cena), okazje, sprzedaże i obrót; [[ep:market_index|Indeks cen i obieg yang]].
- [[ep:economy_wealth|Majątek botów]] — kto ile ma, nierówność, najwięksi sprzedawcy.
- [[ep:economy_itemshop|ItemShop i Smocze Monety]] — waluty kont i zakupy.
- [[ep:drops|Drop z potworów]] ✏️ — edytor dropu potwora; zmiany trafiają do gry po „Zastosuj w grze”, z kopiami zapasowymi i przywracaniem.
- [[ep:chests|Szkatułki]] ✏️ — edytor zawartości szkatułek, z tym samym „Zastosuj w grze” i kopiami.
- [[ep:manage_items|Nadawanie przedmiotów]] ✏️ — masowe nadanie przedmiotu botom i graczom według warunków (poziom, klasa, królestwo…).
- [[ep:items_database|Baza przedmiotów]] — katalog VNUM.

## Ustawienia → Gra i serwer

[[ep:manage|Gra i serwer]] ✏️ to jedna długa strona; Ctrl+K skacze do jej sekcji.

| Sekcja | Co ustawia | Kiedy działa |
|---|---|---|
| Ustawienia wymagające restartu | raty EXP, drop, yang; liczba botów; przycisk „Przelicz ceny w sklepach botów teraz” | 🔁 przyciskiem „Zastosuj zmiany i zleć restart” |
| Aktualizator Seban | aktualizacja Playerbots z kopią świata | tylko w trybie VPS (u Ciebie wyłączony) |
| Plan wejścia botów | w ile minut po starcie wchodzi kohorta, dodatkowe „późne” boty | przy kolejnym starcie |
| Boty przy drzwiach | trzymanie botów poza światem / wpuszczenie; Wieża Demonów i rajd na Azraela „teraz” | ⚡ |
| Poziom trudności | czekanie u Biologa, Stajennego, przerwy między księgami (gotowe poziomy albo własne godziny) | ⚡ |
| Autołowy i kanał CH2 | przedmiot wymagany do autołowów; CH2 przy restarcie i udział botów na nim | autołowy ⚡, CH2 🔁 |
| Rajdy na Azraela | rajdy botów 80+ na Diabelskie Katakumby | ⚡ |
| Zachowanie botów | suwaki 0–200% (kowal, księgi, Biolog, stragany, handel…) i przełączniki: noc na serwerze, godziny gry botów na dobę, wojny gildii, Wieża Demonów, ItemShop, sklepy w M2, persona/nastrój, krzykacze, złomiarze, podatek od sprzedaży, odpoczynek, PvP królestw, dno zwoju, targowanie się, Szkatułki Blasku | ⚡ do 5 s |
| Polityka przedmiotów | reguły, co boty robią z danymi przedmiotami (jedna reguła na linię) | ⚡ |
| Boty na mapach | tylko podgląd: ile botów jest teraz na każdej mapie i kanale | — |
| VIP / SM dla botów | VIP albo Smocze Monety dla wszystkich botów | od razu |
| Masowe nadawanie przedmiotów | skrót do strony nadawania | — |
| Skrzynia startowa | Skrzynia Ucznia dla nowych postaci | ⚡ |
| Prawdziwi gracze w rankingach | czy Twoje postacie są w rankingach razem z botami | ⚡ |
| Ogłoszenia ulepszeń +9 | ogłoszenie w grze przy +9 | ⚡ |

## Ustawienia → Moduły świata

[[ep:world_modules|Moduły świata]] ✏️ ⚡ — dodatkowe systemy świata, każdy działa od razu i zostaje po restarcie:

| Sekcja | Co ustawia |
|---|---|
| Arezzo | mapy i lochy Arezzo włączone albo wyłączone |
| Seon-Hae | 6. i 7. bonus u Seon-Hae, czas pracy, szanse poziomów lv1–lv5, reguły dropu odłamków i suplementów |
| Alchemia, Smocze Kamienie i szarfy | Cor Draconis z Metinów i bossów, odłamki u Alchemika (szansa i Cory na dobę), szarfy z bossów |
| Życie potworów | procent życia potworów, bossów i Metinów (10–300%) |
| Dzieci Kwiaty | szanse, koszty, poziomy bonusów kwiatów i nagrody za kwiaty (event włącza Planer eventów) |
| Wielkanoc | osobno Metiny wielkanocne i Wielkanocny Zając |

## Ustawienia → Panel webowy i Reset świata

- [[ep:manage_panel|Panel webowy]] ✏️ ⚡ — nazwa panelu, motyw kolorów (Ocean, Ember, Forest, Cesarstwo), kursor, źródło monitoringu, **hasło do panelu**, alarm bez ruchu, odznaka Top 10 poziomu, odznaka pełnego +9, rozmiar rankingu sprzedaży, wyjaśnienia decyzji sklepów botów, gdzie pokazywać legendarne ogłoszenia, ścieżka magii w rankingach, Szkatułki Umarłego Rozpruwacza.
- [[ep:world_reset|Reset świata]] ✏️ — reset samych botów albo całego świata. Nie da się cofnąć: najpierw kopia bazy w launcherze.

## Narzędzia

- [[ep:character_creator|Kreator postaci]] ✏️ — nowa postać na wybranym koncie.
- [[ep:gm_commands|Komendy GM]] — ściąga komend (np. `/warp X Y` — globalne metry, `/goto X Y` — metry na bieżącej mapie).
- [[ep:system_cores|Rdzenie gry]] i [[ep:system|Wydajność serwera]] — CPU, pamięć, dysk, boty na rdzeń.
- [[ep:diagnostics|Diagnostyka wędkowania]], [[ep:panel_logs|Logi panelu]] (z pobieraniem), [[ep:changelog|Changelog]].
- Ta strona — instrukcja panelu.

## Co robi panel sam w tle

- **Kolektor** (`seban-collector`) co 5 minut zapisuje migawki do wykresów (mapy, rdzenie, ceny, przedmioty) i powiadomienia.
- **Sprzątanie bazy logów** co godzinę: z `log.log` po 3 dniach znika szum, którego nic nie czyta (podniesienie yang i przedmiotów, odczyty kamieni i bonusów, dzielenie stosów, wygaśnięcia…), a okno trwającego eventu zostaje do jego końca. Ulepszenia, śmierci, Metiny, bossowie, handel, umiejętności i decyzje botów zostają na zawsze. Nieczytane tabele logów trzymają 7 dni, pozycje botów 2 dni, sumy przedmiotów 15 dni. Dzięki temu kopia bazy jest mniejsza i szybsza.

## Jeden panel zamiast dwóch

Dawny „panel klasyczny” (kontener `panel`, port 7792) został wchłonięty w ten panel i usunięty razem z kontenerem, obrazem i launcherowym oknem wyboru panelu. Co z niego przeszło i gdzie jest teraz:

| Było w panelu klasycznym | Jest teraz |
|---|---|
| Raty, respawn, trudność, autołowy, kanały, skrzynia startowa, zachowanie botów, polityka przedmiotów | [[ep:manage|Gra i serwer]], [[ep:respawns|Respawny]] |
| Arezzo, Seon-Hae, Smocze Kamienie i alchemia, szarfy, życie potworów, Dzieci Kwiaty, Wielkanoc | [[ep:world_modules|Moduły świata]] |
| Decyzje botów z wyjaśnieniami | [[ep:decisions_page|Decyzje botów]] (ten sam kod wyjaśnień) |
| Mapa na żywo z kartą bota | mapa świata botów na [[ep:dashboard|Pulpicie]], karta w [[ep:players|Postaciach]] |
| Eventy, legendy, gildie, sezon, akcje na postaci, ranga GM | [[ep:events|Planer eventów]], [[ep:legends_page|Legendy]], [[ep:guilds|Gildie]], [[ep:season|Sezon]], karta postaci |
| „Podgląd” postaci z okna GM w grze | adres `/players/<id>` prowadzi teraz do karty postaci w tym panelu |

Nie przeszło celowo: portal gracza (rejestracja, pobieranie klienta, gra w przeglądarce) i raporty awarii klienta z przeglądarki — w singleplayer nieużywane. Hasło panelu ustawia się w [[ep:manage_panel|Panel webowy]].

## Czego jeszcze brakuje (propozycje)

- Kopia bazy i jej przywracanie z panelu (dziś tylko launcher).
- Podgląd i edycja flag questów (np. `lexiw_horse.skin`) bez komendy `/setqf`.
- Edytor plików spawnu mapy (dodanie Metina albo bossa w nowym miejscu) — dziś tylko czas respawnu całej mapy.
- Raty osobno dla map albo pór dnia.
- Jeden klik „teleportuj mnie” w Timerach spawnów.
- Instalacja na VPS (installer\install.ps1, linux-port/tools/update.sh) wciąż zakłada dawny panel klasyczny — do przerobienia, gdyby serwer miał kiedyś trafić na VPS.
