# Security Policy

## Supported versions

AYTHER Tessera has no stable release. Every published artifact is a
`0.x.y-rc.N` pre-release; fixes land on `main` and ship in the next candidate,
and earlier candidates are not patched. Reports are evaluated against `main`.

## Reporting a vulnerability

Do not open a public issue for an unpatched vulnerability.

Use **Report a vulnerability** under the repository's **Security** tab (GitHub
private vulnerability reporting, enabled for this repository). Include whenever
possible:

- affected version, tag, or commit;
- platform and toolchain;
- description and impact;
- minimal reproduction steps, with a synthetic or homebrew ROM rather than
  commercial content;
- known mitigations;
- whether you can validate a fix.

## Priority scope

Tessera runs user-supplied ROMs inside a host process, so these areas are
especially sensitive:

- memory corruption or out-of-bounds access driven by ROM content, cartridge
  headers, or emulated bus, DMA, and VDP behaviour;
- unsafe parsing of save states, checkpoints, or persistent memory supplied by
  the host;
- violations of the C API contract: ownership, lifetime, or thread-safety
  errors across the boundary, and buffers written past the size the host gave;
- denial of service through content that hangs or exhausts the host;
- the release pipeline: checksums or provenance that do not match the published
  bytes.

Report a defect in how AYTHER Engine loads, configures, or trusts Tessera to
the [AYTHER Engine](https://github.com/Ayther-Dev/AYTHER-Engine) maintainers.

## Response process

The maintainers acknowledge receipt when possible, assess scope and severity,
prepare a fix, and coordinate disclosure. Information that could enable
exploitation is not published before a mitigation is available. Once a fix is
available, affected versions and the recommended update are documented in a
GitHub Security Advisory.

## Out of scope

- ROMs, BIOS images, and third-party tools not distributed by AYTHER Tessera.
- Inaccurate emulation that has no memory-safety or availability impact; report
  it as a regular issue.
- Issues that require deliberately modifying a local binary after it has
  passed integrity checks.
