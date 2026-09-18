# Contributing to Josh OS

## The one rule

> Josh OS should feel simpler after you understand it, not more complicated.

Every pull request gets measured against it. A feature that adds a second way
to do something already possible is a regression, even if the code is good.

## Practical guidelines

- **No hex codes outside `design/tokens.json`.** If you need a colour, add a
  token. `scripts/build-tokens.mjs` regenerates `design/tokens.css`; never
  hand-edit the generated file.
- **Apps never touch the window manager.** An app declares itself and fills a
  body element. That boundary is what lets the shell prototype become real
  processes later without rewriting every app.
- **Don't fake capability.** A dead toggle is worse than a missing one. If a
  setting can't do anything yet, leave it out and note it in the roadmap.
- **One decision, one ADR.** Anything architectural goes in
  `docs/decisions/` before it goes in code.

## Commit style

Plain imperative subject lines: `add snap zones to window drag`. No prefixes
needed. Explain *why* in the body if it isn't obvious.
