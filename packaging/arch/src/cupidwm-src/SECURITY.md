# Security Policy

## Scope

cupidwm is an X11 window manager and relies on system X11 libraries.

## Reporting a Vulnerability

Report security issues privately to the project maintainer first. Include:

- impact summary
- affected version/commit
- reproduction steps
- proposed fix (if available)

## Dependency Security Expectations

Keep your system X11 stack patched through your distribution updates.

In particular, treat `libX11` updates as required maintenance. Known examples
called out during project review include:

- CVE-2025-26597 (`libX11`)
- CVE-2020-14363 (`libX11`)

cupidwm does not vendor `libX11`; security posture depends on the host distro
shipping patched packages.

## Hardening Notes

- Sanitizer builds (`make debug`) are for testing, not production deployment.
- Run `make check` before release tagging.
- Keep `status_allow_external_cmd` disabled unless you trust the configured command.
- `status_external_cmd` is executed through `popen(3)` (shell execution). Treat it as
  code execution in your session context.
- If you enable external status commands, use explicit absolute paths and avoid
  untrusted input expansion.
- Keep `ipc_enable` disabled unless you need local control via `cupidwmctl`.
- IPC sockets are created with mode `0600`; prefer paths in `$XDG_RUNTIME_DIR`.
