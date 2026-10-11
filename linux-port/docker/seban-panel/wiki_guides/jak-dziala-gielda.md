---
title: Jak działa Giełda
summary: Skąd są oferty, co znaczą „za sztukę”, „vs rynek” i „okazja” oraz jak szukać i porównywać ceny.
order: 50
---

[[page:gielda|Giełda]] zbiera w jednym miejscu **wszystkie oferty ze sklepów offline** (straganów) świata, głównie botów, a także graczy. Pokazuje stan rynku, nie jest sklepem: niczego tu nie kupisz, widzisz tylko, kto co wystawił i za ile.

## Skąd się biorą dane

- Collector panelu robi migawkę rynku **co 5 minut** i buduje z niej płaską, indeksowaną tabelę ofert. Dlatego strona otwiera się szybko, ale pokazuje stan **sprzed najwyżej kilku minut**. Godzina zrzutu jest u góry strony.
- Liczą się tylko oferty, które da się teraz kupić: stragan, któremu skończył się czas, trzyma towar, ale nikomu go nie sprzeda, więc go pomijamy. Sprzedane przed chwilą pozycje też znikają.
- Wiek oferty liczy się od chwili, gdy panel zobaczył ją po raz pierwszy **z obecną ceną**. Zmiana ceny zeruje wiek.

## Kolumny listy

| Kolumna | Znaczenie |
|---|---|
| **Przedmiot** | ikona (najedź, żeby zobaczyć opis jak w grze), nazwa i kolorowe pigułki bonusów — patrz [[guide:co-znacza-bonusy]] |
| **Ilość** | ile sztuk w stosie |
| **Za sztukę** | cena stosu podzielona przez ilość; to ona jest porównywana |
| **Cena stosu** | tyle kosztuje cała oferta |
| **vs rynek** | różnica od mediany (patrz niżej) |
| **Wiek** | od kiedy oferta wisi w tej cenie |
| **Sprzedawca** | kto, na jakiej mapie i w jakim sklepie; nazwa prowadzi do jego profilu |

## „vs rynek” i okazje

**Mediana** to środkowa cena za sztukę tego przedmiotu wśród wszystkich ofert. Liczy się ją tylko wtedy, gdy ofert jest **co najmniej 3**; inaczej w kolumnie jest „—”.

Porównuje się **podobne z podobnym**: dla broni i zbroi mediana jest liczona wśród ofert z **taką samą liczbą dodanych bonusów**, a dla ksiąg umiejętności — wśród ksiąg **tej samej umiejętności**. Miecz z czterema dodanymi bonusami nie jest oceniany względem gołego miecza.

Znak przy liczbie mówi, gdzie leży cena: **−42%** to 42% **taniej** od mediany, **+30%** to 30% drożej. Zielone są oferty tańsze o co najmniej 15%, czerwone — droższe o co najmniej 15%. Przy tanich ofertach jest też „+kwota”: ile yang zyskałbyś, odsprzedając cały stos po medianie.

Karta **🔥 Okazje** pokazuje oferty co najmniej 30% poniżej mediany, od największego potencjalnego zysku. To tylko podpowiedź: tania oferta bywa pomyłką sprzedawcy, ale bywa też po prostu dobrą ceną.

## Szukanie i filtry

Po lewej są kategorie z liczbą ofert (broń według rodzaju, zbroje i biżuteria, księgi umiejętności, reszta). Nad listą są filtry, które łączą się ze sobą:

| Filtr | Jak działa |
|---|---|
| Nazwa lub VNUM | fragment nazwy albo numer przedmiotu |
| Klasa | tylko przedmioty, które ta klasa może nosić |
| Poziom | wymagany poziom przedmiotu, od–do |
| Plus | ulepszenie +0…+9, od–do |
| Bonus + wartość od | przedmioty z danym bonusem o co najmniej takiej wartości |
| Cena za sztukę | od–do |
| Taniej od mediany o (%) | tylko oferty o tyle tańsze od mediany |
| Sortuj | cena za sztukę (rosnąco i malejąco), cena stosu, największa okazja (zysk), największa zniżka %, najnowsze, najdłużej wystawione |

Każdy zestaw filtrów ma własny adres, więc można go zapisać w zakładkach.

## Strona ceny przedmiotu

Klik w nazwę przedmiotu otwiera jego stronę ceny: **najtaniej, mediana, najdrożej i liczba ofert**, wykres ceny za sztukę w czasie (jeden punkt na godzinę, więc wykres pojawia się po dwóch godzinach historii), porównanie wariantów oraz 20 najtańszych ofert. Obok jest link do opisu w wiki.

## Obrót i sprzedaże oraz Indeks cen

- **🧾 Obrót i sprzedaże:** sklepy na mapach, wartość rynku w czasie, to, co **naprawdę się sprzedaje**, i ostatnie transakcje. Silnik zapisuje tylko stronę sprzedawcy, więc nie wiadomo, kto kupił.
- **📈 Indeks cen i obieg yang:** jak zmieniają się ceny. Indeks liczy się z godzinnych median przedmiotów, które mają teraz co najmniej 10 ofert; **100** to pierwsza zmierzona godzina. Obok jest lista, co najbardziej podrożało i potaniało, oraz wykres yang w obiegu.

## Czego Giełda nie robi

- Nie wie, **ile ktoś by zapłacił**, tylko ile żądają sprzedawcy. Sprzedaże widać dopiero w „Obrocie i sprzedażach”.
- Nie ostrzega o ofertach i nie kupuje za Ciebie.
- Wiki ([[page:wiki]]) mówi, czym przedmiot jest i skąd się bierze; Giełda mówi, ile kosztuje **teraz**. To dwie osobne rzeczy.
