# DMA-időzítés: KS3-mérés és ami ebből a JNEXT-re vonatkozik (2026-11-21)

A ZEsarUX-beszélgetésben készült; a JNEXT-munkának szól. A JNEXT-javítás 2026-11-22-én elkészült (lent: „Megvalósítás”). (Eredetileg a claude.ai-projekt `claude/jnext-dma-meres.md` dokumentuma; 2026-11-21 óta a tárolóban él.)

## A mérés

- Tesztprogram: `dmatest.nex` – forrás a ZEsarUX forkban: `src/tests/next/dmatest/` (`dmatest.asm`, `font.asm`, `README.md` a teljes táblázattal; sjasmplus `savenex`). A zxnDMA idejét a képsorszámlálóval (NR 0x1E/0x1F) méri 3,5 és 28 MHz-en, és kiírja a processzor várakozó ciklusának köreit is.
- Viktor KS3-án mérve: core 3.02.04 (60 Hz), 3.03.03 (60 Hz és 50 Hz). A DMA a két core-on gyakorlatilag azonos; a 3.03-ban 28 MHz-en a processzor löketes módban több kört tesz (nincs memória-várakozás a CPU oldalán).

| (3,5 MHz / 28 MHz, sor; zárójelben körök) | KS3 3.03.03, 50 Hz | JNEXT (`77f57b4` körüli állapot, SDL-build) |
|---|---|---|
| A2: folyamatos, előosztó nélkül, 2 ciklus, 4K / 16K | 73 / 45 | 37 / 27 |
| A3: ugyanez, 3 ciklus | 109 / 54 | 37 / 27 |
| A4: ugyanez, 4 ciklus | 145 / 72 | 38 / 27 |
| B8: folyamatos, előosztó 8, 512 B | 80 / 72 | 74 / 72 |
| C8: löketes, előosztó 8, 512 B | 90 (342) / 74 (2940) | 80 (385) / 73 (2429) |
| D2: löketes, előosztó 2, 4096 B | 280 (513) / 160 (5632) | 196 (820) / 152 (4915) |
| E: hang, löketes, előosztó 55, 16K → $DF (képkocka) | 52 | 51 (50 Hz) |

## Mit mutat (a valódi gép viselkedése)

1. **Előosztó nélkül bájtonként az olvasás + írás ciklushossza** a WR1/WR2 időzítési bájtja szerint (00 → 4, 01 → 3, 10 → 2 órajel): 2+2 = 4, 3+3 = 6, 4+4 = 8 T bájtonként (mért 3,96 / 5,92 / 7,88). **28 MHz-en a 2 ciklusos memóriaolvasás 3** (4,89 / 5,87 / 7,82). A JNEXT bájtonként fix 2 T-t számol (`Dma::CHARGED_TSTATES_PER_BYTE = 2`) – ez a mérés szerint hibás; a JNEXT saját `timing_to_cycles` / `read_cycles()` / `write_cycles()` függvényei (dma.vhd:313-316, :365-368) már megvannak, csak nem ezek adják a felszámolt időt. A MAME (z80dma alapú) a ciklushosszt használja.
2. **Előosztó:** bájtonként legalább előosztó × 32 db 28 MHz-es órajel, a processzor sebességétől függetlenül – ez a JNEXT-ben jó (`tick_burst_wait`). 3,5 MHz-en a várakozás után még kb. 3 órajel (folyamatos mód) ill. kb. 7 órajel (löketes mód: a sín visszaszerzése az M-ciklus végén) jön.
3. **Löketes módban** a processzor kb. 75–81% (3,5 MHz, C8), ill. 36% (D2) idejét kapja.
4. **Hang (E):** 875 kHz / 55 ≈ 15,9 kHz → 16384 minta ≈ 1,03–1,06 s; a KS3-on 52 képkocka (50 Hz).

## Javaslat a JNEXT-hez (csak Viktor jóváhagyása után)

- A felszámolt idő bájtonként `read_cycles() + write_cycles()` legyen (a 28 MHz-es SRAM-várakozással: `last_burst_read_wait_tstates`), a fix 2 helyett. Ez a VHDL-lel összhangban van (READ_2/3/4, WRITE_2/3/4 állapotok), tehát a tároló „VHDL a mérvadó” szabályát nem sérti; a KS3-mérést hivatkozásként felírni.
- Viktor eldönti, hogy ez a saját forkban marad, vagy issue-ként az upstreamnek (jorgegv/jnext) is jelzi a mérésekkel.

## Hatásfelmérés (Claude, 2026-11-21 este, a felhőben)

- **Kód:** `Dma::CHARGED_TSTATES_PER_BYTE` (`src/peripheral/dma.h`) és két felhasználója az `src/core/emulator.cpp`-ben:
  - a DMA-löket órajel-felszámolása (`master_cycles = (transferred * CHARGED_TSTATES_PER_BYTE + last_burst_read_wait_tstates()) * cpu_divisor()`);
  - a `port_fe_write_time()`: a DMA-val írt 0xFE port időbélyege a kazettakimenethez (ugyanazt az idővonalat követi).
- **Tesztek:** `test/dma/dma_test.cpp` 23.1–23.3 a 8 bájtos löket idejét rögzíti: most 24 / 16 / 16 T (28 MHz SRAM-várakozással / 14 MHz / 28 MHz bank 7). Az új modellben, ha az alapértelmezett időzítés 2+2 ciklus, kb. 40 / 32 / 32 T – a reset utáni időzítési bájtot a VHDL-ből ellenőrizni kell. A `doc/testing/DMA-TEST-PLAN-DESIGN.md` megfelelő sorai, és a DMA-t használó regressziós sorok (képernyőképek, hangos sorok) is érintettek lehetnek, mert eltolódhat bennük az időzítés.
- Kezdés csak Viktor jóváhagyása után.

## A ZEsarUX-ban

Kész: `6809ec69` (hungarian-ui) – a fenti modell; a ZEsarUX a KS3-hoz 1–3%-on belül mér (3,5 MHz-en kb. 2 sor állandó eltéréssel, ami a sorszámláló visszaolvasásából jön, nem a DMA-ból).

## Megvalósítás (2026-11-22, Viktor jóváhagyásával, csak a saját forkban)

- **Bájtonkénti idő:** `Dma::charged_tstates_per_byte()` = `read_cycles() + write_cycles()` a fix 2 helyett (`src/peripheral/dma.h`); az `emulator.cpp` két felhasználója (a löket felszámolása és a `port_fe_write_time()`) ezt használja. A reset utáni időzítés „01” = 3+3 ciklus (dma.vhd), így egy program, amely nem állít időzítést, bájtonként 6 T-t kap.
- **28 MHz-es várakozás csak 2 ciklusos olvasásnál:** a VHDL igazolja a mérést. Az `sram_wait_n` egyetlen 28 MHz-es órajelig alacsony a kérés élétől (zxnext.vhd:3167, :3171-3181), a DMA ezt a lefutó élen regisztrálja (dma.vhd:139-144) és csak a `TRANSFERING_READ_4` állapotban nézi (:337-342). A 2 ciklusos olvasás READ_1 → READ_4, ezért éri az impulzus; a 3 és 4 ciklusos a lejárta után ér oda. Kód: `dma.cpp`, a `read_mem_wait_tstates` hívása `read_cycles() == 2` feltétellel.
- **Előosztó:** nem kellett hozzányúlni; a JNEXT-ben az előosztó időzítője a bájt elején (READ_1) indul, és a felszámolt átviteli idő beleszámít, ahogy a VHDL-ben.
- **Tesztek:** `dma_test` 23.1–23.3, 23.8 2+2 ciklussal (40 / 32 / 32 / 40 T), 23.4 és 23.6 2 ciklusos olvasással; új DMA-HU-01 (3+3, 28 MHz: 48 T, várakozás nélkül), DMA-HU-02 (4+4, 3,5 MHz: 64 T), DMA-HU-03 (B→A: az olvasás a B, az írás az A port időzítése; 56 T); GH283-03: 24 T. `tape_save_test` TSAVE-55 (6 T-s impulzusok), TSAVE-56 (28 MHz: 64 impulzus, nincs 0 T-s), TSAVE-62 (a DMA két saját impulzusa kihagyva). A 2 ciklusos feltétel kivételét a DMA-HU-01 elkapja (mutáció). `DMA-TEST-PLAN-DESIGN.md` és a tesztmátrix frissítve (a mátrixot a CI-éval azonos FPGA-core, 3.02.03 alapján helyben generálva).
- **CI:** `45067641285` (`6cf89c6`) minden feladata zöld, a regresszió is: egyetlen képernyőképes sor sem csúszott el.
- **Viktor próbája a `dmatest.nex`-szel (2026-11-23, `6cf89c6` Windows-build):**

| (3,5 MHz / 28 MHz, sor; zárójelben körök) | KS3 3.03.03, 50 Hz | JNEXT régi | JNEXT `6cf89c6`, 50 Hz | JNEXT `6cf89c6`, 60 Hz |
|---|---|---|---|---|
| A2 | 73 / 45 | 37 / 27 | **73 / 45** | 74 / 45 |
| A3 | 109 / 54 | 37 / 27 | **109 / 54** | 109 / 54 |
| A4 | 145 / 72 | 38 / 27 | **145 / 72** | 145 / 72 |
| B8 | 80 / 72 | 74 / 72 | 74 / 72 | 73 / 72 |
| C8 | 90 (342) / 74 (2940) | 80 (385) / 73 (2429) | 81 (342) / 72 (2386) | 81 (342) / 72 (2386) |
| D2 | 280 (513) / 160 (5632) | 196 (820) / 152 (4915) | 222 (586) / 152 (4681) | 222 (586) / 152 (4681) |
| E (képkocka) | 52 | 51 | 51 | 61 |

  Az előosztó nélküli folyamatos átvitel (A) pontos. A 60 Hz-es sorszámok a ±1 kerekítésen túl azonosak (a Next képsora 50 és 60 Hz-en is 1824 mesterciklus); az E a képfrissítés arányában nő. **Ami még eltér:** az előosztós módok (B, C, D) 3,5 MHz-en bájtonként kb. 3–4 órajellel gyorsabbak (B8: 6 sor / 512 B ≈ 2,6 T; C8: 9 sor ≈ 3,9 T; D2: 58 sor / 4096 B ≈ 3,2 T); 28 MHz-en kicsi. Ez a mérés 2. pontjának „a várakozás után még kb. 3 / 7 órajel”-e: a VHDL várakozás utáni állapotváltásai, amelyeket a JNEXT nem számol. **Második lépés:** kész (lent).

## Második lépés: a várakozás utáni órajelek (2026-11-23, Viktor jóváhagyásával)

- **VHDL:** az előosztó várakozása után az állapotgép a WAITING_CYCLES-ből WAITING_ACK-on (folyamatos mód, a sín nála marad), illetve START_DMA + WAITING_ACK-on át (löketes mód, a sínt visszakéri) jut a következő bájt READ_1-éhez (dma.vhd:436-462, :266-300); az időzítő a READ_1-nél indul újra (:309). A KS3 szerint ez bájtonként kb. 3 órajel (B8: 80 sor a 74 helyett); a ZEsarUX-modell is 3-at használ.
- **Megvalósítás:** `Dma::RESUME_CLOCKS = 3`, a várakozás utáni első bájttal felszámolva (`last_burst_resume_tstates()`, az `emulator.cpp` két helyén, a kazettaport időbélyegében is), de a következő előosztó-ablakba nem számít bele (`uncounted_clocks_`, `tick_burst_wait`). Löketes módban a Z80 a gépi ciklus végén adja át a sínt (legfeljebb kb. 4 órajel), a JNEXT viszont utasításonként lép: ha az utasítás `BURST_GRANT_CLOCKS = 4` órajelnél tovább tartott a lejárat után, a többlettel a következő ablak előbb indul (mint a ZEsarUX-ban).
- **Tesztek:** DMA-HU-04 (folyamatos, előosztó 8: 32, 35, 35 órajel), DMA-HU-05 (löketes, NOP-ok: 34, 37, 37), DMA-HU-06 (löketes, 19 órajeles utasítás: 44, 28, 47 – átlag 37,5, kiegyenlítés nélkül 47); mindkét rész kivételét elkapják (mutáció). A korábbi 189 DMA-sor és a kazettamentési sorok változatlanul zöldek.
- **Várható `dmatest`-eredmény:** B8 ≈ 80 / 72, C8 ≈ 88 / 73, D2 ≈ 276 / 159, E ≈ 52.
- **Viktor próbája (2026-11-23, `188907b` Windows-build, 50 Hz):**

| (3,5 MHz / 28 MHz, sor; zárójelben körök) | KS3 3.03.03, 50 Hz | JNEXT `6cf89c6` | JNEXT `188907b` |
|---|---|---|---|
| A2 / A3 / A4 | 73 / 45, 109 / 54, 145 / 72 | ugyanaz | **ugyanaz** |
| B8 | 80 / 72 | 74 / 72 | **80 / 72** |
| C8 | 90 (342) / 74 (2940) | 81 (342) / 72 (2386) | 87 (342) / 73 (2386) |
| D2 | 280 (513) / 160 (5632) | 222 (586) / 152 (4681) | 262 (513) / 156 (4608) |
| E (képkocka) | 52 | 51 | **52** |

  A folyamatos mód (A, B) és a hang (E) pontos, a 3,5 MHz-es körszámok (342, 513) is egyeznek. A D2-re jósolt 276 túlbecslés volt. **Ami még eltér:** löketes módban 3,5 MHz-en bájtonként kb. 1 órajel (C8: 3 sor / 512 B ≈ 1,3 T; D2: 18 sor / 4096 B ≈ 1,0 T). Valószínű ok a sín visszaadása: a T80 a BUSRQ-t egy órajellel regisztrálja (`BusReq_s`, t80n.vhd:1663), és csak utána ejti a BusAck-et (:1738-1740), közben nem fut (`ClkEn <= CEN and not BusAck`, :410). Ezt a JNEXT nem számolja. A 28 MHz-es körszám-eltérés (2386 / 2940, 4608 / 5632) a processzor oldaláé: a 3.03-as core-ban 28 MHz-en nincs memória-várakozás, a JNEXT a 3.02.03-at követi.

## Harmadik lépés: a löketes mód sínkérése (2026-11-23, Viktor jóváhagyásával)

- **VHDL:** löketes módban a DMA minden várakozás után újra kéri a sínt; a T80 a BUSRQ_n-t egy órajellel regisztrálja (`BusReq_s`, t80n.vhd:1663), és csak ezután adhatja át a sínt a gépi ciklus végén (:1746-1747). Folyamatos módban a sín a DMA-nál marad, ott ez nem jelentkezik.
- **Megvalósítás:** `Dma::BURST_REQUEST_CLOCKS = 1`: löketes módban a várakozás utáni bájt `RESUME_CLOCKS + 1` órajelet fizet a READ_1 előtt (ez sem számít bele a következő előosztó-ablakba).
- **Tesztek:** DMA-HU-05 (NOP-ok: 34, 38, 38 órajel), DMA-HU-06 (19 órajeles utasítás: 44, 29, 48); `BURST_REQUEST_CLOCKS = 0`-val mindkettő bukik, a folyamatos DMA-HU-04 változatlan. A 26.11a-s kártya bootja (meleg indítás felvétele) 780 helyett 782 képkocka lett (`WSCARD-02`, `warm-start-chans-func`).
- **Várható `dmatest`-eredmény:** C8 ≈ 90 (342) / 74, D2 ≈ 280 (513) / 158, a többi változatlan – Viktor próbájára vár.
