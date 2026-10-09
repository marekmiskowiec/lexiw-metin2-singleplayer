# Aktualizacje paczki moda

Launcher Windows, `linux-port/tools/update.sh` (VPS), panel admina (7788) i
updater Seban Panelu czytają aktualizacje **tylko** z repozytorium moda:

| Co | Skąd |
|---|---|
| Manifest (launcher, `update.sh`, updater Seban) | `https://raw.githubusercontent.com/zaxerrrr-dot/mt2009-sp-plus/main/update-manifest-mt2009.json` (najpierw przez GitHub API, CDN raw jako zapas) |
| Powiadomienie w panelu admina (7788) | `VERSION` i `CHANGELOG.md` z gałęzi `main` tego repozytorium |
| Powiadomienie w Seban Panelu (7790) | najnowszy GitHub Release tego repozytorium (tag `vX.Y.Z`) |
| Paczka ZIP | adres `url` z manifestu, zwykle załącznik GitHub Release |
| Serwer zapasowy (gdy GitHub nie odpowiada) | `http://141.94.100.53/aktualizacje/` — te same nazwy plików, patrz niżej |

Adres z oficjalnego repozytorium (`TieruYT/metin2-playerbots`) jest odrzucany.
Pusty `manifestUrl` zapisany w `.m2launcher.json` z czasów, gdy aktualizacje
były wyłączone, launcher sam zamienia na adres moda.

> [!IMPORTANT]
> Repozytorium musi być **publiczne**. GitHub nie wyda plików z prywatnego
> repozytorium bez logowania, więc launcher i `update.sh` dostałyby 404.

## Numer wersji

Aktualizatory porównują `VERSION` z dysku z `server.version` z manifestu
**dokładnie** (równe = aktualne), a panel porównuje je jako `X.Y.Z`. Dlatego
każde wydanie moda podnosi `VERSION` (np. `2.2.0` → `2.2.1` → `2.2.2`).
`MOD_VERSION` (`mod-3`, `mod-4`, …) to tylko etykieta — możesz ją podbijać
równolegle.

## Wydanie nowej wersji serwera (Windows)

W **pełnym folderze serwera moda** (tym z `VERSION`, `MOD_VERSION`,
`linux-port\`, silnikiem w `linux-port\docker\game\src\server\` itd.):

1. Wprowadź zmiany. Plik, którego nie ma w
   `launcher\server-update-files.mod.txt`, **nie trafi do graczy** — nowe pliki
   dopisz w sekcji „the mod's own files”.
2. Nałóż zmiany silnika MT2009 Plus (jak `playerbotify.py` u Tieru – tylko
   u Ciebie, przy wydaniu; gracze niczego nie łatają, dostają gotowe pliki
   `.cpp` w paczce):

   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\port\Apply-MT2009PlusEngine.ps1 -ServerRoot .
   ```

   Drugie uruchomienie ma wypisać `0 file(s) changed`. Potem zbuduj i sprawdź
   serwer w grze (`start-server.ps1`). Nowa zmiana silnika = nowy krok w tym
   skrypcie (z własnym znacznikiem `MT2009_PLUS_...`) i plik silnika na
   liście `launcher\server-update-files.mod.txt`.
3. Ustaw nową wersję w `VERSION` (np. `2.2.1`), podbij `MOD_VERSION` i dopisz
   na górze `CHANGELOG.md` sekcję `## 2.2.1 — RRRR-MM-DD` (panel pokazuje ją
   graczom przed aktualizacją).
4. Zbuduj paczkę:

   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\New-M2UpdatePackage.ps1 `
     -Type server -Version 2.2.1 -SourceRoot . `
     -FileList launcher\server-update-files.mod.txt `
     -OutputDirectory ..\wydania `
     -DownloadUrl https://github.com/zaxerrrr-dot/mt2009-sp-plus/releases/download/v2.2.1/metin2-server-update-2.2.1.zip
   ```

   Skrypt odmówi, jeśli `VERSION` nie zgadza się z `-Version` albo plik
   silnika na liście nie ma zmian MT2009 Plus (punkt 2). Wypisze SHA-256
   i zapisze `server-manifest-fragment-2.2.1.json`.
5. Na GitHubie utwórz **Release** z tagiem `v2.2.1` i dołącz
   `metin2-server-update-2.2.1.zip`.
6. **Dopiero potem** w repozytorium na `main` zaktualizuj
   `update-manifest-mt2009.json` (blok `server` z fragmentu), `VERSION`,
   `MOD_VERSION`, `CHANGELOG.md` i zmienione pliki źródłowe; commit i push.
   Od tej chwili launchery i serwery widzą nową wersję (API GitHuba po ok.
   minucie, CDN raw po do 5 minutach).

Przykładowy manifest:

```json
{
  "schema": 1,
  "channel": "stable",
  "publishedAt": "2026-09-30T00:00:00Z",
  "server": {
    "version": "2.2.1",
    "url": "https://github.com/zaxerrrr-dot/mt2009-sp-plus/releases/download/v2.2.1/metin2-server-update-2.2.1.zip",
    "sha256": "SKOPIUJ_Z_FRAGMENTU"
  }
}
```

> [!IMPORTANT]
> Paczka klienta musi być **pełna** (zbiorcza): launcher instaluje tylko
> najnowszą paczkę z manifestu, nie po kolei każdą. Gracz, który przeskoczył
> z 2.0.4 na 2.0.8, nie dostał `metin2client.exe` z 2.0.6/2.0.7 (klient
> 2.0.10 to naprawił). Każda paczka klienta zawiera więc wszystkie pliki,
> które mod zmienia w kliencie, także te niezmienione od poprzedniej wersji.

Klienta aktualizuje się tak samo: `-Type client`, własna lista plików
(wzór: `launcher\client-update-files.example.txt`) i blok `client` w
manifeście. Bez bloku `client` launcher klienta nie rusza.

Dopóki manifest nie ma bloku `server`, launcher mówi, że w kanale nie ma
wersji, i niczego nie zmienia.

## Serwer zapasowy aktualizacji

Gdy GitHub nie odpowiada (błąd sieci, kod inny niż 200, odpowiedź, która nie
jest JSON-em), launcher, aktualizator klienta, `update.sh`, `instaluj-vps.sh`
i updater Seban biorą pliki z `http://141.94.100.53/aktualizacje/` — pod tymi
samymi nazwami: `update-manifest-mt2009.json`, `client-files.json` i nazwa
pliku z adresu `url` w manifeście (`metin2-server-update-<V>.zip`,
`metin2-client-update-<V>.zip`). Paczka z serwera zapasowego przechodzi tę
samą kontrolę SHA-256 (i rozmiaru w aktualizatorze klienta). W logu pojawia
się „GitHub niedostępny - pobieram z serwera zapasowego”. Opcjonalne pole
manifestu `"mirrors": ["https://.../"]` dodaje kolejne adresy (sprawdzane
przed wbudowanym).

**Przy każdym wydaniu**, po wypchnięciu manifestu i `client-files.json`,
na serwerze (vps1) z tego repozytorium:

```sh
sh tools/publish-update-mirror.sh ../wydania/metin2-server-update-2.2.1.zip ../wydania/metin2-client-update-2.0.39.zip
sh tools/publish-update-mirror.sh ../wydania/metin2-server-update-2.2.1.zip -   # bez nowego klienta
```

Skrypt sprawdza SHA-256 paczek z manifestem, kopiuje je, potem
`client-files.json`, a manifest na końcu (każdy plik przez `.tmp` + `mv`) do
`/opt/metin2/dist/aktualizacje`, i zostawia tylko dwie najnowsze paczki
serwera i dwie klienta.

## Na serwerze Linux / VPS

```sh
cd /opt/metin2            # folder serwera (VERSION, linux-port/)
sh linux-port/tools/update.sh check   # co jest zainstalowane, co wydane
sh linux-port/tools/update.sh         # pobierz, sprawdź SHA-256, rozpakuj, zbuduj
```

Przycisk aktualizacji w panelu działa po włączeniu `M2_UPDATE_APPLY=1` w
`.env` i uruchomieniu updatera:
`docker compose --profile update up -d updater` (patrz `UPDATING.md`).
Inne repozytorium lub gałąź: zmienne `M2_UPDATE_REPO` i `M2_UPDATE_BRANCH`.
Serwer zapasowy: `M2_UPDATE_MIRROR` (adresy oddzielone spacją; puste wyłącza),
czas czekania na GitHub: `M2_UPDATE_GITHUB_TIMEOUT` (domyślnie 15 s).

Baza danych (postacie, boty, przedmioty) nie jest ruszana przez żadną z tych
dróg.

## Istniejące instalacje

Instalacje z wersji moda, w której aktualizacje były wyłączone, mają stary
launcher i stary `update.sh`, które niczego nie pobiorą. Taka instalacja
musi **raz** dostać nowe pliki ręcznie (albo pełną nową paczkę):

- Windows: `launcher\Metin2Launcher.psm1`
- Linux: `linux-port/tools/update.sh`
- panele (powiadomienia): `files/admin_panel.py`,
  `linux-port/docker/panel/app/admin_panel.py`,
  `linux-port/docker/seban-panel/app.py`,
  `linux-port/docker/seban-panel/updater/update-mt2009.py`

Każda kolejna aktualizacja przychodzi już sama.

## Przenoszenie nowej wersji od Tieru

Nie kopiuj całego drzewa oficjalnego wydania na repozytorium. Wydanie 2.2.3
tak zrobiło i cofnęło wygląd launchera oraz kanał aktualizacji (naprawione
w 2.2.5). Poniższe pliki mają zmiany MT2009 Plus — scalaj je ręcznie
(weź poprawki Tieru, zostaw nasze fragmenty), a nie nadpisuj:

| Plik | Co jest nasze |
|---|---|
| `Metin2-Launcher-GUI.Layout.ps1` | tytuł MT2009 PLUS, podtytuł, metin2sp.pl, przycisk Discord, stopka |
| `Metin2-Launcher-GUI.Background.png` | tło MT2009 PLUS |
| `launcher/Metin2Launcher.psm1` | kanał `zaxerrrr-dot/mt2009-sp-plus`, odrzucanie kanału Tieru, naprawa pustego `manifestUrl` |
| `launcher/launcher.config.example.json` | adres `update-manifest-mt2009.json` moda |
| `files/admin_panel.py` i `linux-port/docker/panel/app/admin_panel.py` | sprawdzanie wersji w repozytorium moda (`/VERSION`) |
| `linux-port/docker/seban-panel/app.py` | sprawdzanie wydań repozytorium moda |
| `linux-port/docker/seban-panel/updater/update-mt2009.py` | manifest moda, odrzucanie Tieru |
| `linux-port/tools/update.sh` | domyślne repozytorium moda, odrzucanie Tieru |
| `linux-port/docker/mariadb/playerbot/apply.sh` | pętla wgrywająca `mod/*.sql` (oferty ItemShopu: kostiumy, fryzury, nakładki, pety, mounty) — bez niej nowe światy mają pusty ItemShop |
| `linux-port/docker/mariadb/playerbot/mod/*.sql` | oferty ItemShopu w grze i w przeglądarce |
| `tools/New-M2UpdatePackage.ps1` | sprawdzanie `VERSION`, BOM w skryptach PowerShell i zmian silnika MT2009 Plus |
| pliki silnika w `linux-port/docker/game/src/server/game/src/` (m.in. `item_manager.cpp`, `char_item.cpp`, `cmd.cpp`, `char_affect.cpp`, `MountSystem.cpp`, `input_main.cpp`) | po wzięciu wersji Tieru uruchom ponownie `tools/port/Apply-MT2009PlusEngine.ps1` (albo bliźniaki `server-patches/*/apply_*.py`) – każda łatka ma swój znacznik `MT2009_PLUS_...` |
| `README.md`, `README_EN.md`, `MODS_PL.md`, `AKTUALIZACJE_MOD.md` | opis moda |
| `update-manifest-mt2009.json`, `VERSION`, `MOD_VERSION`, `CHANGELOG.md` | wersje i kanał moda |

**Baza Tieru: 2.2.20** (scalone 26 września 2026, razem z 2.2.19; wcześniej 2.2.18 i 2.2.15, 25 września, 2.2.9, 24 września). Scalaj trójstronnie
(`git merge-file`): nasz plik, plik Tieru z wersji, na której nasz jest
oparty, i nowa wersja Tieru. Nie zakładaj jednej bazy dla wszystkich plików
– przed tym scaleniem źródła botów były na Tieru 2.2.3, choć CHANGELOG
mówił o 2.2.6, a pojedyncze pliki na 2.2.5–2.2.8. Dla każdego pliku bazą
jest ta wersja Tieru, od której nasz plik różni się najmniej (albo ta, z
którą jest identyczny – wtedy po prostu weź nową wersję Tieru).

`files/admin_panel.py` ma być identyczny z `linux-port/docker/panel/app/admin_panel.py`:
launcher przy starcie kopiuje ten pierwszy na drugi (`start-server.ps1`,
`update.sh`), więc zmiana tylko w panelu znika u graczy.

Po scaleniu sprawdź:

```sh
grep -rn "TieruYT\|metin2singleplayer\|buycoffee" --include=*.ps1 --include=*.psm1 --include=*.py --include=*.sh .
```

Jedyne dozwolone trafienia to blokady kanału Tieru (`TieruYT/metin2-playerbots`
w warunkach odrzucających) i stary instalator `installer/`.

Każdy plik `.ps1` / `.psm1` musi być zapisany jako **UTF-8 z BOM** — bez
tego Windows PowerShell nie wczyta polskich znaków i launcher nie wstanie.
Pakowacz odmówi zbudowania paczki z takim plikiem.

## Ogłoszenie nowej wersji na Discordzie

Gdy push na `main` zmienia `VERSION`, GitHub Actions
(`.github/workflows/discord-release.yml`) wysyła na kanał Discorda wpis tej
wersji z `CHANGELOG.md` — tytuł, opis, sekcje `###` jako pogrubione nagłówki,
punkty listy. Długi wpis dzieli się na kilka części.

**Jednorazowo:** na Discordzie *Ustawienia kanału → Integracje → Webhooki →
Nowy webhook* → skopiuj adres. Na GitHubie *Settings → Secrets and variables →
Actions → New repository secret*: nazwa `DISCORD_WEBHOOK_URL`, wartość = adres
webhooka.

**Przy każdym wydaniu** wystarczy dobrze napisany wpis w `CHANGELOG.md`.
Tytuł ogłoszenia można podać w nagłówku, po dacie:

```markdown
## 2.2.7 — 2026-09-24 — Nowe kostiumy, szybsze Auto Łowy

Serwer 2.2.7 i klient 2.0.7. Zaktualizuj oba w launcherze.

### Nowe kostiumy
- ...
```

Aktualizację samego klienta bot ogłasza, gdy zmienia się `CLIENT_VERSION`;
jej wpis ma nagłówek `## Klient 2.0.8 — 2026-09-25 — Tytuł`.

Bez tytułu w nagłówku bot bierze nagłówki `###`, a gdy ich nie ma, pierwsze
zdanie wpisu. Wysłać (albo wysłać ponownie) dowolną wersję można ręcznie:
*Actions → Discord - nowa wersja → Run workflow*, wpisać numer i wybrać
`serwer` albo `klient`. Podgląd bez
wysyłania: `python3 .github/scripts/discord_release.py 2.2.7 --dry-run`.
