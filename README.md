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
| Obrót klocka | prawy przycisk podczas trzymania; przytrzymany obraca dalej (po ok. 0,25 s, potem co 6 klatek) |
| Pauza / koniec gry (pytanie o scoredisk, potem tabela wyników; także w etapie bonusowym) / wyjście z gry | P / Esc w grze / Esc w menu |
| Po ukończeniu etapu | klik lub Enter na ekranie z hasłem |
| Muzyka | M – wyłącz / włącz (25%); F2 – zmiana utworu (cyklicznie) |
| Rozmiar okna | F5 – kolejno 1280×800, 1600×1000… do największego, który mieści się na ekranie, potem znowu 1280×800 |
| Pełny ekran | F11 albo Alt+Enter – włącza/wyłącza; obraz 16:10 tak duży, jak pozwala monitor, z czarnymi pasami po bokach lub u góry i dołu |
| Filtr obrazu | F8 – po kolei: monitor Amigi 1084S (domyślny) → linie CRT → wygładzony (bilinearny) → wyłączony → 1084S |

## Budowanie
Linux: `cmake -S . -B build && cmake --build build -j` → `build/tangram_x11 [--scale 4]`
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

## Obraz
- **Okno:** startuje w 1280×800; F5 przełącza 1280×800 → 1600×1000 → … → największy rozmiar mieszczący się na ekranie.
- **Pełny ekran:** F11 albo Alt+Enter. Obraz 16:10, tak duży, jak pozwala monitor (np. 1728×1080 na 1920×1080), z czarnymi pasami;
  mysz działa w obrębie obrazu. Na Windows, gdy 1280×800 nie mieści się na ekranie, gra od razu startuje w pełnym ekranie.
  Zmaksymalizowane lub ręcznie powiększone okno dopasowuje obraz tak samo.
- **Filtry (F8):**
  - *monitor 1084S* (domyślny) – naśladuje monitor Amigi: lekkie rozmycie poziome, miękkie linie (ciemne kolory z przerwami,
    jasne prawie bez), delikatna maska pasków RGB;
  - *CRT* – wyraźne linie skanowania;
  - *wygładzony* – interpolacja bilinearna;
  - *wyłączony* – ostre piksele.
  Filtr liczy obraz od razu w docelowym rozmiarze (także przy skali niecałkowitej w pełnym ekranie), bez późniejszego rozciągania.

## Muzyka z nagrań
Pliki `music1.wav` (utwór 1, od startu gry) i `music2.wav` (utwór 2, etapy bonusowe) zastępują emulowane utwory. Gra szuka ich
obok pliku exe (albo w bieżącym folderze); bez nich gra emulowany odtwarzacz z oryginału. Najprościej włożyć je do folderu
`assets/` – przy kompilacji CMake kopiuje je obok exe. Wymagany WAV 16-bit PCM (mono lub stereo, dowolna częstotliwość);
nagranie gra od początku, gdy utwór się zaczyna (start, M, F2, bonus), po końcu znów od początku, z tą samą głośnością (25%).

## Stan
Gra jest kompletna i porównywana z oryginałem (zrzuty z WinUAE, analiza kodu 68000): intro, menu, 200 etapów z hasłami,
układanie i obracanie klocków, sprawdzanie ułożenia jak w oryginale, punktacja, tryby Novice/Expert, tryb dla dwóch graczy
(z odpadaniem gracza), etapy bonusowe (co 10 ukończonych etapów), pauza, „PLAY AGAIN FOR … POINTS”, tabela wyników
ze scoredisk, muzyka (utwór 2 w etapach bonusowych) i efekty dźwiękowe.
Testy działają na Linuksie; wersja Win32 jest budowana i sprawdzana przez użytkownika w Visual Studio 2022 (okno, filtry
i pełny ekran działają).

Tabela wyników jest zapisywana w pliku `tangram.sco` w bieżącym katalogu.
