---
title: Ulepszanie u kowala
summary: Jak czytać tabelę ulepszeń na stronie przedmiotu: szansa, koszt, materiały, wyniki botów i „bez porażki”.
order: 40
---

Broń i zbroje ulepsza się u kowala od **+0 do +9**. Każdy plus to osobny przedmiot z własnym numerem VNUM (np. Miecz+0, Miecz+1, …), więc wiki pokazuje całą drogę na stronie dowolnego z nich. Przykład: [[item:10]].

## Co jest w tabeli

| Kolumna | Znaczenie |
|---|---|
| **Poziom** | przedmiot na danym plusie |
| **Statystyki** | co daje na tym plusie (np. wartość ataku); jeśli bonus własny przedmiotu zmienia się z plusem (buty: szybkość ruchu), jest dopisany do wiersza |
| **Szansa** | szansa, że ulepszenie **na następny plus** się uda |
| **Koszt** | opłata dla kowala w yang za tę próbę |
| **Materiały** | przedmioty potrzebne do tej próby (jeśli „—”, to nie trzeba żadnych) |
| **U botów** | ile razy boty próbowały i jaki procent się udał, z logu ulepszeń serwera (pokazane od 20 prób) |
| **Bez porażki od +0** | szansa, że dojdziesz do tego plusa od +0 **bez ani jednej porażki** |

Szansa, koszt i materiały pochodzą z tabeli ulepszeń serwera, więc jeśli zmienisz ją w grze, wiki to pokaże. Zwoje (np. Błogosławieństwa) działają osobno i nie są w tabeli.

## Przykład: Miecz

Droga od +0 do +9 według tabeli serwera:

| Ulepszenie | Szansa | Koszt (yang) | Materiały | Bez porażki do początku kroku |
|---|---|---|---|---|
| +0 → +1 | 90% | 400 | — | 100% (start) |
| +1 → +2 | 90% | 800 | — | 90% |
| +2 → +3 | 90% | 1 600 | — | 81% |
| +3 → +4 | 90% | 3 300 | — | 72,9% |
| +4 → +5 | 80% | 6 600 | — | 65,6% |
| +5 → +6 | 60% | 13 300 | — | 52,5% |
| +6 → +7 | 50% | 20 000 | [[item:30028]] | 31,5% |
| +7 → +8 | 40% | 30 000 | [[item:30004]] | 15,7% |
| +8 → +9 | 30% | 50 000 | [[item:30003]] | 6,3% |

Ostatnia kolumna jest iloczynem szans kolejnych kroków, więc **+9 bez żadnej porażki** to tylko **1,9%**. Dlatego pełne +9 jest rzadkie i dlatego ulepszenia +8 i +9 są ogłaszane w [[page:wiadomosci|wiadomościach ze świata]].

## Jak czytać „U botów”

Boty ulepszają naprawdę, a wyniki trafiają do logu. Kolumna „U botów” to rzeczywista skuteczność: przy 90% oczekuj okolic 90%, a duże odchylenia przy małej liczbie prób (np. 44 próby) są normalne. Gdy bot-wynik odbiega od tabeli na stałe, warto sprawdzić, czy szansa w tabeli serwera nie została zmieniona.

## Ile to kosztuje

Suma kosztów kolejnych kroków i materiały dają najtańszą drogę, **jeśli wszystko pójdzie dobrze**. Kolumna „Bez porażki” pokazuje, jak rzadko tak jest: do +8 bez potknięcia dochodzi tylko 6% prób. Co dokładnie dzieje się z przedmiotem po nieudanej próbie, zależy od ustawień serwera i od użytych zwojów, więc wiki tego nie obiecuje.

Ceny gotowych ulepszonych przedmiotów można sprawdzić w [[page:gielda|Giełdzie]] (filtr „Plus”), a poradnik [[guide:jak-dziala-gielda]] tłumaczy, jak porównać cenę z medianą.
