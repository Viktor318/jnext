# JNEXT fork – átadás a következő munkamenetnek

(Eredetileg a claude.ai „ZX Spectrum emulátorok továbbfejlesztése” projekt `claude/jnext-atadas.md` dokumentuma. **2026-11-21 óta itt, a tárolóban él**, és itt vezetjük – a projektben csak egy hivatkozás maradt rá.)

Repó: `Viktor318/jnext` (upstream: `jorgegv/jnext`). ZX Spectrum Next emulátor fejlesztőknek: C++17, Qt6 + SDL3 felület, WebAssembly-változat, a hivatalos FPGA-forrás (VHDL) alapján, erős hibakeresővel. Verzió 1.3.43 (a legutóbbi nyilvános kiadás v1.3.5; a köztes patch-verziók csak belső címkék, `releases.yaml`). Ágak: `main` (upstream tükre) és `hungarian-ui` (saját munka, a GitHubon ez az alapértelmezett ág). **Állapot 2026-11-21 21:00: a `hungarian-ui` = upstream main (1.3.43, `62485f8`) + magyarítás 1–3. lépés (főablak, Beállítások, a hibakereső teljesen), 1.5x-es nagyítás, Csak a kép / Shift+F11, 26.11a alapértelmezés a magyar buildben, Windows-fordítás WSL-ből (`tools/hu/build-windows-cross.sh`), kézi tesztfutás a GitHubon (`hu-tests.yml`), a `startup.language` a hibajelentésben (`5b20d8e`), `CLAUDE.local.md` a tárolóban (`3a1a793`), a magyar munkadokumentumok a `doc/hu/`-ban.**

## A magyar munkadokumentumok (`doc/hu/`)

| Fájl | Tartalom |
|---|---|
| `jnext-atadas.md` | ez: állapot, döntések, szabályok, munkakörnyezet, következő lépések |
| `jnext-magyaritas.md` | a felület magyarítása: lépések, működés, szóhasználat, gyorsbetűk |
| `jnext-dma-meres.md` | DMA-időzítés KS3-mérése, javaslat és hatásfelmérés (javítás csak jóváhagyás után) |
| `jnext-hang-terv.md` | recsegés, hangszín, DAC-keverő (kívánságlista 3–4.) |
| `jnext-blitter-becsles.md` | a Next-emuláció állása és a zxnDDMA-blitter becslése |

A `CLAUDE.local.md` (a tároló gyökerében, `git add -f`-fel követve, mert a `.gitignore` kizárja) a tartós szabályokat és egy rövid állapotsort tartalmaz; a részletek itt vannak.

## Három munkahely (Viktor, 2026-11-21 este)

- **Helyi Claude Code Viktor gépén:** a WSL-ben (Ubuntu 26.04, 20 szál), a `~/jnext`-ben. A `CLAUDE.local.md` írja le a fork szabályait. **A nagy fordításokat és teszteket mostantól inkább ott futtatja Viktor**, percek alatt. Ezeket a dokumentumokat ott is olvasni és frissíteni kell.
- **A JNEXT-es claude.ai-beszélgetés (felhő, 2 mag):** tervezés, felmérés, kisebb változtatások. A `CLAUDE.local.md`-t a felhős Claude Code is automatikusan betölti, ha a tárolóban van.
- **A ZEsarUX-beszélgetés:** nem a JNEXT-tárolóval dolgozik; ha ott születik JNEXT-re vonatkozó jegyzet, az a projektbe kerül (`claude/jnext-*.md`), és Viktor szól, hogy át kell vezetni ide.
- Mindegyik helyről a `hungarian-ui`-ra kerül a munka. **Munka előtt mindig `git fetch` + `git pull --ff-only`**, mert a másik hely közben tolhatott (2026-11-21 este a helyi Claude Code egyszer a régi `origin/hungarian-ui`-hoz hasonlított).
- **A helyi gép sajátosságai:**
  - A tároló hookjai (`.claude/hooks/`, az upstream szerzőé) `jq` és `perl` nélkül minden parancsot letiltanak – Viktor telepítette a `jq`-t. A `block-push.sh` minden `git push`-t letilt, ha nincs előtte `JNEXT_ALLOW_PUSH=1`; a `hungarian-ui`-ra így: `JNEXT_ALLOW_PUSH=1 git push origin hungarian-ui`. A `block-main-write.sh` a `main`-t védi.
  - A `~/.gitconfig` hitelesítője a Git for Windows régi útvonalára mutatott (`…/Git/mingw64/bin/git-credential-manager.exe`); az újabb Git for Windows a `…/Git/ucrt64/bin/` alá teszi. Javítás: `git config --global credential.helper '/mnt/c/Program\ Files/Git/ucrt64/bin/git-credential-manager.exe'`.
  - A `gh` a WSL-ben is telepítve és bejelentkezve (Viktor, 2026-11-21 este: `sudo apt install -y gh`, `gh auth login`), így a `gh workflow run` / `gh run view` onnan is megy.

## Viktor céljai és döntései

- **Elsődleges cél: saját magyar változat** (magyar felület, windowsos magyar billentyűzet), ahogy a Fuse és a ZEsarUX forknál, **és hibajavítások** – Viktor használat közben már több hibát talált (gyűjti a listát).
- Párhuzamosan dolgozik a ZEsarUX forkkal; az ötleteket mindkét emulátorban meg szeretné valósítani.
- **Egyetlen munkaág: `hungarian-ui`.** Az upstreamnek visszaküldés **egyelőre nem cél**.
- **Commitok a `hungarian-ui`-n:** magyarul, „Terület: leírás” formában, záró pont nélkül, a szokásos záró sorokkal (Co-Authored-By, Claude-Session). A tároló `CLAUDE.md`-jének commitszabálya (tömör angol, Co-Authored-By nélkül) az upstreamnek szánt munkára vonatkozna. Push a `hungarian-ui`-ra: állandó engedély; más ágra, címkére, PR-re külön engedély kell.
- A hibajavítások önálló, lezárt commitok legyenek (teszttel, ahol lehet).
- **Javítani csak Viktor jóváhagyása után** szabad kezdeni (a saját, épp készülő munkában okozott hibát Claude azonnal javíthatja – Viktor ezt elfogadta).
- Viktornak van valódi gépe is: **ZX Spectrum Next KS3** (issue 5; 3.02.04 és 3.03.03 core-t futtat).
- **Viktor a System/Next 26.11a-t használja**, lásd lent.
- **Blitter (zxnDDMA) a JNEXT-ben:** becslés kész (`doc/hu/jnext-blitter-becsles.md`); Viktor **még nem döntött**.
- **Nagyítás (2026-11-20):** új 1.5x-es fokozat (960×768) – kész, `53dbe28`, Viktor kipróbálta.
- **Csak a kép (2026-11-20):** Shift+F11 elrejti a menüt és az állapotsort – kész, `c313eff`, Viktor kipróbálta. Csak az adott futásra szól, nem mentődik.
- **Upstream beolvasztása:** 1.3.34 (`6ee9600`) és 1.3.43 (`62485f8`), mindkettő Viktor kérésére. A következő előtt is megkérdezni.
- **Hibakereső magyarítása (2026-11-21):** kész, `77f57b4`. Viktor a fordítást átnézte („a fordítások jók lesznek”).
- **26.11a alapértelmezés (kívánságlista 1., 2026-11-21):** Viktor döntése: **csak a magyar build** (`a4ffecb`). A CMake `JNEXT_DEFAULT_DISTRO_URL` változója (`sdcard::default_distro_url()`); a `tools/hu/build-windows-cross.sh` ezzel fordít (`JNEXT_HU_DISTRO_URL` felülírja). A `kDistroUrl` (26.11) változatlan, mert a regressziós tesztek és a böngészős kártya rögzítései arra épülnek. Teszt: PROV-URL-04. Viktor kipróbálta, jól működik.
- **Windowsos magyar billentyűzet (kívánságlista 2., Viktor döntései 2026-11-21):**
  - „Windowsos billentyűzetkiosztás”: alapból a felület nyelvét követi (magyar felület → be); a mentett beállítás felülírja. Tervezett kulcs: `input.windows_layout = auto|on|off`, Beállítások > Bemenet.
  - Bal Alt + számjegy = Caps Shift + számjegy: külön kapcsoló, alapból be (`input.alt_digit_caps_shift`). Az Alt+E/G/C marad.
  - Ha be van töltve hibakereső-szkript (van `Host` feliratkozás), az Alt+1..8 továbbra is a szkripté; az Alt+9 és az Alt+0 mindig Caps Shift.
  - Terv (Claude): a Fuse-fork `hungarian_keyboard.c` (`C:\Dev\fuse`, WSL-ből `/mnt/c/Dev/fuse`) mintájára egy önálló, tesztelhető modul (`src/platform/`) a leütött karakterből (`QKeyEvent::text()`) állítja elő a Spectrum-kombinációt, a fizikai billentyű (`nativeScanCode()`) szerint tartja nyilván, és a Router felé küldi SDL-scancode-eseményekként (betű, számjegy, LSHIFT = Caps Shift, LCTRL = Symbol Shift).
    - Az E-módos jelek (`[ ] { } \ | ~ ©`) képkockákhoz időzített sorozatként mennek (3 képkocka lépésenként, a `QtApp` `post_frames`-éből ütemezve).
    - A Shift/Ctrl a karakteres leütés idejére felülíródik, utána a gazdagép módosítói szerint áll vissza.
    - Karakteres út csak Alt és Ctrl nélkül, vagy AltGr-rel (Windowson Ctrl+Alt együtt); a Ctrl+betű marad Symbol Shift + betű.
    - Az Alt+számjegy ugyanebben a modulban van: CS + számjegy kibocsátása, így a Router `script_chord_held()` nem nyeli el, mert Shift is lenyomva. A Router és a `Keyboard` érintetlen; a szkriptvizsgálathoz a `Debugger` kap egy `has_host_subscribers()` függvényt (`impl_->events.has_kind(EventKind::Host)`).
    - Billentyűzetvesztéskor (`keyboard_lost`) a modul nyilvántartása törlődik.
    - Beállítás: `config_keys.h` sor + `main.cpp` `qt_assignments` + `app_config` load/save/merge + `bug_report.cpp` + Beállítások-mező + man page/user guide + fordítás.
  - **Pontosítások (2026-11-21 este, a helyi és a felhős Claude átnézése után, Viktor elé téve):**
    - **Az Alt-ot is a modul tartja nyilván**, a Shifthez és a Ctrl-hoz hasonlóan: karakteres leütés idejére felengedi, utána visszaállítja. Ok: Windowson az AltGr bal Ctrl + jobb Alt, a `Keyboard` a jobb Alt-ot is Altnak veszi (`keyboard.cpp:378`), így AltGr mellett a G GRAPH, az E EDIT, a C CAPS LOCK lenne (pl. a `}` = E mód + SS+G). Mérendő Windowson: hogyan jelenti a Qt az AltGr-t (várhatóan Ctrl+Alt módosítók, `Key_Alt`, a jobb Alt `nativeScanCode()`-ja 0x138).
    - **Halott billentyűk** (magyar windowsos kiosztás: AltGr+1 `~`, AltGr+3 `^` stb.): a modul kifejezetten elnyeli a `Qt::Key_Dead_*` leütéseket és a felengedésüket. A karaktert hozó következő leütést (pl. szóköz → `~`) annak a fizikai billentyűjéhez köti. Windowson kipróbálandó.
    - **Spectrumon nem létező jel** (pl. `ß`, `€`): a modul elnyeli, a régi útra (`qt_key_to_sdl`) nem engedi.
    - **Nincs külön óra:** az E-módos sorozat a `QtApp` `post_frames(frames_rendered)`-jéből lép, ugyanabból, amiből a Router `on_tick_end`-je, tehát emulált képkockákat számol; szüneteltetett gépnél a sorozat is áll. A 3 képkockás lépést a 48K-s, a 128K-s és a NextBASIC-szerkesztőben és Viktor KS3-án össze kell vetni.
    - **Ctrl+Alt egérelengedés:** bekapcsolt magyar kiosztásnál csak a bal Alt-tal működik (Windowson a `nativeScanCode()` szerint), mert az AltGr a jobb Alt; így a `@` és a `{` nem engedi el az egeret. Teszt a `pointer_capture` mellé.
    - **Alt+számjegy:** a döntés (szkript vagy Caps Shift + számjegy) a lenyomáskor születik, a felengedés azt követi, a fizikai billentyűhöz kötve; akkor is, ha a szkript a kettő között töltődik be. Külön teszt.
    - **Hatókör:** csak a Qt-felület; az SDL-es ablak marad. A fizikai azonosító: `nativeScanCode()`, ha 0, akkor `nativeVirtualKey()`, végül `key()`.
    - **Sorrend:** előbb a Qt nélkül tesztelhető modul és egységtesztjei, utána a bekötés a `MainWindow`-ba; a Windows-mérés (AltGr, halott billentyűk) a végén, Viktor gépén.
  - Még nincs elkezdve; Viktor 2026-11-21 este: „holnap kezdjük”.
- **GitHub Actions (Viktor engedélyezte, 2026-11-21):**
  - „Windows Build (manual)”: `gh workflow run "Windows Build (manual)" -R Viktor318/jnext --ref hungarian-ui -f qt6=true -f qt5=false -f win32=false` (14 perc, PGO-s).
  - **A teljes CI** (`ci.yml`): `gh workflow run CI -R Viktor318/jnext --ref hungarian-ui`. **Viktor döntése (2026-11-21 20:46): a `ci.yml` SD-kép-gyorsítótára („Cache NextZXOS SD-card image”, actions/cache) belefér a szabályba** – a gyorsítótárat csak a tároló munkafolyamatai olvassák, a felületen nem tölthető le, és az upstream CI is így használja. Úgy fut, ahogy van. Első futás: `44942749295` (a `5b20d8e`-re).
  - **`hu-tests.yml` (saját, kézi):** csak `make unit-test` + `make unit-test-sdl` fedora:45-ben, SD-kép-gyorsítótár nélkül. Indítás: `gh workflow run hu-tests.yml -R Viktor318/jnext --ref hungarian-ui`. Első futás (`44941423919`, `77a02cb`): várt módon a `startup.language` `-Wswitch` hibáján bukott; második futás: `44942439407` (`5b20d8e`). A teljes CI mellett kérdés, kell-e még (gyorsabb, kisebb).
  - A felhő proxyja az Actions API egy részét tiltja (`actions/permissions` 403); a run list/view és az indítás működik. A naplót a `gh run view --log` / `--log-failed` csak a futás végén adja.
- **Viktor kívánságlistája (2026-11-21), ebben a sorrendben**, mindegyik előbb felmérés, aztán jóváhagyás:
  1. ~~a 26.11a legyen az alapértelmezett letöltés~~ – kész (magyar build);
  2. windowsos magyar billentyűzet – döntések megvannak, következik;
  3. ne akadjon az emuláció a Windows-ablak menüinek használata és az ablak mozgatása közben (a Windows modális ciklusa);
  4. külön hangkeverő – az első konkrét tartalom (a négy DAC-csatorna hangereje, bal–közép–jobb elhelyezés, alapból a valódi gép szerint) és a recsegés: `doc/hu/jnext-hang-terv.md`. A recsegés és a 3. pont (akadás) összefügghet.
- **DMA-időzítés (2026-11-21, KS3-mérés):** `doc/hu/jnext-dma-meres.md` (benne a hatásfelmérés). A JNEXT-javítás csak Viktor jóváhagyása után.
- Egérbefogás: alapból Ctrl+Alt engedi el (Beállítások > Bemenet: középső kattintás vagy Esc is választható; `--mouse-release`); fókuszvesztéskor is elenged.

## A tároló saját szabályai (`CLAUDE.md`) – a legfontosabbak

- A `main`-re közvetlenül nem szabad írni.
- **Push csak Viktor kifejezett engedélyével** (a `hungarian-ui`-ra állandó engedély van).
- **NextZXOS SD-kártyakép soha nem kerülhet a tárolóba vagy a GitHubra** (Viktor szabálya és a `CLAUDE.md`: se commit, se melléklet, se csomag, se zip, se letölthető CI-műtermék). **A GitHub Actions gyorsítótára (actions/cache) Viktor döntése szerint belefér** (2026-11-21). Viktornak egy **üres** FAT32-képet (`next-ures-1gb.zip`) küldtünk – az nem tartalmaz NextZXOS-t, ezért szabad.
- Minden változáshoz teszt és független átnézés; a SKIP is hiba; a tesztszámok a `test/unit-tests.conf`-ban rögzítettek (sorszám változásakor a manifesztet is igazítani, megjegyzéssel). **Nyitott kérdés Viktornak:** a javításokat külön átnéző ágens nézze-e át, vagy elég Claude saját, tesztekkel alátámasztott ellenőrzése.
- Minden Beállítások-mezőnek legyen `--set` kulcsa (`src/platform/config_keys.h`), man page-bejegyzéssel (`cli_options_test` ellenőrzi); a kulcstípusok számát a `config_keys_test` SETK-08/10 rögzíti. **Új kulcsnál a `src/platform/bug_report.cpp` `config_value_rule()` kapcsolójába is fel kell venni** (és a `bug_report_test` várt listájába): `-Werror` mellett a hiányzó eset fordítási hiba, a `JNEXT_WERROR=OFF`-os fordításban csak figyelmeztetés (így maradt ki a `startup.language`, javítva `5b20d8e`).
- A menük gyorsbetűinek és gyorsbillentyűinek számát a `main_window_accel_test` MA-05 / MA-HU-05 és a `debugger_accel_test` DACC-05 / DACC-HU-05 rögzíti (1.3.43 óta a hibakeresőben 8 menü, 16 almenü, 69 gyorsbetű, 28 gyorsbillentyű); az emulátorablak hibakeresőben is lefoglalható gyorsbillentyűit a `debugger_keymap_test` DKH-01 (hat: Alt+F4, Ctrl+F5, Ctrl+F6, F4, F11, Shift+F11).
- Minden külső folyamat `LANG=C` alatt fut.
- A generált dokumentumok commitolva vannak és frissességre ellenőrzöttek (hogyan lehet őket a felhőben előállítani: `doc/hu/jnext-magyaritas.md`); a `doc/testing/TRACEABILITY-MATRIX.md` is generált (lent).
- A ChangeLog csak kérésre frissül; a FEATURES.md-t jelentős újdonságnál frissíteni kell.
- A felületen soha nem szabad felirat szerint keresni (a fordítás elrontja) – objektumnév kell.
- A VHDL a mérvadó.

## Billentyűk (a főablakban)

F1 hidegindítás, F2 nagyítás léptetése, F3/F5–F8 a Next saját funkcióbillentyűi (a gép kapja), F4 újraindítás, F9 Multiface NMI, F10 DivMMC NMI, F11 teljes képernyő, **Shift+F11 Csak a kép**, F12 szabad (de a hibakereső Follow parancsa, amíg a hibakereső nyitva). Alt+E/G/C a vendégé. Foglalt Alt-gyorsbillentyűk: O, L, S, K, Q, P, U, D, T.

A hibakereső menüsora magyarul: &Fájl, &Hibakeresés, &Ugrás, &Szkript, &Térkép, Tö&réspontok, Fi&gyelők, &Ablak (benne 1.3.43 óta &Panelek és Elrendezés &visszaállítása); a hibakereső Alt-gyorsbillentyűi: Alt+L, Alt+1..8, Alt+Bal/Jobb.

A billentyűzet útja: `MainWindow::keyPressEvent` → `handle_key` → `qt_key_to_sdl` (logikai Qt-billentyű → SDL scancode, amerikai kiosztást feltételez) → `key_callback_` → `host_key_latch::Router` (sorba rendezés, minimális tartás, Alt+1..8 szkriptbillentyűk) → `Keyboard::set_key`. Ctrl = Symbol Shift, Shift = Caps Shift, Tab = EXTEND, Esc = BREAK, Alt = gazdagép-módosító.

## Munkakörnyezet a felhőben (minden új beszélgetésben újra elő kell állítani)

- Tároló: `add_repo` push-joggal, `git clone --depth 1 --no-single-branch`, `git checkout hungarian-ui`, `git pull --ff-only`. Upstream: `git remote add upstream https://github.com/jorgegv/jnext; git fetch --depth=200 upstream main`.
- **Ezen a gépen 2 mag van.** Egy nagy beolvasztás utáni teljes újrafordítás a Linuxos Qt-fán (≈490 lépés, a tesztekkel) kb. 1,5 óra; a háttérparancsokat 2 órás (7200000 ms) időkorláttal kell indítani. **A környezet a felhasználói üzenetek között újraindulhat, és leállítja a háttérparancsokat** – ezért egy körön belül kell lekérdezni őket, és ha leálltak, újraindítani (onnan folytatja). A Windows-fán a `jnext` célpont ~30 perc. A teljes `run-unit-tests.sh` ~15–20 perc. Ha a `jnext_hu.qm` változik, a `jnext_gui`-t használó összes teszt újralinkelődik (~100 cél, kb. 30 perc). Gyors próbákhoz elég a Qt nélküli SDL-es fa: SDL3 forrásból (`release-3.2.26`, `/usr/local`-ba), `cmake -S . -B build-sdl -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_QT_UI=OFF -DENABLE_DEBUGGER=OFF`, `ninja -C build-sdl jnext` – néhány perc. **Nagy fordításra és teljes tesztre inkább Viktor helyi Claude Code-ja vagy a GitHub Actions.**
- **Linuxos Qt-s fordítás:** `apt install libcurl4-openssl-dev libssl-dev libpng-dev zlib1g-dev ccache ninja-build xvfb xdotool imagemagick libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-keysyms1 libxcb-shape0 libxcb-render-util0 libxcb-image0 libegl1 libdbus-1-dev libasound2-dev libpulse-dev` stb.; SDL3 forrásból (`release-3.2.x`) a `/home/claude/sdl3`-ba; Qt 6.8.3 linuxra az aqt-vel (lent); `cmake -S . -B /home/claude/jnext-build-qt -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/home/claude/qt/6.8.3/gcc_64;/home/claude/sdl3" -DENABLE_QT_UI=ON -DENABLE_TESTS=ON -DJNEXT_WERROR=OFF` (a `JNEXT_WERROR=OFF` az Ubuntu gcc-je miatt kell a `sd_rom_extractor_test.cpp` `(void)write(...)` sorára; **emiatt a `-Wswitch`-szerű hibák itt csak figyelmeztetések – a fordítási naplóban `warning:`-ot keresni**). Egyes tesztek: `cmake --build … --target <teszt>`, futtatás `build/test/<teszt>` (Qt-s tesztnél `QT_QPA_PLATFORM=offscreen`). Az összes: `JNEXT_TEST_JOBS=2 bash test/run-unit-tests.sh /home/claude/jnext-build-qt`. Kipróbálás: `Xvfb :91 -noreset`, `jnext --set startup.language=hu --silent --sdcard <másolat>`, `import -window root`, kattintás és billentyű `xdotool`-lal.
- **Ismert, csak környezeti hibák a teljes futásban** (2026-11-21, `77f57b4`): `crt_shader_widget_test` (a Fedora `xvfb-run -d` kapcsolóját kéri), `color_scheme_linux_test` CS-29 (dbus), `esp_socket_test` LSN-19/20 SKIP (nincs RFC1918 cím).
- **Nyomonkövetési mátrix:** `perl test/refresh-traceability-matrix.pl` a `build/test/`-et keresi – ideiglenes `ln -sfn /home/claude/jnext-build-qt build` (a `build`-et a `.git/info/exclude`-ba tenni). A felhőben nincs FPGA-forrás, ezért a VHDL-hivatkozás oszlopa másképp jön ki: **csak a valódi sorokat szabad átvenni** (a manifeszt-számok táblája, a „Rows the sections…” és a „These 184 suites…” sor), a többi eltérést nem.
- SD-kép a tesztekhez: `jnext --headless --sdcard-download-confirm --delayed-automatic-exit 2`.
- A teljes `make unit-test` a felhőben nem fut zölden (pandoc-verzió, wine, Graphviz, a fenti környezeti hibák).
- Generált dokumentumok kézi frissítése (pandoc nélkül): a man page forrását szerkeszteni, a `jnext.1`-et, a `USAGE.md`-t és a user guide függelékét pythonos `textwrap`-pel 72 oszlopra tördelve (a roff-ban mondatonként új sor, `\-`, `\(cq`; a markdownban `’`, `\|`); a user guide HTML-je a venv-es mkdocs-szal (`/home/claude/mkdocs-venv`, mkdocs 1.6.1, mkdocs-material 9.7.1, Markdown 3.10; `PATH=/home/claude/mkdocs-venv/bin:$PATH make docs-userguide`, majd `make docs-userguide-check` OK).
- Fordítások: `LUPDATE=…/gcc_64/bin/lupdate LRELEASE=…/gcc_64/bin/lrelease bash tools/hu/update-translations.sh` (mindkét változó kell); kitöltés `/home/claude/tsfill.py TS JSON` (`{"Kontextus": {"forrás": "fordítás"}}`, `%n`-es szövegnél lista).

## SD-kártyakép

- A jnext a képet a `%USERPROFILE%\.jnext\sdcard\` mappába tölti le (ha `HOME` be van állítva, az előbbre való; `JNEXT_CONFIG_DIR` az egész `.jnext`-et áthelyezi). Ugyanitt a `jnext.conf`.
- Letöltési cím: a magyar buildben 26.11a (`https://www.specnext.com/distro/26.11a/sn-emulator-26.11a.zip`, 66 MB), egyébként `kDistroUrl` = 26.11. A `JNEXT_SDCARD_DISTRO_URL` környezeti változó mindkettőt felülírja.
- A 26.11a 3.03.03-as core-t hoz, a JNEXT 3.02.03-at emulál.
- A jnext a gyári képből `cspect-next-1gb-fixed.img`-t készít: újraformázás 8 KB-os klaszterekkel, alapértelmezett `config.ini`, a régi 16 KB-os `zx81.rom` megduplázása, a kötetazonosító megtartása.
- **26.11a kontra tbblue `master` (`3a892b03`):** 1310 fájl bájtra azonos, 10 eltér, 11 csak a képen (AlpineNext), 181 csak a `master`-ben. Pontos 26.11a-s kártyához a hivatalos kép kell. Viktor klónozta a tbblue-t a `C:\Dev\tbblue`-ba.

## Nagyítás (1.5x, `53dbe28`)

- A belső kép 640×256, 1x = 640×512. A nagyítás **fél lépésekben**: `set_scale_x2()`, 2 = 1x, 3 = 1.5x, 4 = 2x, 6 = 3x. `AppConfigData::window_scale_x2`.
- `startup.window_scale` szókulcs: `1|1.5|2|3`; `--scale 1.5`. Az F2 léptet: 1 -> 1.5 -> 2 -> 3 -> 1. Teszt: `window_scale_test` WS-HU-01.

## Csak a kép (Shift+F11, `c313eff`)

- `MainWindow::toggle_picture_only()` / `apply_picture_only()`, Nézet > Csak a kép (`pictureOnlyAction`). Elrejti a menüsort, az eszköztárakat és az állapotsort; rejtett menüsornál a gyorsbillentyűs menüpontokat az ablakhoz is hozzáadja. Teszt: `window_scale_test` WS-HU-02; MA-05 / MA-HU-05; DKH-01.

## Upstream beolvasztások

- **1.3.34 (`6ee9600`):** hibakereső Graphics fül, bővített töréspontok, böngészős demóoldal stb. Tanulság: menüpont- vagy gyorsbillentyű-változás után a teljes `run-unit-tests.sh`.
- **1.3.43 (`62485f8`, 2026-11-21):** gyors indítás képernyő nélkül (`startup.fast_boot_hide_screen`), hibajelentés, dokkolható hibakereső-panelek (Ablak > Panelek, Elrendezés visszaállítása), hívási verem jelölése, remote-run eszköz. Ütközések: `config_keys.h`, `preferences_dialog`, `app_config.cpp` (a nyelvválasztó és az ablakméret mentése megmaradt), a dokumentációk, a mátrix. A saját AC-102/103 sorok upstreamütközés miatt AC-HU-01..03 lettek; DACC-HU-05 16/69-re igazítva; 13 új fordítás (997 üzenet, 0 fordítatlan).

## Hibakereső magyarítása (`77f57b4`)

- 616 új fordítás. A töréspontok Típus oszlopa megjelenítéskor fordul (`BreakpointModel`), a `.jds`-fájl szava angol marad. Teszt: `debugger_accel_test` DACC-HU-01..05 (13 sor). Részletek: `doc/hu/jnext-magyaritas.md`.

## Windows-fordítás Viktor gépén (`tools/hu/build-windows-cross.sh`)

- Első futás (beállítás, idempotens, a `~/jnext-win-env` alá): apt-csomagok, Qt 6.8.3 aqt-vel, zlib / libpng / SDL forrásból; MinGW GCC ≥ 13. Utána `git pull --ff-only`, fordítás `-j$(nproc)`, `-DJNEXT_WERROR=OFF -DJNEXT_DEFAULT_DISTRO_URL=<26.11a>`, csomagolás, licencek, SD-kép-őr, zip. Kimenet: `C:\Dev\jnext-bin\jnext\jnext.exe` + `jnext-<verzió>-hu-windows-x64-<hash>.zip`. Kapcsolók: `nopull`, `clean`.
- Viktor klónja a WSL-ben: `~/jnext` (ugyanitt fut a helyi Claude Code is).
- A csomagoló a `/usr/x86_64-w64-mingw32/{bin,lib}` DLL-jeit is felveszi (`6b921ce`).
- Device bridge (a felhős beszélgetésből): Viktor engedélyezte a `C:\Dev` mappát (a Fuse-fork forrása a `C:\Dev\fuse`-ban, pl. `hungarian_keyboard.c`).

## Windows-zip a felhőben (működik, Viktor kipróbálta)

Engedélyezett webhelyek: `download.qt.io`, `master.qt.io`, `specnext.com`, `gitlab.com`. Az aqt-t mindig `--base https://master.qt.io/`-val kell hívni. Lépések: aqtinstall + MinGW (posix) csomagok; Qt 6.8.3 `win64_mingw` és `linux_gcc_64`; toolchain-fájl `/home/claude/mingw64-toolchain.cmake`; zlib/libpng/SDL forrásból a `/home/claude/winsys`-be; `cmake -S . -B /home/claude/jnext-build-win … -DENABLE_QT_UI=ON -DENABLE_TESTS=OFF`, `--target jnext`; csomagolás a `tools/hu/build-windows-cross.sh`-val (`JNEXT_WIN_ENV=/home/claude/wenv`, `nopull`). A wine ott összeomlik; a valódi próbát Viktor végzi.

## Következő lépések

1. **GitHub-tesztek eredménye:** teljes CI `44942749295` és `hu-tests.yml` `44942439407` (mindkettő `5b20d8e`). Hibánál a `gh run view <id> -R Viktor318/jnext --log-failed`. Döntés: kell-e még a `hu-tests.yml`.
2. **Windowsos magyar billentyűzet** (kívánságlista 2.) – „holnap kezdjük” (Viktor, 2026-11-21 este). Döntések és terv fent.
3. **DMA-időzítés** (`doc/hu/jnext-dma-meres.md`): hatásfelmérés kész, javítás csak jóváhagyás után.
4. Akadás menüzés/ablakmozgatás közben (kívánságlista 3.), majd hangkeverő és recsegés (4., `doc/hu/jnext-hang-terv.md`).
5. Viktor hibalistája (gyűjti); javítani csak jóváhagyás után.
6. Magyarítás 4. lépése: a magból jövő üzenetek (`doc/hu/jnext-magyaritas.md`).
7. Döntés a független átnézésről; a blitterről.
8. Kapcsolat a ZEsarUX-szal: a ZEsarUX blittere (`4f552285`, `d3021c55`) a JNEXT-hez minta; a DMA-modell a ZEsarUX-ban kész (`6809ec69`). Ötlet a ZEsarUX-hoz: a „Csak a kép” és az 1.5x megfelelője.
