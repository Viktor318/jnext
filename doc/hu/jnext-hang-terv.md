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
- **Viktor próbája (2026-11-23, `6f4a015` Windows-build): a recsegés megszűnt.**
- **Ami a tervből marad** (mind Viktor döntésére vár): állítható hangpuffer (a párbeszédablakok alatti akadásra, kívánságlista 3.), „tévé” szűrő. A jobb mintavételezés és a DAC-keverő kész (lent).

## 4×-es túlmintavétel (2026-11-23, Viktor jóváhagyásával)

Viktor két WAV-ot hasonlított össze (Jarresque, a mostani és a 4×-es módszer ugyanabból a DAC-felvételből), és hallotta a különbséget.

- **Megvalósítás:** `src/audio/oversampler.{h,cpp}`: az átlagolás a kimeneti minta négy részén (176,4 kHz), majd 256 együtthatós lineáris fázisú FIR (Blackman, 20,5 kHz) 44,1 kHz-re. A `Mixer`-ben csak egy elágazás; kikapcsolva a mai út fut. Nem a gép állapota, mentésbe nem kerül.
- **Beállítás:** `audio.oversample` (alapból **ki**), Beállítások > Hang > „Túlmintavételezett hang (4x)”, azonnal életbe lép; `--set audio.oversample=true`. A felvételekben is benne van.
- **Mérés:** 3 kHz-es, 31,25 kHz-en lépcsőzött hang visszahajló összetevői legalább 30 dB-lel kisebbek (OS-HU-04); 16 kHz-en a mai −2 dB helyett 0,3 dB-en belül. A Jarresque-on a program kimenete a meghallgatott WAV-val 0,37 egységen belül egyezik; kikapcsolva bitre a korábbi. Sebesség (NXModPlayer, Release, SDL-build): kikapcsolva nem lassabb, bekapcsolva 4–6%. Késleltetés 0,7 ms; átkapcsoláskor apró pattanás lehet.
- **Tesztek:** OS-HU-01..07, SG-HU-01, SGC-HU-01..02, SGP-HU-01..03, mutációval ellenőrizve.
- **ZEsarUX:** nem kell, a DAC ott a jó minőségű hangnál már sávkorlátozott (Blip_Buffer, `audio_hq_dac`, 48 kHz).

## DAC-keverő és AY sztereó (2026-11-25, Viktor jóváhagyott terve szerint)

A felület Viktorral rajzról rajzra készült (hat vázlat); a döntései: csatornánként hangerő és háromállású hely; az AY-nál a gép felépítése (egy közös ABC/ACB, csipenként mono pipa), „A gép most” és „Ahogy szól” sor; keretek, keskeny listák, üres sor a keretek fölött; a címek kötőjellel.

- **DAC-keverő:** `src/audio/dac_mixer.h`, csatornánként −24…+24 dB és bal / közép / jobb; alapból a gép szerint (A, B bal; C, D jobb), akkor a `mix()` egész számos útja fut (bitre a mai, +0,1 ns a 4,7-ből). A közép fele-fele arányban adja a jelet a két oldalra: a `$DF` (A+D) középre téve ugyanolyan hangos, egy csatorna viszont oldalanként halkabb. A meglévő DAC csúszka a négy közös hangereje.
- **AY:** `TurboSound::set_stereo_override()`; ABC vagy ACB mindhárom csipre a NextReg 0x08 helyett, mono csipenként. A mono csak hozzáad: a program által monóra állított csip mono marad, a program bal/jobb kapcsolása is. A hibakereső némításának mellékútján fut, kikapcsolva a sebesség a régi.
- **Csak a hallható hang változik:** a NextReg-ek, a pillanatkép, a visszatekerés, az oszcilloszkóp és a sávfelvétel a gép szerinti jelet látja; a WAV- és videófelvétel a hallhatót.
- **Beállítások > Hang:** „AY-csatornák” és „DAC-csatornák” keret; az „Elrendezés” lista a négy helyből számolt (kézi eltérésnél „Egyéni”), nem külön beállítás. „A gép most” a program beállítása, „Ahogy szól” a felülírással együtt, fél másodpercenként frissül; a hangerőt és a némítást nem mutatja.
- **12 kulcs:** `audio.dac_a_gain_db`…`dac_d_gain_db`, `audio.dac_a_pan`…`dac_d_pan`, `audio.ay_stereo`, `audio.ay0_mono`…`ay2_mono` (`src/audio/audio_channels.h` egyben viszi végig; a KeyId-k sorrendje számít).
- **Tesztek:** DM-HU-01..08, AS-HU-01..08 (az AY-é kimerítő: 16 gépállás × 24 felülírás, ikerpárhoz mérve), SG-HU-02..03, SGC-HU-03..05, SGP-HU-04..09, SETK-HU-01; a DM és AS sorok mutációval ellenőrizve.
- **Javítás Viktor próbája után (2026-11-26):** elhúzáskor a Beállítások ablak magasabb lett, a lap alján üres sorokkal. Ok: a tördelt magyarázó szöveg (QLabel) „kívánt” magassága 5 sor volt (a Qt egy becsült keskeny szélességgel számol), a legkisebb 1; megnyitáskor a legkisebbet kapta, a Windows elhúzáskor a kívántra nagyította. Most `FitNoteLabel` (`preferences_dialog.cpp`): a kívánt és a legkisebb méret ugyanaz, a szöveg fele szélességéhez tartozó két sor. Viktor kérésére a szöveg és a „Túlmintavételezett hang” balra, a lap teljes szélességére került, a szöveg sorkizárt. Teszt: SGP-HU-10 (megnyitás, újrarendezés, újra: ugyanaz a magasság; mutációval ellenőrizve).
- **Bővítés Viktor kérésére (2026-11-26, rajz alapján jóváhagyva):** a DAC-csatornáknál csatornánként **némítás** (pipa = némítva, a betű előtt; `audio.dac_a_mute`…`dac_d_mute`, elmentődik, a hibakereső némításától független), a rádiógombok helyett **folyamatos panoráma** (`audio.dac_a_pan`…: −100 bal … 0 közép … 100 jobb, csúszka, kiírás „B 40”, „Közép”, „J 100”), a hangerő-csúszkák a betűkig érnek. **Lineáris** beosztás (Viktor: a hangzás a mostanihoz képest ne változzon): a bal, a közép és a jobb állás bitre a korábbi; a köztes állás arányos ((100 − p)/200 balra, (100 + p)/200 jobbra). A korábbi `left`/`centre`/`right` értékeket a beállításfájlból −100/0/100-ként olvassa. Az Elrendezés lista marad (Gép szerint, Bal–közép–jobb, Mono, Egyéni), csak a panorámát állítja. Új sorok: DM-HU-09..10, SGC-HU-06; módosítva: DM-HU-* (egész számú hely), SETK-HU-01, SGC-HU-04..05, SGP-HU-04..06; mutációval ellenőrizve.
- **Szóló a némítás helyett (Viktor, 2026-11-26):** a pipa szóló: ha bármelyik DAC-csatornánál van pipa, csak a bepipáltak szólnak (a saját hangerejükkel és helyükkel), ha sehol, mind. **Nem mentődik** (Viktor döntése): `audio.dac_a_solo`…`dac_d_solo`, új kulcsfajta `Owner::Session` (`--set`-tel megadható, a futás alatt él, a jnext.conf-ba nem kerül; a mentés a fájlból a korábbi `dac_X_mute` kulcsokat is törli). A betű a jelölőnégyzet saját felirata lett, mint a Mono sorban, így a Windows tartja a távolságot (Viktor jelezte, hogy a pipák belelógtak a betűkbe). DM-HU-10 és SGC-HU-04 a szólóra írva át, mutációval ellenőrizve.
