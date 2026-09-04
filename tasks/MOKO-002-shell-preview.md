# MOKO-002 — MOKO Shell Developer Preview
State: DONE (validated)

Implement a Qt 6/QML full-screen shell preview based on the approved bright MOKO OS concept.

## Acceptance
- top bar;
- launcher panel;
- central MOKO identity;
- AI panel;
- floating dock;
- original MOKO programmatic glyph language;
- responsive minimum 1280×720.

## Validation
- 2026-09-04: `./scripts/test-shell-debian.sh` configured and built all 34 Ninja targets on Debian 13, then passed the offscreen `moko-shell-smoke` CTest.
- The same test launched the shell under Weston headless Wayland and rendered a non-empty 1280x720 screenshot; the live ISO rendered the complete shell at 1280x800 in `out/moko-iso-smoke-20260904T144109Z-boot-{1,2,3}.png`.
- Visual inspection confirmed the top bar, launcher, MOKO identity, AI panel and dock render without overlap in all three cold boots.

## Rollback
Revert the shell/QML changes and rerun `./scripts/test-shell-debian.sh`; the live ISO is not modified in place and can be rebuilt from the previous source revision.
