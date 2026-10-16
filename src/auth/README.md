# Authentication and Xbox sessions

`http_client` performs bounded native HTTPS requests with certificate validation and restricted destinations. `json` validates bounded responses. The authentication worker runs Microsoft device-code sign-in, polling, expiry, token renewal, and Xbox session requests without blocking the UI.

`xbox_live` obtains the Xbox credentials required to read the service catalog and create a streaming session. The worker exposes copied status and catalog snapshots to the main thread. Credentials and response bodies remain in private worker state and are cleared when no longer needed; UI snapshots carry no tokens or account passwords.

The public OAuth client identifier is used for service compatibility. It is not a secret or a Nexo X4-owned app registration. Service authorization is determined by the provider.

The PSBox metadata worker may enrich products already present in the verified catalog, but cannot add an entitlement or change the title ID used to launch a session. See the [architecture](../../docs/ARCHITECTURE.md) and [third-party notices](../../THIRD_PARTY_NOTICES.md).
