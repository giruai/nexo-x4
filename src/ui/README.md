# User interface

The active interface adapts RafaelNGP's PSBox catalog and in-stream menu under `src/ui/psbox/vendor`. `src/ui/psbox/bridge.cpp` maps native catalog, authentication, session, controller, and stream snapshots into that interface. PSBox source headers and notices are retained; see [Credits](../../CREDITS.md).

`src/ui/psbox_backend` uses a persistent worker for public catalog metadata, curated rows, and images. It can enrich verified catalog entries but cannot create an entitlement or change a launch title ID. `canvas.c` and `screen.c` retain the earlier native fallback and local development views. The current UI source includes Spanish text.

The large in-stream menu has a higher rendering cost than compact statistics; see [Architecture](../../docs/ARCHITECTURE.md) for the observed performance limit.
