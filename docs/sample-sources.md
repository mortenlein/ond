# Sample sources — curated by people

Rule: only sounds recorded and chosen by humans. Two lists, because licenses differ:
**A** may be built into Ond (shipped in the GPL firmware). **B** may only be used in Morten's own
music and as a listening/measurement reference; never copied into the repo.

## A — may ship in Ond

| Source | License | Useful for Ond |
| --- | --- | --- |
| VCSL (github.com/sgossner/VCSL) | CC0 | Pipe Organ, Renaissance Organ, Hohner harmonicas (free reeds), Upright Piano (Yamaha, Knight), Woodblock, Slapstick, Ratchet, Guiro, Shakers, Claps, Cajon, Bass Drums, Snare (rope tension), Ocean Drum, Wine Glasses, Kalimbas |
| VSCO-2 CE (github.com/sgossner/VSCO-2-CE) | CC0 | Orchestral; already used by Felucca/Clipper tools |
| Univ. of Iowa MIS (theremin.music.uiowa.edu/mis.html) | Free, "without restrictions" (no named license: credit them anyway) | Found Objects 1 & 2, hand percussion, piano |
| Freesound (freesound.org) | Per file: CC0, CC BY, CC BY-NC | Only CC0 (or CC BY with credit) may ship; never BY-NC |
| Morten's own recordings | his | Everything: bellows, wood, keys, cables, tape machines, rooms |

Every shipped sample gets a line in `assets/samples/CREDITS.txt`: file, source, author, license.

## B — own music and reference only

| Source | Terms | Why |
| --- | --- | --- |
| Pianobook "Antique Harmonium" | Pianobook terms: no giving samples away | Real harmonium recorded at Spitfire's Studio B, bellows and pedals sampled: the reference for Ond's reed and bellows |
| Pianobook "Elektrisk Salmesykkel" (Benjamin Dehli) | Pianobook terms | Yamaha L-20D electric harmonium, wind supply, key noise, tremulant |
| Wavesfactory Little Harmonium | Paid (Kontakt) | 19th-century pump organ, air on the mod wheel |
| Goldbaby TapeTonic | Free, terms unstated | Drum machine hits recorded to tape |
| One Man Tribe "Tape Drum Bites" | Free, royalty-free, terms on benaylon.com | Lo-fi drums through a Roland RE-201 Space Echo |
| OpenAIR (Univ. of York) | Creative Commons, varies per IR | Real rooms as impulse responses, for Ableton; check each IR before shipping anything |

Measuring a B recording (spectra, envelopes, bellows timing) to set Ond's parameters is fine;
copying its audio into Ond is not.
