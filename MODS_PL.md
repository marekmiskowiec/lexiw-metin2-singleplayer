# Metin2 Playerbots – paczka modyfikacji

Serwer Metin2 z botami (na bazie projektu **metin2-playerbots**, silnik mt2009,
wersja 2.2.0) z naszymi modyfikacjami. Instaluje się i uruchamia dokładnie tak
samo jak oficjalne wydanie: lokalnie, na Windowsie, w Docker Desktop.

## Instalacja

1. Zainstaluj **Docker Desktop** (https://www.docker.com/products/docker-desktop/)
   i uruchom go.
2. Rozpakuj całe archiwum, np. do `C:\Metin2Mod\`. Ścieżka bez polskich znaków
   i spacji jest najbezpieczniejsza.
3. Uruchom **`Metin2-Launcher-GUI.bat`**.
4. Kliknij **1. INSTALUJ / PRZYGOTUJ**, potem **2. GRAJ**.
   Pierwsze uruchomienie trwa długo (kilkanaście–kilkadziesiąt minut):
   Docker buduje serwer ze źródeł i tworzy bazę z botami.

Hasła (baza, panel) są losowane przy pierwszym starcie i zapisywane w
`linux-port\docker\.env` – tylko na Twoim komputerze.

## Klient

Do gry potrzebny jest **nasz klient** (z nowymi kostiumami, mountami, petami
i przedmiotami). Oficjalny klient nie pokaże nowych przedmiotów.

## Aktualizacje

Aktualizacje przychodzą **z repozytorium moda**
([zaxerrrr-dot/mt2009-sp-plus](https://github.com/zaxerrrr-dot/mt2009-sp-plus)),
nigdy z oficjalnego — oficjalna paczka nadpisałaby modyfikacje.

- **Windows:** w launcherze przycisk sprawdzania/instalacji aktualizacji, jak
  w oficjalnym wydaniu.
- **Linux / VPS:** z folderu serwera `sh linux-port/tools/update.sh`
  (`sh linux-port/tools/update.sh check` tylko pokazuje wersje).

Jak wydać nową wersję: [AKTUALIZACJE_MOD.md](AKTUALIZACJE_MOD.md).

## Co zmienia ta paczka

**Przedmioty i systemy**
- Auto Łowy pod klawiszem **K**: walka, skille, mikstury, przedmioty
  czasowe, wstawanie oraz filtrowane podnoszenie własnego łupu.
- Cor Draconis i wszystkie szarfy można przekazywać graczom oraz wystawiać
  w sklepach zwykłych i offline.
- Wadliwa rodzina Skrzydeł Władcy Śmierci 85101–85104 nie wypada już z
  Metinów, bossów ani szkatułek.
- Kostiumy, fryzury i nakładki na broń Gameforge 26.1.11 (ok. 2200 przedmiotów).
- Mounty z pieczęci (kostium wierzchowca, ok. 240 pieczęci, 129 modeli) z
  poprawioną prędkością ruchu; deski/chmury/łodzie pozwalają używać umiejętności.
- Proste pety (ok. 190), jeden naraz, bonusy z przedmiotu, powrót po relogu;
  pety oznaczone „(łup)” same podnoszą przedmioty i yang (tylko Twoje).
- Bonusy kostiumów: **Transformuj kostium** (70063, losuje 1–3 bonusy),
  **Zaczaruj kostium** (70064, zmienia wartości), **Transfer bonusów** (70065,
  u Kowala: przenosi bonusy z jednego kostiumu na drugi tego samego rodzaju).
  Do kupienia u Handlarki Różności.
- System Smoczych Kamieni (alchemia), Szarfy (kombinacja i przechwytywanie
  bonusów u Uriela), zestaw Władcy Śmierci, kostiumy Nosiciela Światła i Diademy,
  sklep kostiumów u Ah-Yu, Magma Manni.
- ItemShop w grze i ItemShop w przeglądarce z nowymi kategoriami (Fryzury +,
  Kostiumy, Nakładki na broń, Pety, Mounty); w przeglądarkowym przedmioty są
  filtrowane pod klasę i płeć postaci, z której otwarto sklep.
- Kosz na śmieci, podgląd dropu potworów, przełącznik eventu wielkanocnego
  w panelu.
- Wyszukiwarka sklepów pozwala kliknąć konkretny przedmiot i znaleźć tylko
  sklepy offline oraz stragany botów oferujące dokładnie ten vnum/socket.

**Boty**
- Płynniejsza praca serwera przy wielu botach (budżet czasu na turę).
- Boty trzymają zapas mikstur 27101/27104 zamiast wystawiać je na straganach.
- „Emerytura” botów z panelu (wymiana starych botów na nowe).
- Cor Draconis i szarfy: boty zdobywają je z metinów i bossów z własnymi,
  niższymi szansami (Cor 5%, szarfa 3%; gracze bez zmian) – prosto do plecaka;
  Cora z ziemi bot nie podniesie, więc nie zabiera Corów graczy. Nie otwierają
  Corów i nie zakładają ani nie łączą szarf. Wystawiają je na swoich sklepach offline dla
  graczy (Cor 500 000, szarfa 700 000 za sztukę przy kursie yang 100%; cena
  rośnie z inflacją i szybką sprzedażą, a spada o 10% co 2 h bez sprzedaży,
  najwyżej o 50%). Każdy rodzaj może być naraz w najwyżej 20% sklepów botów,
  do 3 pozycji na sklep. Co nie sprzeda się przez 12 h, bot zdejmuje i sprzedaje
  u handlarki.

**Inne**
- Serwer wpuszcza klienta bez względu na jego wersję (brak blokady wersji).

## Kopia i powrót

Launcher ma przycisk **KOPIA ŚWIATA** – zapisuje cały świat do pliku zip.
Przycisk „Zatrzymaj i zapisz” nigdy nie usuwa postaci ani postępu botów.

## Licencja

Narzędzia projektu metin2-playerbots są na licencji MIT (plik `LICENSE`).
Kod serwera gry, dane gry i klient nie należą do autorów projektu ani do nas –
przeczytaj `NOTICE.md` przed dalszym udostępnianiem.
