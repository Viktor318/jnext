# JNEXT: hol tart a Next-emulációban, és a blitter (zxnDDMA) becslése (2026-11-19)

Csak felmérés és becslés, kód nem készült. **Döntés (Viktor, 2026-11-21): a 3.03-as core és a blitter egyelőre nem kerül a JNEXT-be** – túl kockázatos a haszonhoz képest; a blitteres programok a ZEsarUX-ban és Viktor KS3-án futnak. Párja a ZEsarUX-oldalon: a claude.ai-projekt `claude/zesarux-next-2611-terv.md` dokumentuma. (Eredetileg a projekt `claude/jnext-blitter-becsles.md` dokumentuma; 2026-11-21 óta a tárolóban él.)

## Melyik core-t emulálja a JNEXT

- **3.02.03-at, szándékosan.** NR 0x01 = 0x32, NR 0x0E = 0x03 (`src/port/nextreg.cpp`), a VHDL-hivatkozások a hivatalos FPGA-tároló `master` ágára (3.02.03, 2025-11-06) mutatnak. A felhasználói kézikönyv is így írja: a 26.11 `TBBLUE.TBU`-jában lévő újabb core nincs emulálva.
- **A nyilvános FPGA-forrásban van újabb is:** `30204` (3.02.04, 2026-05-28) és `30205` (3.02.05, utolsó commit 2026-11-16, HDMI v3). A JNEXT-nek van róla saját elemzése (`doc/analysis/FPGA-CORE-3.02.04-PI-DEBUG-CONTROL.md`, upstream GH #393, #394), de **nem vette át**: a Pi-s hibakereső SPI-slave csak issue 4/5-ön van, a többi apróság (PS/2 F5/F6/F12, hatgombos MD-pad azonosítása, NR 0x0A bit 2/5 sd-csere, új boot ROM, NR 0x11 HDMI v3 módok) sincs meg.
- **A 3.03 (26.11: 3.03.00, 26.11a: 3.03.03) forrása nem nyilvános.** A tbblue tároló `CHANGELOG`-ja szerint a 3.03.00 két dolga: **„28MHz 0 WS”** és a **zxnDDMA**. A tbblue `docs/extra-hw/io-port-system/registers.txt` régi (2023, v2.4), nem a 3.03-at írja le; a DDMA egyetlen leírása a `docs/zxnDDMA/zxnddma.gde` + 11 példa.
- **Blitter: nincs.** A teljes forrásban nincs DDMA-ra utaló kód. A DMA (`src/peripheral/dma.cpp`, 1124 sor) a régi zxnDMA/Z80-DMA a 0x6B/0x0B porton, VHDL-hű időzítéssel: burst/folytonos mód, prescaler, a busz elengedése burst-várakozáskor (`dma.vhd:441-449`), 28 MHz-es SRAM-olvasási várakozás (GH #106), sínarbitráció. Teszt: `test/dma/dma_test.cpp` (3984 sor, 186 sor a manifestben), terv: `doc/testing/DMA-TEST-PLAN-DESIGN.md`. (A bájtonkénti időzítés KS3-méréssel összevetve: `doc/hu/jnext-dma-meres.md`.)
- **28 MHz várakozás:** a JNEXT a 3.02-es viselkedést emulálja (28 MHz-en SRAM-olvasási várakozás, `Mmu::sram_read_wait28`); a 3.03 ezt elhagyja.

## A blitter becslése

Alap: a ZEsarUX-ban az első változat + javítás egy hosszabb esti munkamenet volt (`4f552285`, `d3021c55`). A JNEXT-ben több a munka, mert (1) a DMA időzít, így a blitternek is kell, (2) a tároló szabályai szigorúak (tesztterv, egy sor = egy azonosító, traceability, mentett állapot, dokumentáció), (3) a VHDL nem elérhető, a leírás az orákulum.

| Rész | Tartalom | Becslés |
|---|---|---|
| 1. Parancsok és állapot | `$D7/$DB` 24 bites címek, `$DF` (fizikai, 2D, átlátszó, kitöltés, Layer 2 X,Y 256×192 és 320×256, rejtett képernyő NR 0x13), `$E3/$E7/$EB/$EF`, `$F3`, `$F7`, `$C3`, `$CF` újratöltés; csak a 0x6B-n (a 0x0B-n a parancsbájt elvész) | 1 munkamenet |
| 2. Fizikai memóriaút | a mostani DMA a CPU 16 bites címterén át ír (`mmu_.read/write`); a DDMA-nak fizikai SRAM-hozzáférés kell (2 MB, körbefordulás), és ügyelni kell a ULA/tilemap által olvasott 5-ös/7-es bankra (a ZEsarUX-ban ez hibás lett) | 0,5–1 |
| 3. Időzítés | a leírás szerint 3+3 órajel/bájt, kitöltés 3, prescaler; beillesztés a meglévő burst/folytonos és buszfoglalási modellbe | 1 |
| 4. Core-verzió 3.03.03 | NR 0x01/0x0E, a `nex_loader` verzióellenőrzése, a kézikönyv két bekezdése; a firmware indítóképe más verziót ír, ezért **regressziós referencia-képek** változhatnak | 0,5 |
| 5. Tesztek és járulékok | új tervfejezet a `DMA-TEST-PLAN-DESIGN.md`-ben (a `zxnddma.gde`-re hivatkozva), kb. 40–60 új sor, mentett állapot (`save_state`/`describe_state`), hibakereső-kijelzés, FEATURES.md, user/developer guide | 1–2 |
| 6. Kipróbálás | a 9 példaprogram (`.nex`, sjasmplus-szal itt is fordítható), összevetés a ZEsarUX-szal; **Viktor KS3-a (issue 5) a 26.11a-val valódi 3.03.03-at futtat**, így a `zxnddma_compare` mérése a valódi gépen az időzítés orákuluma lehet | 0,5 |

**Összesen: kb. 4–6 munkamenet.** Ha az első lépcső csak a működés (1, 2, 4, kipróbálás, alapszintű tesztek) és az időzítés második lépcsőben jön: az első lépcső kb. 2–3 munkamenet.

**Külön tétel, nem a blitter része:** a 3.03-as várakozás nélküli 28 MHz. Ez a CPU és a DMA időzítését is érinti; ha csak a 3.03.03-as módban él, kapcsoló kell a két viselkedés közé. Kb. 1 munkamenet, és sok meglévő időzítési teszt érintett lehet.

## Kockázatok, korlátok

- **Orákulum:** nincs VHDL, a leírás és a valódi gép a mérvadó; a tervsorok a `zxnddma.gde` fejezeteire hivatkoznak (a traceability-generátor VHDL-hivatkozást vár – ezt meg kell nézni, lehet, hogy kivétel kell a `test/traceability-exceptions.conf`-ba).
- **A teljes kapu a felhőben nem fut** (wine, pandoc/mkdocs-verzió); Viktor helyi gépén vagy a fork GitHub Actions CI-jában (`gh workflow run CI -R Viktor318/jnext --ref hungarian-ui`, Viktor engedélyezte) igen.
- **A példaprogramok licence** nem tisztázott, ezért tesztként nem kerülhetnek a tárolóba; saját, kicsi tesztesetek kellenek.
