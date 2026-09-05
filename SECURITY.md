# Security policy

WayKeeper is local-first and pre-release. Report security findings privately to the repository owner; no public tracker or security contact has been configured yet.

Never include real operator profiles, journal entries, coordinates, Wi-Fi credentials, SSH host keys, API keys, or built image secrets in a report. The image build must generate host keys on first boot. Ollama is compile-disabled in embedded profiles, and no service should listen beyond SSH unless the operator explicitly enables it.

Before any public release, configure a monitored security contact, a supported-version table, signed checksums, an SBOM, dependency scanning, and a response/patch publication process.

