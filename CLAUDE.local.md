# Viktor saját JNEXT-forkja – helyi szabályok (CLAUDE.local.md)

Ez a fájl a `~/jnext`-ben (WSL) a tároló `CLAUDE.md`-je mellett érvényes, és nem kerül a tárolóba. Ahol a kettő ellentmond, **ez a fájl dönt**: a `CLAUDE.md` az eredeti fejlesztő (jorgegv) szabályait írja le az upstreamnek szánt munkára, ez pedig Viktor saját forkjára vonatkozik.

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
- Minden külső folyamat `LANG=C` alatt fut.
- A generált dokumentumok (man page, user guide, developer guide, `TRACEABILITY-MATRIX.md`) commitolva vannak és frissességre ellenőrzöttek: a forrást kell szerkeszteni, újragenerálni, mindkettőt commitolni.
- A felületen soha ne felirat szerint keress (a fordítás elrontja), hanem objektumnév szerint.
- Független átnézés: Viktor még nem döntött, hogy külön átnéző ágens kell-e; addig a saját, tesztekkel alátámasztott ellenőrzés elég, de a változtatás után a teljes `test/run-unit-tests.sh` fusson (menü- vagy gyorsbillentyű-változásnál mindenképp).

## Fordítás és tesztelés ezen a gépen

- Windows-változat: `~/jnext/tools/hu/build-windows-cross.sh` (kapcsolók: `nopull`, `clean`; kimenet `C:\Dev\jnext-bin\jnext\jnext.exe` + zip). Pár perc.
- Linuxos tesztfordításhoz (Qt6, SDL3, ccache stb.) előbb mérd fel, mi van telepítve a WSL-ben (Ubuntu 26.04), és a hiányzó csomagokat Viktorral egyeztesd (sudo-t kér).
- A GitHub Actions is használható (Viktor engedélyezte): `gh workflow run "Windows Build (manual)" -R Viktor318/jnext --ref hungarian-ui -f qt6=true -f qt5=false -f win32=false`, és a teljes teszt-CI: `gh workflow run CI -R Viktor318/jnext --ref hungarian-ui`.
- SD-kép: Viktor a 26.11a-t használja (`JNEXT_SDCARD_DISTRO_URL` = `https://www.specnext.com/distro/26.11a/sn-emulator-26.11a.zip`). A jnext írja a képet: kézi próbához másolatot használj.

## Ahol tartunk (2026-11-21)

Teteje: `5b20d8e`. Kész a `hungarian-ui`-n:

- magyar felület: főablak, menük, Beállítások, párbeszédablakok (`1532a59c5`), nyelvválasztás (`17f3d198a`), hibakereső (`77f57b40f`);
- 1.5x nagyítás (`53dbe28`);
- Csak a kép / Shift+F11 (`c313eff`);
- Windows-fordítás WSL-ből: `tools/hu/build-windows-cross.sh` (`a109030f6`);
- a magyar Windows-build alapból a NextZXOS 26.11a-t tölti le (`a4ffecba6`);
- upstream beolvasztva: 1.3.43 (`62485f8`);
- kézi GitHubos unit-teszt: `hu-tests.yml` (`77a02cb06`).

Következő feladat: windowsos magyar billentyűzet (előbb felmérés, aztán Viktor jóváhagyása).

A részletek (döntések, tervek, a további kívánságlista, hang, DMA) a claude.ai „ZX Spectrum emulátorok továbbfejlesztése” projekt `jnext-atadas.md` dokumentumában vannak; innen nem éred el, ha kell, kérd el Viktortól.
