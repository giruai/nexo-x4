# Privacy and local-data notice

This describes the Nexo X4 client source in this repository and its current local 0.1.3 candidate. The first published release is v0.1.0; behavior can differ between versions. It does not describe all data collected by Microsoft, game publishers, Sony, a modified build, or the network and device used to run it. See the [Microsoft Privacy Statement](https://www.microsoft.com/en-us/privacy/privacystatement) for Microsoft's separate practices.

## Sign-in and cloud traffic

Nexo X4 starts a Microsoft device-code sign-in: it displays a short code and Microsoft verification page, while the client polls Microsoft for the result. After authorization, it uses Microsoft and Xbox authentication tokens to request a catalog and to start a cloud session. It sends the selected title, session signaling, controller input, and network media traffic needed to play to Xbox Cloud Gaming. The client uses Microsoft sign-in and Xbox endpoints, Microsoft catalog and image endpoints, regional Xbox streaming endpoints, and Google's public STUN server for WebRTC connection setup. These services can see the information necessary for their respective requests, including network addresses; the service and game may process gameplay under their own policies.

The currently selected public OAuth client identifier is for compatibility. It is not a secret or a Nexo X4-owned app registration. Nexo X4 does not ask for or store an account password in its UI; the user completes authorization on Microsoft's page.

## Data on the PS4

To offer sign-in restoration, Nexo X4 saves a **refresh token in plaintext** in an app-local file associated with the local PS4 user. It restricts the directory and file permissions, but this is not encryption. Someone with privileged filesystem access, including a privileged FTP service, could read the token; keep such access protected and do not share the file. Access tokens, device codes, and passwords are not saved in that file. If the local user cannot be resolved or saving fails, access may remain only in memory and restoration may be unavailable.

The app also saves local UI preferences (including language, sort order, stream profile, diagnostic choice, and hidden product IDs) and caches public catalog images. Public catalog metadata is fetched to display games; account-derived catalog and session state is held in memory for the active run. The legacy `X4`/`XCLD00001` identity and app-local data locations are retained for migration compatibility.

The **Forget account / sign out locally** action removes the saved refresh-token file and clears the active client session when the operation succeeds. If deletion fails, the app reports an error instead of claiming the credential was removed. A Microsoft `invalid_grant` renewal response also clears the local saved token when possible. Local sign-out does **not** revoke a credential at Microsoft or delete service-side account and gameplay records. UI preferences, cached images, and diagnostic files are separate local data and are not removed by that action.

## Diagnostics and logs

The Options **Diagnostics** switch defaults off. Enabling it adds bounded numeric input and render measurements; those probes do not intentionally capture typed text or credentials. Turning it off stops those optional measurements. Independently of that switch, the media pipeline records a bounded numeric performance trace and attempts to write it to app-local storage when a session closes. The application also emits lifecycle, network-status, and error messages to local logs. These diagnostics can include timings, packet counts, transport state, error codes, build/version information, and game or region context; treat exported logs and trace files as potentially sensitive and review them before sharing.

Static inspection of the client source found **no Nexo-owned analytics or telemetry upload endpoint**. This is a bounded statement about this source, not a promise about Microsoft, Google STUN, game publishers, the PS4 environment, or modified builds. Nexo X4 does not intentionally write typed game text, passwords, or token values to its diagnostic files or logs; unexpected platform or third-party logging is outside this source-level assessment.

### Experimental in-game text entry

The local keyboard temporarily holds the game's prompt, default value and entered text in memory to answer its Xbox virtual-keyboard transaction. Text is sent to the active Xbox session only; it is not written to preferences, account storage or diagnostic logs. Password-scope requests are cancelled. Buffers are cleared on completion, cancellation and teardown. Screenshots or recordings can still expose visible game/account information; test captures belong outside the repository and release assets.
