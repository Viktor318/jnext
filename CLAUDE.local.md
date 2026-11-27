# Viktor saját JNEXT-forkja – helyi szabályok (CLAUDE.local.md)

Ez a fájl a tároló `CLAUDE.md`-je mellett érvényes, a helyi (WSL, `~/jnext`) és a felhős Claude Code-ra egyaránt. A `.gitignore` kizárja, ezért `git add -f`-fel követjük (`3a1a793`). Ahol a kettő ellentmond, **ez a fájl dönt**: a `CLAUDE.md` az eredeti fejlesztő (jorgegv) szabályait írja le az upstreamnek szánt munkára, ez pedig Viktor saját forkjára vonatkozik.

## Kivel és hogyan

- Viktorral **magyarul** beszélj. Rövid, lényegre törő válaszokat kér; ha egy diagnózis téves, ezt egyenesen megmondja.
- A tároló: `Viktor318/jnext` (upstream: `jorgegv/jnext`). Cél: **saját magyar változat** (magyar felület, windowsos magyar billentyűzet) és **hibajavítások**. Az upstreamnek visszaküldés egyelőre nem cél.
- **Javítani csak Viktor jóváhagyása után** szabad: előbb felmérés (mi a hiba, mit érintene a javítás, mekkora munka), aztán kérdés, aztán munka. A saját, épp készülő munkádban okozott hibát azonnal javíthatod.

## Git

- Munka előtt: `git pull --ff-only`. Ha nem gyors előretekerés, vagy vannak nem commitolt helyi változások: **állj meg és szólj**, ne olvassz be és ne rebase-elj magadtól.
- **Egyetlen munkaág: `hungarian-ui`.** A `main` az upstream tükre, oda ne írj. Worktree-k, ágensenként külön ágak, `epic`/`gh<N>` ágak, autonóm futtatások nem kellenek (a `CLAUDE.md` ezen részei az upstream csapatmunkájára vonatkoznak).
- **Push: csak a `hungarian-ui`-ra**, Viktor állandó engedélyével. Más ágra, címkére, force-push, PR: csak külön engedéllyel.
- A tároló hookja (`.claude/hooks/block-push.sh`) minden push-t letilt, ha nincs előtte `JNEXT_ALLOW_PUSH=1`. A `hungarian-ui` push-t ezért így futtasd: `JNEXT_ALLOW_PUSH=1 git push origin hungarian-ui`. Az előtag csak a hookon enged át, engedélyt nem ad: más ágra, címkére, force-pushra és PR-re továbbra is külön engedély kell.
- Upstream beolvasztása (`main` → `hungarian-ui`) csak Viktor kérésére. Merge közben soha ne `git stash`.
- **Commitüzenet magyarul**, „Terület: leírás” formában, záró pont nélkül (pl. „Hang: alkalmazkodó puffer a recsegés ellen”), a törzs is magyarul, tömören. A `Co-Authored-By` sor maradhat (a `CLAUDE.md` tiltása az upstreamnek szól). Upstreamnek szánt commit angol – előtte kérdezz.
- A `make bump-patch` / verziócímke-szabály (a `CLAUDE.md` merge-protokolljának 5. pontja) itt **nem kell**; a ChangeLog és a FEATURES.md csak Viktor kérésére frissül.

## Ami a `CLAUDE.md`-ből érvényes marad

- **NextZXOS SD-kártyakép soha nem kerülhet a tárolóba vagy a GitHubra** (se commit, se melléklet, se zip).
- A VHDL a mérvadó; ahol Viktor KS3-án mért adat van, az is bizonyíték (lent).
- Változáshoz teszt; a SKIP is hiba; a tesztszámok a `test/unit-tests.conf`-ban rögzítettek (sorszám változásakor a manifesztet is igazítani).
- Minden Beállítások-mezőnek legyen `--set` kulcsa és man page-bejegyzése (`make cli-check`).
- Új beállításkulcsnál a `src/platform/bug_report.cpp` `config_value_rule()` kapcsolójába és a `bug_report_test` várt listájába is fel kell venni. `JNEXT_WERROR=OFF` mellett a hiány csak figyelmeztetés, a CI-ban fordítási hiba (`5b20d8e`).
- Minden külső folyamat `LANG=C` alatt fut.
- Az 1.3.57 óta a `make unit-test` része a `test-affected-check` (GH #547): minden követett fájlnak vagy fordítási függőségnek, vagy egy `test/test-affected.conf`-szabálynak kell lennie. Új fájltípusnál a szabályt ugyanabban a commitban kell felvenni (a magyar `.ts`/`.qm` fájlok: `full`). Helyben a `run-unit-tests.sh` ezt nem futtatja: `perl test/test-affected.pl --check --build <fordítási fa>` külön kell.
- A generált dokumentumok (man page, user guide, developer guide, `TRACEABILITY-MATRIX.md`) commitolva vannak és frissességre ellenőrzöttek: a forrást kell szerkeszteni, újragenerálni, mindkettőt commitolni.
- A felületen soha ne felirat szerint keress (a fordítás elrontja), hanem objektumnév szerint.
- Független átnézés: Viktor még nem döntött, hogy külön átnéző ágens kell-e; addig a saját, tesztekkel alátámasztott ellenőrzés elég, de a változtatás után a teljes `test/run-unit-tests.sh` fusson (menü- vagy gyorsbillentyű-változásnál mindenképp).

## Fordítás és tesztelés ezen a gépen

- **A WSL 6 szálra van korlátozva** (Viktor, 2026-11-21: `%USERPROFILE%\.wslconfig`, `[wsl2]` `processors=6`), mert a 20 szálas fordítás a Windowst is lefoglalta. Az `nproc` így 6-ot ad, a `-j$(nproc)` és a tesztek alapértéke rendben van; ennél több párhuzamos munkát ne indíts.
- Windows-változat: `~/jnext/tools/hu/build-windows-cross.sh` (kapcsolók: `nopull`, `clean`; kimenet `C:\Dev\jnext-bin\jnext\jnext.exe` + zip). Pár perc.
- Linuxos tesztfordításhoz (Qt6, SDL3, ccache stb.) előbb mérd fel, mi van telepítve a WSL-ben (Ubuntu 26.04), és a hiányzó csomagokat Viktorral egyeztesd (sudo-t kér).
- A GitHub Actions is használható (Viktor engedélyezte): `gh workflow run "Windows Build (manual)" -R Viktor318/jnext --ref hungarian-ui -f qt6=true -f qt5=false -f win32=false`, és a teljes teszt-CI: `gh workflow run CI -R Viktor318/jnext --ref hungarian-ui`.
- SD-kép: Viktor a 26.11a-t használja (`JNEXT_SDCARD_DISTRO_URL` = `https://www.specnext.com/distro/26.11a/sn-emulator-26.11a.zip`). A jnext írja a képet: kézi próbához másolatot használj. A WSL-es `~/.jnext/sdcard/cspect-next-1gb-fixed.img`-et Viktor kézzel módosította, **ne írd felül**; a helyi tesztekhez az eredeti kártya másolata kell: `JNEXT_TEST_SD_IMAGE=~/.cache/jnext-hu-tools/test-home/copy.img`.

## Ahol tartunk (2026-11-27)

Alap: upstream 1.4.8 (2026-11-27). Kész a `hungarian-ui`-n:

- magyar felület: főablak, menük, Beállítások, párbeszédablakok (`1532a59c5`), nyelvválasztás (`17f3d198a`), hibakereső (`77f57b40f`);
- 1.5x nagyítás (`53dbe28`);
- Csak a kép / Shift+F11 (`c313eff`);
- Windows-fordítás WSL-ből: `tools/hu/build-windows-cross.sh` (`a109030f6`);
- a NextZXOS 26.11a az alapértelmezett letöltés (előbb saját beállítással, `a4ffecba6`; az 1.3.57 óta az upstreamben is);
- upstream beolvasztva: 1.4.8 (2026-11-27; előtte 1.4.1, 1.3.78, 1.3.72, 1.3.66, 1.3.57, 1.3.43);
- kézi GitHubos unit-teszt: `hu-tests.yml` (`77a02cb06`);
- windowsos magyar billentyűzet (karakter szerinti gépelés, bal Alt + számjegy; `1263eb4`, `c743724`, `3f109af`), Viktor Windowson kipróbálta (2026-11-22);
- a címsorra kattintás nem akasztja az emulációt (Windows, `gui/window_stalls.h`);
- DMA-időzítés a KS3 mérése szerint (`6cf89c6`, `188907b`, `e5b2fef`): 3,5 MHz-en pontosan a KS3-é, 28 MHz-en 1 soron belül;
- az NXModPlayer recsegése: két IM2 RETI-hiba javítva (`6f4a015`), Viktor kipróbálta: megszűnt;
- 4×-es túlmintavétel (`audio.oversample`, alapból ki; `doc/hu/jnext-hang-terv.md` vége);
- a Beállítások Alkalmaz gombja csak változás után aktív (`1c2ad00`);
- DAC-keverő (szóló, panoráma) és AY sztereó felülírása, élő hang a Beállítások > Hang lapon (`doc/hu/jnext-hang-terv.md` vége).

Következik: a **hangterv felülvizsgált sorrendje Viktor döntésére vár** (`doc/hu/jnext-hang-terv.md` vége: előbb mérés az ő gépén, aztán állítható hangpuffer – ez a párbeszédablakok alatti akadásra is –, „tévé” szűrő; a mérés és a jobb mintavételezés kész). Felmérésre vár: az átméretezhető ablak (7.), a virtuális billentyűzet (5.), az állapotmentés kilépéskor (6.). A blitter (3.03-as core) egyelőre nem kell (Viktor, 2026-11-21).

A részletek (döntések, tervek, a további kívánságlista, hang, DMA) a tárolóban vannak: **`doc/hu/jnext-atadas.md`** (és mellette `jnext-magyaritas.md`, `jnext-dma-meres.md`, `jnext-hang-terv.md`, `jnext-blitter-becsles.md`). Ezeket munka közben olvasd és frissítsd; ha új döntés vagy terv születik, oda írd be, ugyanabban a commitban. A ZEsarUX-beszélgetésben készült JNEXT-jegyzetek a claude.ai-projektbe kerülhetnek; ha Viktor ilyet hoz, vezesd át ide.
