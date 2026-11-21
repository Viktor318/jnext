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
