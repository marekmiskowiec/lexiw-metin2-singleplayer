"""Lexiw: the wiki page about the bots' personalities.

Texts are written from the game core's own sources (playerbot_types.h,
playerbot_persona_rules.h, playerbot_rare_persona.h of the playerbot overlay):
what each personality does, what makes a bot take it up and for how long.
The ids are the ones the core writes to the status file (BOT_PERSONALITIES and
BOT_PERSONAS in app.py), so the page can show how many bots carry each one now.

Two layers, which the panel shows as "Osobowość" and "Persona":
  * the character - drawn by pid at login, it tilts the odds for good;
  * the persona - Iwakura's "system osobowości v2.0", recomputed from what the
    bot is doing right now (a full bag makes a trader, a Metin in sight a stone
    breaker, ...).
"""

# id (BOT_PERSONALITIES), name is taken from app.py so the two never drift.
CHARACTERS = {
    0: {"summary": "Domyślny charakter: równo poluje, wraca do miasta i wyrusza na dalsze mapy.",
        "does": ["Wyrusza na dalekie mapy (frontier) średnio w 5 przypadkach na 8 — w pozostałych zostaje przy wioskach.",
                 "Nie ma specjalnego zajęcia, w którym byłby lepszy od innych."]},
    1: {"summary": "Lubi Metiny. Dalekie mapy mają własne Metiny, więc często tam bywa.",
        "does": ["Wyrusza na dalekie mapy w 6 przypadkach na 8.",
                 "To tylko skłonność charakteru. Bot, który akurat walczy z Metinem, ma osobno personę „Pogromca metinów” (niżej)."]},
    2: {"summary": "Trzyma się grup i miejsc, gdzie tworzą się drużyny.",
        "does": ["Wyrusza daleko tylko w 4 przypadkach na 8, bo zostaje blisko miejsca, gdzie zbierają się drużyny.",
                 "Ma większą szansę niż inne charaktery na personę Towarzysza (osobny, niższy próg losowania)."]},
    3: {"summary": "Zbiera i ulepsza ekwipunek, w tym bronie 30 poziomu z Doliny Orków.",
        "does": ["Wyrusza na dalekie mapy w 6 przypadkach na 8 — Dolina Orków to miejsce, gdzie padają bronie 30 poziomu.",
                 "Poza tym gra jak zwykły bot — różnicę robi ta dalsza wyprawa."]},
    4: {"summary": "Ostrożny: chroni to, co zdobył, i szybko wraca do miasta.",
        "does": ["Wyrusza na dalekie mapy tylko w 2 przypadkach na 8, a wyprawy skraca o połowę.",
                 "Gdy mikstur zostaje mało, wraca do miasta, zanim zrobi się groźnie."]},
    5: {"summary": "Kupiec: prowadzi stragan, bo taki ma charakter, a nie przy okazji.",
        "does": ["Wszyscy inni za czymś gonią (poziom, koń, lepsza broń). Handlarz zakłada stragan i handluje.",
                 "Nie rusza na wyprawę po medale konia, bo to byłoby dokładnie to dążenie, którego ten charakter unika."]},
    6: {"summary": "Odkrywca: prawie zawsze na dalekich mapach.",
        "does": ["Wyrusza daleko w 7 przypadkach na 8, a pobyty trwają dwa razy dłużej niż u innych.",
                 "Rzadko się go widzi w wioskach."]},
    7: {"summary": "Dropek: ma stały cel — Metiny — i zostaje przy nim, zamiast dalej awansować.",
        "does": ["Zatrzymuje się na 40. poziomie (blokada expa) i dalej zbija Metiny dla rynku.",
                 "Zatrzymuje księgi umiejętności, które wypadają z kamieni, zamiast je sprzedawać."]},
    8: {"summary": "Dropek z M3: zostaje na mapie gildii (Waryong) po bronie 30 poziomu.",
        "does": ["Blokada expa na 30. poziomie. Zostaje tam, dopóki mapa ma dla niego sens.",
                 "Broń 30 poziomu, którą zdobędzie, to dla niego towar na sprzedaż, a nie powód, żeby odejść."]},
    9: {"summary": "Dropek z M2: obozuje na Bestiach z Bokjung (druga wioska) po ich bronie.",
        "does": ["Blokada expa na 36. poziomie. Poluje na bestie w drugiej wiosce."]},
    10: {"summary": "Dropek medali: farmi medale konia w Małpim Lochu, więcej niż potrzebuje własny koń.",
         "does": ["Blokada expa na 33. poziomie. Medale sprzedaje przez własne stoisko."]},
    11: {"summary": "Dropek surowców: zbiera materiały na budynki gildii i sprzedaje je na straganach.",
         "does": ["Farmi Kamień Węglowy, Pień albo Dyktę — zależnie od swojego miejsca.",
                  "Blokada expa zależy od miejsca farmienia."]},
    12: {"summary": "Dropek broni 30 poziomu: pierwsza wyspa w Dolinie Orków.",
         "does": ["Blokada expa na 21. poziomie. Czeka na respawn bossa wyspy, a między nimi poluje wokół miejsca respawnu.",
                  "Walczy z potworami o więcej poziomów wyżej niż zwykłe boty."]},
}

# persona id -> text. kind: base | rare | gambler
PERSONAS = {
    0: {"kind": "base", "summary": "Zwykły stan: bot expi tam, gdzie jego poziom ma sens, i trzyma się swojej blokady poziomu.",
        "how": ["To persona „nic specjalnego się nie dzieje”. Bot, który nie handluje, nie ulepsza i nie walczy z Metinem, to Grinder (albo Zdobywca, jeśli już awansował).",
                "Grinder zatrzymuje się na poziomie z tabeli „Poziomy blokady” (niżej) i czeka, aż zdobędzie wymagany sprzęt (Prawo Awansu).",
                "13% botów nigdy się nie zatrzymuje na takich poziomach; kolejne 10% może w pewnym momencie w ogóle przestać grindować (po spełnieniu Prawa, 33% szans na każdym tierze)."]},
    1: {"kind": "base", "summary": "Grinder, który spełnił Prawo Awansu i może iść do trudniejszego tieru.",
        "how": ["Po zdobyciu wymaganego sprzętu bot przestaje być Grinderem i idzie dalej.",
                "Jeśli ginie 3 razy od potworów w pół godziny, wraca do grindowania i jest wstrzymany na poziomie, na którym zginął, aż jego sprzęt spełni Prawo dla tego poziomu."]},
    2: {"kind": "base", "summary": "Handlarz: ma pełny plecak i handluje — stragan, sklepy, rynek.",
        "how": ["Zajmuje bota, gdy sygnał „handluję” jest aktywny (np. pełny plecak do sprzedania).",
                "Handel jest w kolejce ważności za walką Metinów i ulepszaniem, a przed zwykłym grindem."]},
    3: {"kind": "base", "summary": "Dawny Hazardzista: ulepsza przedmioty na kowadle na ryzyko. Został zastąpiony czterema rzadkimi hazardzistami (niżej).",
        "how": ["Każdy przedmiot dostaje własny cel: +7 w 6 przypadkach na 10, +8 w 3, +9 w 1.",
                "Do +7 używa zwykłego kowala, od +8 Zwoju Błogosławieństwa. Nieudany zwój cofa przedmiot o stopień; potem wraca na +7 i jest sprzedawany jak jest.",
                "Przeznacza na to 40% sakiewki. Autor systemu uznał tę personę za źle zbalansowaną i zastąpił ją rzadkimi hazardzistami, więc w grze zobaczysz ją rzadko."]},
    4: {"kind": "base", "summary": "Perfekcjonista: ciężka sakiewka idzie na ulepszanie sprzętu i księgi umiejętności.",
        "how": ["50% sakiewki na ulepszanie ekwipunku, 50% na księgi umiejętności.",
                "Jest wyżej w kolejce niż Handlarz, a niżej niż hazard."]},
    5: {"kind": "base", "summary": "Pogromca metinów: Metin w zasięgu wzroku „zajmuje” bota.",
        "how": ["Wchodzi w grę tylko, gdy Metin jest nie więcej niż 10 poziomów wyżej lub niżej od bota.",
                "Gdy 3 boty z jego królestwa już biją ten kamień, wraca do tego, co robił.",
                "Gdy ma mniej niż 35% życia, odwraca się przeciw potworom kamienia, a potem wraca do niego.",
                "Metin, który zabił bota więcej niż 6 razy, zostaje porzucony.",
                "To krótki stan sytuacyjny. Dłuższą wyprawę po Metiny prowadzi rzadki Metinolog (niżej)."]},
    6: {"kind": "base", "summary": "Górnik: bot z kilofem przy żyle rudy.",
        "how": ["Zajmuje bota, gdy kopie rudę (czynność „Kopie rudę”) — kilof trzyma wtedy w ręce.",
                "Wyprzedza zielarza, pogromcę, hazard, perfekcjonistę i handlarza w kolejce ważności."]},
    7: {"kind": "base", "summary": "Rybak: bot nad wodą z wędką.",
        "how": ["Szansa wyboru zależy od nastroju: Słaby 75‰ w oknie, Normalny 4‰, Bardzo dobry 0 (stąd rybacy to często pechowcy).",
                "Suwak łowienia w ustawieniach zwiększa udział rybaków, ale nigdy ponad 90% botów rdzenia.",
                "Po 5 śmierciach z rąk graczy na tym samym miejscu w kwadrans bot kapituluje i od 30. poziomu (poza drużyną) ma 8% szans na godzinę nad wodą."]},
    8: {"kind": "base", "summary": "Najemnik: silniejszy bot wynajęty za yang przez słabszego, który ciągle ginie.",
        "how": ["Bot, który ginie od potworów 3 razy w pół godziny, może wynająć silniejszego bota z tego samego królestwa na tej samej mapie. Ten silniejszy to Najemnik, a płacący ma w tym czasie personę Towarzysza.",
                "Godzina kosztuje 250 000 yang (przez krzywą yanga), płatne z góry. Najemnik musi mieć co najmniej 3 poziomy więcej i znacznie lepszy sprzęt (min. +25% mocy, nie mniej niż +30).",
                "Kontrakt kończy się po godzinie, jeśli klient dostał, po co przyszedł (3 poziomy, pełny plecak albo lepszy sprzęt), albo nie stać go na kolejną godzinę."]},
    9: {"kind": "base", "summary": "Towarzysz: bot w drużynie.",
        "how": ["Suwak „drużyny” w ustawieniach decyduje, jaka część botów gra w grupach. Szansa jest losowana na nowo co fazę (45–90 minut solo), więc każdy bot bywa towarzyszem przez jakiś czas.",
                "Szaman ma na to znacznie większą szansę (losowanie ścięte do 35%).",
                "W drużynie bot gra z nastrojem Normalnym, niezależnie od faktycznego."]},
    10: {"kind": "rare", "summary": "Metinolog: długa wyprawa po Metiny.",
         "how": ["120–250 minut polowania na kamienie metin: patrol, łup i wycieczki do miasta.",
                 "Wymaga konia na 11 poziomie i broni +7.",
                 "Maksymalnie 1 bot na 300 spełniających warunki (co najmniej 1 w świecie). Nie ma przerwy między kolejnymi.",
                 "Nie wchodzi do Pająków (nie ma tam kamieni); zamiast nich idzie na Górę Sohan."]},
    11: {"kind": "rare", "summary": "Nałogowiec: bardzo długa sesja hazardu przy kowadle.",
         "how": ["Do 3 godzin sesji, 85% sakiewki na kowadło. Bazy kupuje na ladach sklepów aż do +6, a to, co wyjdzie lepsze od jego sprzętu, zakłada.",
                 "Szansa 1 do 1000, przerwa świata 4 godziny, jednocześnie jeden."]},
    12: {"kind": "rare", "summary": "Szalony Naukowiec: wyprawa po księgi umiejętności do mistrza.",
         "how": ["Do 90 minut na zakupy ksiąg Mistrzowskich, za 70% sakiewki.",
                 "Szansa 1 do 500, przerwa świata 8 godzin, jednocześnie jeden."]},
    13: {"kind": "rare", "summary": "Egzekutor: dwie godziny polowania na graczy z innych królestw na wspólnych mapach.",
         "how": ["Od 39. poziomu i z bronią +6 albo bonusem 10% przeciw ludziom.",
                 "Ofiarą jest postać innego królestwa w zasięgu, nie więcej niż 10 poziomów od niego, nigdy GM. Boty królestwa ofiary w pobliżu przychodzą jej z pomocą (do 6 naraz).",
                 "Śmierć z rąk bota wysyła Egzekutora w inne miejsce.",
                 "Szansa 1 do 400, przerwa świata 5 godzin, jednocześnie jeden."]},
    14: {"kind": "rare", "summary": "Szalony Wędkarz: sześć godzin nad wodą.",
         "how": ["Od 30. poziomu, z wędką i dotychczasowym łowieniem. Sesje oddzielone minutą lub dwiema; plecak opróżnia w mieście między nimi.",
                 "Szansa 1 do 600, przerwa świata 12 godzin, jednocześnie jeden."]},
    15: {"kind": "gambler", "summary": "Młodszy Hazardzista: najłagodniejszy z czterech rzadkich hazardzistów.",
         "how": ["Losowany wśród najbogatszych 60% postaci świata. Stawia 80% sakiewki, jaką miał na początku, i kupuje 1–3 baz +0…+5, jeśli plecak ich nie ma.",
                 "Szansa 1 do 200, przerwa 2 godziny, sesja do 3 godzin."]},
    16: {"kind": "gambler", "summary": "Starszy Hazardzista.",
         "how": ["Wśród najbogatszych 40%. Stawia 70%, kupuje 2–5 baz.",
                 "Szansa 1 do 250, przerwa 4 godziny, sesja do 3 godzin."]},
    17: {"kind": "gambler", "summary": "Naczelny Hazardzista.",
         "how": ["Wśród najbogatszych 25%. Stawia 60%, kupuje 3–6 baz.",
                 "Szansa 1 do 300, przerwa 8 godzin, sesja do 3 godzin."]},
    18: {"kind": "gambler", "summary": "Szalony Hazardzista: pracuje na każdej kategorii przedmiotów naraz.",
         "how": ["Wśród najbogatszych 15%. Stawia 90%, kupuje 3–6 baz. Jeśli nie ma przedmiotów spełniających warunki, zachowuje się jak Naczelny.",
                 "Szansa 1 do 400, przerwa 8 godzin, sesja do 4 godzin."]},
    19: {"kind": "base", "summary": "Zielarz: bot przy tablicy zielarskiej Baek-Go zbiera zioła.",
         "how": ["Siostrzana persona Górnika: bot na tablicy zielarskiej tak jak Górnik przy żyle.",
                 "Zajmuje bota w trakcie drogi do tablicy i przy niej."]},
}

# What the status panel calls the three moods, and what each does.
MOODS = [
    ("Słaby", "Między paczkami potworów robi przerwy 2–8 sekund, a co 10–30 minut stoi 2–5 minut (jak gracz, który poszedł do kuchni). Chętnie łowi ryby."),
    ("Normalny", "Zwykłe zachowanie. W drużynie lub lochu bot zawsze gra z tym nastrojem."),
    ("Bardzo dobry", "Zachowuje się bez przerw. Rzadko łowi ryby."),
]
MOOD_RULES = [
    "Co równe 6 godzin gry nastrój jest losowany od nowa (każdy z trzech jednakowo prawdopodobny).",
    "30 minut polowania bez wartościowego łupu obniża nastrój o jeden stopień. Wartościowy łup podnosi o jeden.",
    "Udane ulepszenie na +8 lub +9 daje euforię: Bardzo dobry na równe 3 godziny, a nic go nie zmienia.",
    "Spalone ulepszenie na +8 lub +9 (także spadek stopnia pod zwojem) obniża nastrój o jeden.",
    "5 śmierci z rąk graczy w ciągu 15 minut w tym samym miejscu to kapitulacja (protokół Anty-PK): nastrój Słaby na 45 minut i porzucenie miejsca.",
]

TIERS = [
    ("1", "Pierwsza wioska (M1)", "10–18", "13–19"),
    ("2", "M3: przeklęte zwierzęta i bronie 30 poziomu", "19–25", "19–25"),
    ("3", "Druga wioska (M2)", "26–35", "30–35"),
    ("5", "Dolina Orków i Pustynia Yongbi", "36–50", "40–48"),
    ("7", "Góra Sohan", "51–65", "55–62"),
]
LAW = [
    ("od 0", "+5", "+4", "—", "—"),
    ("od 19", "+6", "+5", "+4", "—"),
    ("od 26", "+7", "+5", "+5", "—"),
    ("od 35", "+8", "+6", "+6", "+6"),
]
PERSONA_GROUPS = (
    ("base", "Persony zwykłe", "Zmieniają się wraz z tym, co bot akurat robi (kolejność ważności od góry: kontrakt, drużyna, rzadka persona, rybak, górnik, zielarz, pogromca, hazardzista, perfekcjonista, handlarz, awans / grind)."),
    ("rare", "Rzadkie persony (czerwone nad głową)", "Własny stan o określonej długości. Wygrywa go jeden bot na tylu spełniających warunki, ile podaje „szansa”; po starcie jednego danego rodzaju następny może wystartować dopiero po „przerwie świata”. Restart gry kasuje stany i przerwy."),
    ("gambler", "Czterej hazardziści (fioletowe)", "Zastąpili dawnego Hazardzistę. Pracują na przedmiotach z listy przydatnych przedmiotów, a gdy plecak ich nie ma, najpierw kupują bazy +0…+5. Na koniec sesji dokładają zielone kamienie."),
)
