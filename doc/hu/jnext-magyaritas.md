# JNEXT: a felület magyarítása (2026-11-20, frissítve 2026-11-21)

(Eredetileg a claude.ai-projekt `claude/jnext-magyaritas.md` dokumentuma; 2026-11-21 óta a tárolóban él.)

Viktor döntései (2026-11-20 éjjel): a nyelv a **Beállításokban** választható, alapból **Automatikus** (a rendszer nyelve); a **hibakereső is** magyar lesz; a magból jövő üzenetek (állapotsor, hibaüzenetek, SD-kártya letöltés/csere) **a végén**, negyedik lépésként.

## Állapot

| Lépés | Állapot |
|---|---|
| 1. Infrastruktúra + Nyelv beállítás | **kész** – `17f3d19` |
| 2. Főablak, menük, Beállítások, párbeszédablakok | **kész** – `1532a59`, `f49f418` (rcc zstd-javítás), `1436e18` (javítás: a hibakereső a magyar felületen nem nyílt meg a menüből – lent; az FPS-felirat rövidebb: „gép / kép”). Viktor (2026-11-20): „Jó lett.” |
| 3. Hibakereső | **kész** – `77f57b4` (2026-11-21): 616 új fordítás (DebuggerWindow 136, QObject 83 – főleg a hibakereső párbeszédablakai –, SourcePanel 64, MemoryPanel 60, BreakpointPanel 45, ScriptPanel 42, GraphicsPanel 24, WatchPanel 21, BreakpointModel 20, DisasmPanel 19, AudioPanel 15, VideoPanel 15, CpuPanel 14, VariablesPanel 12, SymbolsPanel 11, MmuPanel 9, CallStackPanel 6, CopperPanel 5, OperandMenu 5, StackPanel 5, VideoLayerView 5). Viktor átnézte (`jnext-hibakereso-forditas.xlsx`): „a fordítások jók lesznek”. Az 1.3.43-as beolvasztással (`62485f8`) +13 fordítás (Panelek, Elrendezés visszaállítása, hívási verem, hibajelentés, gyors indítás képernyő nélkül); a katalógusban 997 üzenet, nincs fordítatlan. |
| 4. A magból jövő üzenetek (`fromStdString`, kb. 150 hely; a magban kell egy kis fordítási réteg) | **hátravan** |

## Hogyan működik

- Beállítás: `startup.language` = `auto|en|hu` (`src/platform/config_keys.h`), Beállítások > Indítás > Nyelv, `--set startup.language=hu`. **Indításkor** lép életbe (mint a Fast boot). A hibajelentésbe sima értékként kerül (`src/platform/bug_report.cpp`, `5b20d8e`).
- `src/gui/ui_language.{h,cpp}`: `resolve()` (Auto = a `QLocale::system().uiLanguages()` első eleme hu-val kezdődik-e), `set_session_choice()` (main.cpp, a `merge_cli_assignments` után), `install(app)` minden QApplication után (qt_app.cpp és az SD-letöltés ideiglenes QApplicationje).
- Fordítások: `src/gui/translations/jnext_hu.ts` (forrás, `-locations none`), `jnext_hu.qm` (commitolva), `qtbase_hu.qm` (a Qt 6.8.3 saját katalógusa: Mégse, Alkalmaz, fájlválasztó), beágyazva: `jnext_i18n.qrc`, AUTORCC `--no-zstd`-vel (a keresztfordításnál a gazda-rcc zstd-je nem linkelődik a MinGW-s Qt-hoz).
- Frissítés: `LUPDATE=… LRELEASE=… tools/hu/update-translations.sh` (mindkét változó kell, különben a szkript kilép; a felhőben a `/home/claude/qt/6.8.3/gcc_64/bin/`-ből). Kitöltés: Claude egy JSON-ból tölti (`{"Kontextus": {"forrás": "fordítás"}}`; `%n`-es szövegnél egyelemű lista), a helper a felhős munkakörnyezetben `/home/claude/tsfill.py` (újra kell írni, ha elveszett).
- A `tr(változó)` szövegeket az lupdate nem látja: `QT_TR_NOOP` / `QT_TRANSLATE_NOOP("kontextus", …)` kell a definíciónál (így lett a sebességmenü, a `host_shortcuts.h`, az MMU-fejlécek, a figyelőtípusok, a Grafika palettái és a Videó fülei). Szabad függvényben: `QCoreApplication::translate("Kontextus", …)` (OperandMenu, GraphicsPanel, BreakpointModel).
- Az lupdate a `watch_panel.cpp`-re és a `source_panel.cpp`-re „Qualifying with unknown namespace/class” figyelmeztetést ad – ártalmatlan (a kontextus helyes).

## Tanulság: szöveg szerinti keresés

- A `DebuggerManager` a főablak menüjében a „Debugger” szót kereste a menüpont szövegében; magyarul („&Hibakereső”) nem találta, és a menüpont nem nyitotta meg a hibakeresőt (Viktor jelezte, `1436e18` javította: objektumnév `debuggerToggleAction`, teszt `debugger_menu_test` GH215-08, mutációval ellenőrizve).
- A 3. lépésben átnézve: a `src/debugger` és a `src/gui` kódjában nincs több felirat szerinti keresés (csak a Szimbólumok szűrője, ami tartalomra szűr, és a Memória `currentText()`-je, ami csak üzenetbe kerül). A tesztekben van ilyen (pl. `menu_named(bar, "Debug")`, `box_value(&panel, "Flags: ")`) – azok angol felületen futnak, nem baj.
- Ha a fordított szöveg adatként kerülne fájlba (pl. a töréspont típusa a `.jds`-be), azt nem szabad fordítani: a töréspontok Típus oszlopa csak megjelenítéskor fordul, a `bp_kind_name()` angol marad.

## Döntések a szóhasználatban (a projekt `zx-szojegyzek.md`-je mellett)

- Power Reset → Hidegindítás, Soft Reset → Újraindítás; Debug menü → Hibakeresés, Debugger → Hibakereső; Settings és Preferences → Beállítások; Tape → Kazetta; Scanlines → pásztázósorok; Trainer → trainer (POK-trainerek); host → számítógép/gép; az FPS-felirat „FPS: %1 gép / %2 kép”.
- Hibakereső: Step Into → Belépés (a Debug menüben a Single Step → Lépés), Step Over → Átlépés, Step Out → Kilépés a rutinból (gomb: Ki a rutinból), Step Back → Lépés hátra, Frame Back → Képkocka vissza, Run to Cursor/Here → Futtatás a kurzorig / idáig, Reverse Continue → Folytatás visszafelé, Breakpoint → Töréspont, Watch → Figyelő, Trace → Nyomkövetés, Coverage → Lefedettség, Rewind → Visszatekerés, Disassembly → Visszafordítás, Stack → Verem, Call Stack → Hívási verem, Source Map → Forrástérkép, Map (menü) → Térkép, Annotations → Megjegyzések, Label → Címke, Symbols → Szimbólumok, Slot → slot, Page → lap, Bank → bank, Shadow → árnyék, Tiles → Csempék, Patterns → Minták, Interrupt → Megszakítás, Hits → Találat, Condition → Feltétel, Script → Szkript, Graphics → Grafika, Beeper → Csipogó, T-states → T-ciklus, Panels → Panelek, Reset Layout → Elrendezés visszaállítása. Angolul maradt: NextREG-nevek, VHDL-jelnevek, sprite-oszloprövidítések, PASS/FAIL/ERROR (a szkript ítéletei), NMI, DMA, Copper, MMU, Layer 2, Tilemap.
- Számok: a „3.5 MHz”, „0.5x (50%)”, „%1x” változatlan (a mag és a Qt-felület több helyen ugyanezt a szöveget állítja elő, vegyes alak lenne).
- Személytelen/magázó forma a felszólításokban („Telepítse…”, „Kattintson…”).

## Gyorsbetűk (mnemonikok)

- Főablak menüsora magyarul: &Fájl, Gép (nincs gyorsbetű), Be&menet, K&azetta, &Hibakeresés, &Nézet, &Beállítások, Súgó (nincs). Ok: az Alt+E/G/C a vendégé (ZX EXTEND/GRAPH/CAPS), és a menüsor betűje nem lehet QAction-gyorsbillentyű (Alt+O/L/S/K/Q/P/U/D/T foglalt).
- Hibakereső menüsora magyarul: &Fájl, &Hibakeresés, &Ugrás, &Szkript, &Térkép, Tö&réspontok, Fi&gyelők, &Ablak (benne 1.3.43 óta &Panelek és Elrendezés &visszaállítása; a hibakereső Alt-gyorsbillentyűi: Alt+L, Alt+1..8, Alt+Bal/Jobb). A Hibakeresés menü: Futtatás / &folytatás, &Szünet / megállítás, &Lépés, Á&tlépés, &Kilépés a rutinból, |< Képkocka viss&za, Lépés &hátra, Futtatás a képkocka &végéig, Futtatás a so&r végéig, Futtatás a következő &megszakításig, F&orrás, &Nyomkövetés, Lefe&dettség, Fizikai &címek, Visszat&ekerés.
- Teszt: `main_window_accel_test` MA-HU-01..05 és `debugger_accel_test` DACC-HU-01..05 – a magyar katalógussal is egyediek a menük gyorsbetűi, és nem ütköznek; a menüfa alakja (és a gyorsbetűk száma) azonos az angollal (1.3.43 óta a hibakeresőben 8 menü, 16 almenü, 69 gyorsbetű, 28 gyorsbillentyű). Ha Viktor átír egy menüpontot, a gyorsbetűt is ellenőrizni kell (a teszt elkapja).

## Tesztek és dokumentumok

- `app_config_test` AC-HU-01..03 (az upstream AC-102/103-mal ütközés miatt átnevezve, `62485f8`), `config_keys_test` SETK-10 (8 Word-kulcs), `main_window_accel_test` MA-HU-01..05 (10 sor), `debugger_menu_test` GH215-08 (76 sor), `debugger_accel_test` DACC-HU-01..05 (13 sor), `bug_report_test` (a `startup.language` a várt listában).
- Dokumentumok: man page forrás + `jnext.1` + `USAGE.md` + a user guide függeléke; a felhőben kézzel (pandoc 3.7.0.2 ott nincs; a roff-ban az aposztróf `\(cq`, a markdownban `’`), a user guide HTML-je mkdocs 1.6.1 + mkdocs-material 9.7.1 + **Markdown 3.10**-zel (pip venv: `/home/claude/mkdocs-venv`) – ezzel a `make docs-userguide-check` bájtra egyezik. A FEATURES.md nyelv-sora a hibakeresőt is említi.
