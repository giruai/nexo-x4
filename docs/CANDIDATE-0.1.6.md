# Nexo X4 0.1.6 candidate — hardware validation pending

This is a development build, not a replacement claim for the tested 0.1.5 beta. APP_VER is 01.38; the existing title ID is retained for an in-place upgrade. Do not uninstall the application or erase its saved sign-in data.

## Changes

The experimental 1080p profile now uses the web/Windows G8 negotiation adapted from XCloud4 1.1.0-beta. It requests 1920×1080 at up to 60 FPS with an 8 Mbps video SDP ceiling and adaptive requests between 5 and 8 Mbps. 720p remains selectable with its previous native identity and 3–8 Mbps ABR range. Choosing a profile applies to a new cloud session; received video dimensions remain separately displayed.

The decoder validates SPS syntax and coded/storage dimensions rather than treating the menu preference as the stream format. Visible 1080p can require 1088 coded lines. Initial sizes above 1920×1088 are held for up to 10 seconds while requesting keyframes, rather than passed to native decoding blindly. A later SPS change is explicitly rejected; automatic mid-session resolution reconfiguration is not implemented in this candidate. If the service switches from 720p to 1080p after the decoder starts, a new session may be required.

The 1080p streaming display uses HSYNC to reduce VSync waiting. This may cause tearing. The buffer release wait remains mandatory. The UI and 720p path retain VSync. Audio uses the upstream adaptive worker, preferring 512-sample blocks and falling back to 1024 when the native API rejects 512.

## Deliberate limit

Decoder depth remains 1. Upstream's depth-2 input-buffer retirement includes an explicitly unverified native lifetime assumption. This candidate does not import that assumption. Consequently this is an attempt to obtain and sustain 1080p/60, not a guarantee that the entire upstream performance result is reproduced.

## Features retained

PSBox frontend, six selectable languages, stored refresh/sign-in renewal, game-text keyboard, single-controller mapping, optional diagnostics and adaptive bitrate remain. There is no new voice, local multiplayer, 4K, HDR or rumble claim. Advertised 4096×2160 browser display metadata is not a 4K decoder capability.

## Required console checks

After owner confirmation that the PS4 is available: install over the existing version, confirm stored sign-in restoration, test native 720p regression, then start a separate 1080p session. Record requested and actual SPS/decoded dimensions, fresh presented FPS, loss, decode/copy/conversion/menu cost, memory failures and audio behavior. Check game text and controller mapping before a complete Fortnite match. Compare menu open/closed and 720p/1080p separately. Never infer stream resolution from Remote Play footage.

No console installation or test is authorized merely by preparing this candidate. Build success and portable tests must be reported separately from hardware results.
