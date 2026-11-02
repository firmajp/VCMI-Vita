# 1.1.0

First public release, incorporating the Vita adaptation and the optimisations verified during development.

- Improve map and town rendering with bounded caches and reduced repeated UI work.
- Prepare cached ambient audio in the background and retain guarded synchronous fallbacks.
- Reduce routine logging overhead and redundant battle AI copies and bonus queries.
- Preserve AI difficulty, simulation depth, movement animations and save compatibility.
- Include Vita controls, matching LiveArea/startup/loading artwork and stage-based loading messages.

User-tested on Vita 2000, firmware 3.65 Enso. Performance remains scene-dependent; first-use asset and turn-transition stalls can still occur. See `docs/compliance/DEVICE_TEST.json` for the exact reported scope. Internal development versions remain recorded in `MODIFICATIONS.md` and historical technical evidence.
