# Releasing

## Build Artifacts

Create a release tarball:

```sh
make dist
```

This generates:

- `cupidwm-<VERSION>.tar.gz`

## Validate Tarball

```sh
make distcheck
```

`distcheck` confirms key sources/docs/licenses/tests are present in the tarball.

## Packaging Notes

- `.github` is copied only if it exists.
- Local `config.h` is excluded from release archives.
- Release tarball includes docs, scripts, tests, tools, image assets, and third-party notices.
