# Contributing

Keep changes focused, preserve upstream notices, and run `./scripts/test-host.sh` before submitting a patch. Native changes also need an OpenOrbis build; host tests do not emulate the console decoder or GPU.

Describe expected behavior, actual behavior, and the Nexo X4 version. Never attach account tokens, device codes, session files, console identifiers, network addresses, memory dumps, or unreviewed runtime logs. Use a minimal synthetic reproduction whenever possible.

A release must include its corresponding application source, the complete patched dependency source bundle, retained licenses, and SHA-256 checksums. Keep generated files and local build inputs in ignored `build/`, `dist/`, and `.local/` directories.
