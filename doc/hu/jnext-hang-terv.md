# JNEXT – hangra vonatkozó ötletek és kérések (2026-11-21)

A ZEsarUX-beszélgetésben merült fel; a JNEXT-munkának szól. A `doc/hu/jnext-atadas.md` kívánságlistájának 3. és 4. pontjához kapcsolódik. Mindegyiknél előbb felmérés, aztán Viktor jóváhagyása. (Eredetileg a claude.ai-projekt `claude/jnext-hang-terv.md` dokumentuma; 2026-11-21 óta a tárolóban él.)

## Viktor megfigyelése

Ugyanazt a DAC-hangot (NXModPlayer, ill. a `dmatest.nex` E tesztje) összevetve: **a ZEsarUX-ban „kicsit mélyebb és tisztább” a hang, a JNEXT-ben néha kicsit recseg.**

## A valószínű okok (Claude, gyors kódnézés alapján, a JNEXT-ben nem mérve)

- **Hangszín:** a JNEXT keverője (`src/audio/mixer.cpp`) 44,1 kHz-en dobozszűrővel (időarányos átlag) mintavételez, utószűrés nélkül; a 8 bites DAC lépcsőinek magas felharmonikusai és az áliasok bent maradnak. A ZEsarUX-fork jó minőségű hangja (`src/audio/audio_hq.c`) sávkorlátozott szintézis (Blip_Buffer), 48 kHz, utána a Fuse-féle „tévéhangszóró” szűrő (100 Hz felüláteresztő, 10 kHz aluláteresztő) – ettől melegebb. A JNEXT-é inkább a Next digitális (HDMI) kimenetéhez áll közel, tehát nem feltétlenül hiba.
- **Recsegés:** a JNEXT gyűrűpuffere kicsi (kb. 4 képkocka, `mixer.h`); ha az emuláció megakad (Windows-időzítés, ablakmozgatás, menük – kívánságlista 3. pont), kiürül, kattan. A ZEsarUX-ban egy alkalmazkodó FIFO a hang sebességét legfeljebb ±2%-kal igazítja, hogy a puffer a célkésleltetés körül maradjon (`audio_hq_end_frame`). A recsegés és a 3. pont (akadás) ezért összefügghet; érdemes egymás után csinálni őket.

## Javaslatok és kérések (felmérendő)

1. **Alkalmazkodó hangpuffer** a recsegés ellen: nagyobb tartalék, és a kimeneti mintaszám finom igazítása a puffer telítettsége szerint (a ZEsarUX `audio_hq.c` mintájára).
2. **Opcionális „tévé” hangszűrő** (100 Hz / 10 kHz), kapcsolhatóan, a Beállításokban és `--set` kulccsal (a tároló szabálya szerint minden beállításnak legyen CLI-megfelelője).
3. **DAC-keverő (Viktor kérése, 2026-11-21):** a Next négy DAC-csatornájának (A, B, C, D) **hangereje és térbeli elhelyezése** külön állítható legyen, **bal – közép – jobb elrendezésben** (Viktor pontosítása: „abcd csatornák keverése jobb-közép-bal elrendezésben”, az AY ABC/ACB módjának mintájára). Vagyis csatornánként hangerő és hely (bal / közép / jobb, esetleg finom panoráma), és kész elrendezések. Ez a kívánságlista 4. pontjának („külön hangkeverő, előbb megkérdezni, mit tartalmazzon”) első konkrét tartalma; az AY-csatornákra és a beeperre is kiterjeszthető. Vigyázat: a valódi Next-ben a DAC-ok rögzítetten kevertek (A és B bal, C és D jobb; a `$DF`/`$FB` port A+D, a `$B3` B+C), tehát ez a gépen túlmutató, felhasználói lehetőség; alapból a valódi gép szerinti elrendezés. A JNEXT-ben már van `set_dac_gain_db` (`mixer.cpp`), erre lehet építeni. Beállítás + `--set` kulcsok.
4. Az AY sztereó módja (ABC / ACB) a JNEXT-ben a gép felől már működik: `TurboSound::set_stereo_mode(bool acb)` a NextReg 0x08 5. bitjéből (a NextZXOS `config.ini` `stereomode=` sora állítja). Felhasználói felülírás (a gép szerint / ABC / ACB / mono) a keverőbe ugyanígy felvehető, ha Viktor kéri.

## Kapcsolódó

- DMA-időzítés mérése és javaslat: `doc/hu/jnext-dma-meres.md`.
- A ZEsarUX-forkban van már AY-keverő (Hang → AY keverő); a DAC-keverő ott is készülhet, ha Viktor kéri.

## Felülvizsgálat az 1.3.66 után (2026-11-23, Viktor kérésére; kód még nem változott)

A fenti „valószínű okok” mérés nélküli becslések voltak; ez a rész dönt, ahol eltérnek.

**Mi változott az 1.3.66-tal**

- **GH #550:** a szalagnak nincs külön tagja a keverőben; a MIC-en (`Beeper::mic_final`) szól, 128-cal a korábbi 512 helyett (−12 dB), ahogy a VHDL-ben. A „Beeper” csúszka továbbra is ezt is szabályozza. Külön „Szalag” csúszka nem következik belőle: a hardverben a szalag és a MIC bit XOR-olva egyetlen jel.
- **GH #533:** csatornánkénti némítás (9 AY, 4 DAC) a hibakeresőben. A DAC-nak van hallható másolata (`Dac::aud_`): a csatornánkénti hangerő és hely ide köthető, a gép állapota, a pillanatkép és a visszatekerés nem látja.
- **GH #224 következő lépései** (nyitott: #536, oszcilloszkópok; később forrásonkénti WAV) ugyanezt a `mixer.{h,cpp}`-t írják majd. A saját kód ezért külön fejlécbe kerüljön, a keverőben csak egy elágazással: ha minden alapértéken áll, a mai út fusson.
- A lejátszási oldal (`audio_pacing.h`, `audio_fill.h`, `sdl_audio.cpp`) nem változott.

**A recsegés: pontosítás**

- A JNEXT-ben már van a hangkártya órájához igazítás: 40–90 ms-os sávban tartja a puffert, képkocka-duplázással és -kihagyással (a 4 képkockás gyűrűpuffer csak a keverő és a kimenet közötti átmeneti tár, a kimeneti 250 ms-os). A ±2%-os sebességigazítás (1. javaslat) ezért **nem kell**.
- Ami hiányzik: 40–90 ms-nál hosszabb akadásra nincs tartalék, olyankor az utolsó szint áll, és a folytatásnál ugrás hallatszik. Ez a kívánságlista 3. pontja (mért akadások: 85–980 ms).
- Javaslat helyette: **állítható hangpuffer** (pl. kicsi 40–90 ms / közepes / nagy), ami a rövid akadásokat elnyeli, a hosszabb késleltetés árán. Egy 900 ms-os akadást ez sem fed el.

**Mérés (dmatest E, `b32ebe7` Linuxon, `--headless`, `--wav-record` + `--dac-trace`; 3,5 MHz, 428 Hz-es négyszögjel)**

- **A generált hangban nincs kattanás:** 1,06 s alatt csak a két szint (±1024) és a szűrt élek. A „recsegés” vagy a lejátszásnál keletkezik (kifogyó puffer), vagy az alábbi érdesség; kattanás a keverőben nincs (ebben a tesztben).
- **A DAC-írások időpontja szór:** 212–243 T (átlag 227,0), az egyenleteshez képest −6,6…+11,9 µs, effektív 4,1 µs. Löketes módban a JNEXT utasításhatáron adja át a sínt, a valódi Z80 gépi cikluson, ott a szórás kisebb.
- **A dobozszűrő alulmintavételezési maradéka** (a nem harmonikus összetevők a teljes jelhez képest, és 8 kHz alatt):

| jel | mai dobozszűrő | 4× túlmintavétel + FIR | sávkorlátos (Blip-jellegű) |
|---|---|---|---|
| mért időpontok, 3,5 MHz | −29,1 / −35,6 dB | −32,1 / −35,8 | −31,1 / −35,7 |
| a szórás nyolcada (28 MHz becslése, nem mérés) | −31,5 / −46,2 | −49,7 / −53,6 | −49,0 / −53,6 |
| pontosan egyenletes időpontok | −31,4 / −46,9 | −59,5 / −67,0 | −68,0 / −68,5 |

  Az első sor a mért időpontokból számolt jel; a JNEXT saját WAV-ja −29,4 / −35,7 dB, vagyis a számítás a valódi kimenetet adja vissza. 3,5 MHz-en a szórás a nagyobb hiba, a jobb szűrő ott keveset ér; 28 MHz-en (a lejátszók többsége) a szűrő számít. Egyetlen jelen mért adat, zenén nincs mérve.
- **„Kicsit mélyebb”:** Viktor megfigyelése a DMA-javítás előtti. A löketes DAC-lejátszás azóta 3,5 MHz-en a KS3 ütemén fut (227 T, 15,42 kHz az előosztó névleges 220 T, 15,91 kHz-e helyett), tehát legfeljebb kb. 3%-kal (fél félhanggal) mélyebb lett. Újra kell hallgatni, mielőtt a „tévé” szűrőről döntünk.

**Felülvizsgált sorrend** (mindegyik külön jóváhagyással)

1. **Mérés Viktor gépén, kód nélkül:** hangos program közben `jnext.exe --log-level platform=debug`; a `cadence:` sorban az `audio-fill: N ms (M events)` csak akkor jelenik meg, ha a puffer kifogyott. Ha recsegéskor ott van, a 2. lépés a javítás; ha nincs, a 4.
2. **Állítható hangpuffer** (kívánságlista 3. is): kb. fél–egy nap; `audio_pacing` sávja paraméterré, `--set` kulcs, Beállítások, `audio_pacing_test`.
3. **DAC-keverő** (Viktor eredeti kérése): csatornánként hangerő és bal–közép–jobb hely, alapból a gép szerint (A, B bal; C, D jobb); kb. egy nap, 8 új `--set` kulcs, Beállítások > Hang, fordítás, man page, tesztek (bitre azonos kimenet alapértéken). A felvételekben (WAV, videó) is hallatszik, mint a mai hangerők. Az AY sztereó felülírása (gép szerint / ABC / ACB / mono) kb. fél nappal ehhez vehető.
4. **Jobb mintavételezés** (4× túlmintavétel + FIR, kapcsolhatóan): kb. másfél–két nap, a keverő forró útját érinti (sebességmérés kell). Sávkorlátos szintézis helyett ez: közel ugyanazt adja, kisebb beavatkozással.
5. **„Tévé” szűrő** (100 Hz / 10 kHz): fél nap, csak ha az újrahallgatás után is hiányzik.

A DAC-írások szórásának csökkentése (a hang szempontjából a lejárat pillanatára visszadátumozva) külön felmérést kérne, egyelőre nincs a sorban.

## A recsegés oka és javítása (2026-11-23, Viktor jóváhagyásával, csak a saját forkban)

**A fenti 1. lépés (mérés) lezárva; a felülvizsgált sorrend 2. és 4. lépése az NXModPlayer recsegésének nem megoldása.** Viktor naplóiban lejátszás közben nem volt `audio-fill`, duplázás vagy kihagyás (a puffer 66–84 ms), a lejátszási oldal hibátlan. A 28 MHz-es memória-várakozás kipróbálva: nem ez az ok.

- **Tünet:** Viktor felvételén (`dune_ecolove.mod`) a bal csatornán pontosan 0,160 másodpercenként (zenei soronként) kattanás; a lejátszó 0 gombja (leállítja, majd új fázisban újraindítja a mintakimenetet) néha megszünteti, néha sokkal rosszabbá teszi. A ZEsarUX-ban ugyanez a dal nem recseg.
- **A lejátszó:** két 10 mintás félpuffer; a CTC0 (28 MHz / 896 = 31 250 Hz) kiírja, a CTC1 (pontosan 10-szer ritkább) a másik félbe kever, csatornánként külön megnézve, melyik fél szól. Dalindításkor a kezelők csak `EI; RETI`, és az első CTC1 állítja be a fázist (önmódosító `LD IY,$FA00`). Ha a fázis rossz, a félváltás a keverés közben történik: rossz blokk szól.
- **Két JNEXT-hiba a `src/cpu/im2.cpp`-ben (VHDL-eltérés):**
  - a RETI-dekóder S_ED4D_T4 / S_ED45_T4 → S_SRL_T1 → S_SRL_T2 → S_0 útja a VHDL-ben *órajelenként* lép (`im2_control.vhd:181-192`), a JNEXT *utasításlehívásonként* léptette, így a RETI utáni három lehívást elnyelte. Egy RETI után azonnal elfogadott megszakítás `EI; RETI` kezelője elvesztette a saját RETI-jét, a CTC0 kiszolgálás alatt maradt, és a következő sormegszakításig (a mért esetben kb. 18 ms) nem jött CTC-megszakítás; a függőben maradt CTC1 állította be a (rossz) fázist;
  - egy RETI-t kétszer dolgozott fel (`on_reti()`, majd ugyanazt az impulzust a `step_devices()`), így egymásba ágyazott megszakításnál a külső eszköz is felszabadult (`im2_device.vhd:123-128` szerint csak egy).
- **Mérés** (26.11a-s kártya, `dune_ecolove`, 30 különböző indítási képkocka): javítás előtt 6 hibás indítás, javítva 0; 21 szakasz a 0-s leállítás és újraindítás után: előtte 2 rossz fázisú, javítva 0. Csak a második javítással a zene elnémul (a kettős feloldás eltakarta az elveszett RETI-t), ezért a kettő együtt kellett.
- **Tesztek:** IM2C-08 és IM2C-09 a VHDL-hez igazítva (a régi, lehívásonkénti modellt rögzítették); új IM2C-HU-01..03, IM2P-HU-01..02. Mutációval mindegyik ellenőrizve.
- **Ami a tervből marad** (mind Viktor döntésére vár): állítható hangpuffer (a párbeszédablakok alatti akadásra, kívánságlista 3.), DAC-keverő, jobb mintavételezés, „tévé” szűrő.
