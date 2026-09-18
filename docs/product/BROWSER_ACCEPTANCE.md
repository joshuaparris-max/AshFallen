# Browser and YouTube acceptance

The final browser bar is deliberately split into automated evidence and a physical human-observed test.

## Automated image evidence

The product ISO CI must prove that:

- Chromium launches as the non-root `josh` user without `--no-sandbox`;
- the main launcher does not disable GPU acceleration;
- PipeWire/Pulse has a default output sink;
- Chromium reports H.264, VP9, AV1, Opus and AAC media support;
- Chromium itself can reach a YouTube HTTPS endpoint;
- NetworkManager/DNS/NTP/CA/TLS checks pass;
- a real Chromium profile (`Local State`) is created;
- `JOSH-DATA` preserves both the browser profile and Downloads across two VM boots;
- Chromium session restore is configured;
- the Stage 0 password manager is disabled because its autologin keyring is not a strong encrypted-at-rest credential store.

These checks make the image testable and materially YouTube-ready, but they do **not** prove a Google account can sign in on a particular physical machine or that speakers are audibly producing sound.

## Physical acceptance

Boot the image with a persistent ext4 volume labelled `JOSH-DATA`, open a terminal, and run:

```sh
josh-youtube-acceptance start
```

Sign in to YouTube, play a video, and confirm audible sound. Then run:

```sh
josh-youtube-acceptance mark-playback
```

Reboot the machine, then run:

```sh
josh-youtube-acceptance verify
```

Confirm the visible browser is still signed in and audio still plays, then run:

```sh
josh-youtube-acceptance confirm
```

A pass report is written to `~/Downloads/josh-youtube-acceptance.txt`. The harness never exports cookie contents, passwords or tokens.

## Security boundary

Stage 0 remains a passwordless autologin development image. Its libsecret/keyring integration gives Chromium stable secret storage across `JOSH-DATA`, but it is not equivalent to an installed OS with PAM-unlocked, password-protected credentials. Browser password saving is therefore disabled on Stage 0. A production installer/account model must replace this before persistent browser credentials are treated as protected at rest.
