# 015 step 6a — BEAT's PUMP: the kick ducks the instrument

Status: **ready to listen**. SLOOP's own DUCK (fx.c `duck_block`, ear-tuned on the groovebox; off in Clipper, whose
slot for it is the reverb TYPE): each kick dips Ond's sound, which swells back over an eighth note on a
(1 − t/T)² curve. PUMP is its depth, 0 by default (off: as before). Your own kick (YOUR KIT) pumps too.

Hip-hop groove 1, LO-FI kit, 88 BPM, Live playing:

- `before_chapel_hiphop_pump0.wav`, `after_chapel_hiphop_pump60.wav`, `after_chapel_hiphop_pump110.wav`
- `before_space-drone_hiphop_pump0.wav`, `after_space-drone_..._pump60.wav`, `..._pump110.wav`

Measured (the organ alone, the drums at level 0): just after a kick vs just before, −2.5 dB at PUMP 0 (the organ's
own motion), −6.8 dB at 60, −15.2 dB at 110.

Listen for: the lo-fi breathing — on the drone especially. Which depth? Should Ond start with some PUMP on when a
FEEL is picked?

## 6b — TONE: the beat's own filter (Ond 0.15)

SLOOP's per-track filter on the drum track (P_TFLT: below 0 a low-pass closing, above a high-pass; OFF at 0, the
default). The muffled, far-away drums of lo-fi, the organ untouched.

- `after_chapel_hiphop_tone-lp30.wav`, `after_chapel_hiphop_tone-lp50.wav` (vs `before_chapel_hiphop_pump0.wav`).

Measured on the drums alone: TONE −50 leaves 40 Hz–4 kHz as it was (±0.2 dB) and takes 24 dB off above 4 kHz.

(The beat's own DUST — a second DUST on the drums only — needs Clipper's DUST to run twice; held until Morten
wants it beyond TONE + the master DUST.)
