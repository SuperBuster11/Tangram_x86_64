# Tangram (Thalion, 1991) – port do C++

Przepisanie gry Tangram z Amigi na C++17. Pliki gry (grafika, poziomy, hasła, muzyka – ok. 250 KB z oryginalnej dyskietki)
są wkompilowane w program (`src/embedded_assets.cpp`), więc obraz dyskietki `.adf` nie jest potrzebny do uruchomienia.
Muzyka jest odtwarzana przez wbudowany interpreter procesora 68000 i emulację układu Paula.

## Sterowanie
| Akcja | Sterowanie |
|---|---|
| Start gry | menu: Enter albo prawy przycisk myszy (lewy klik na smoku nie startuje gry, jak w oryginale) |
| Hasło do etapu | w menu wpisz 5 cyfr (Backspace kasuje), potem Enter |
| Podniesienie klocka | wciśnij lewy przycisk na klocku (z tacki albo z planszy) i trzymaj |
| Odłożenie klocka | puść lewy przycisk nad figurą (poza figurą klocek wraca na tackę) |
| Obrót klocka | prawy przycisk podczas trzymania |
| Pauza / koniec gry (pytanie o scoredisk, potem tabela wyników; także w etapie bonusowym) / wyjście z gry | P / Esc w grze / Esc w menu |
| Po ukończeniu etapu | klik lub Enter na ekranie z hasłem |
| Muzyka | M – wyłącz / włącz (25%); F2 – zmiana utworu (cyklicznie) |
| Rozmiar okna | F7 – kolejno 960×600, 1280×800… do największego, który mieści się na ekranie, potem znowu 960×600 |
| Filtr obrazu | F8 – wyłączony → ostrzejszy (EPX, zaokrągla skosy) → wygładzony (bilinearny) |

## Budowanie
Linux: `cmake -S . -B build && cmake --build build -j` → `build/tangram_x11 [--scale 3]`
(wymaga bibliotek X11). Windows (Visual Studio 2022/2026, „Otwórz folder” z CMake): cel `tangram`, uruchomienie
`Tangram_x64.exe` / `Tangram_x86.exe` – zależnie od konfiguracji x64/x86 (bez argumentów; opcjonalnie ścieżka do `.adf` zastępuje wkompilowane dane). Szczegóły: `README_VS2022.txt`.

Testy: `TANGRAM_ADF=/ścieżka/disk.adf ctest --test-dir build --output-on-failure`
(bez zmiennej testy zależne od ADF są pomijane; `test_embedded` działa bez ADF, a z ADF porównuje wkompilowane pliki z oryginałem).
Odtworzenie wkompilowanych danych: `pack_assets disk.adf src/embedded_assets.cpp`.

## Układ katalogów
- `src/` – kod gry: `game.*` (menu, plansza, hasła, przejścia), `board/pieces/raster` (klocki i pokrycie figury),
  `levels/passwords/scoring/highscores` (dane i reguły), `dat/font/sprite/image/adf` (formaty), `m68k/music` (muzyka),
  `main_x11.cpp`, `main_win32.cpp` (okna).
- `tools/` – narzędzia: rozwiązywanie etapów, eksport grafik, renderowanie muzyki do WAV.
- `tests/` – testy; `tests/data/` – wzorce zdjęte z prawdziwej gry (zrzuty ekranu, rozwiązania etapów 1/10/20, lista haseł).
- `NOTATKI.md` – dziennik prac i pomiarów, etap po etapie (po polsku).

## Stan
Działa (sprawdzone na Linuksie, porównanie z klatkami z prawdziwej gry): intro, menu, plansza, układanie klocków, zliczanie
punktów, hasła (200 etapów), muzyka, przejście z hasłem w żaluzjach.
Etap dodatkowy odtworzony według zrzutu i opisu (ikony losowo rozrzucone). Przybliżone: animacja odliczania bonusu, położenie wpisywanego hasła.
Brak: tryb dla dwóch graczy, tabela wyników / koniec gry, działanie przełączników Novice/Expert.
Kod Win32 nie był kompilowany ani uruchamiany.

Tabela wyników jest zapisywana w pliku `tangram.sco` w bieżącym katalogu.
