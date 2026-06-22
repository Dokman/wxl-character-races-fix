# wxl-character-races-fix

WarcraftXL runtime module that ports the `CHARCREATIONRACE_FIX` logic from [WotLKExtensions](https://github.com/Alyst3r/WotLK-Extensions) tree into `wxl-core`.

Fixes a crash if more than 21 playable races are present during character creation.
The stock 3.3.5a client assumes fixed-size character-creation race tables, so exceeding that limit
can crash the client.

The module applies the patch once on the first rendered frame and repoints the character-creation race
lookup tables to module-owned storage so character creation keeps working with extended race counts.
