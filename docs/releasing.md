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

`distcheck` fails if extraction fails or a required source, document, license, or
test file is missing. Its temporary extraction directory is removed on success
and failure.

Run `make test-distcheck` to test the validator against complete, corrupt, and
incomplete archives.

## Packaging Notes

- `.github` is copied only if it exists.
- Local `config.h` is excluded from release archives.
- Release tarball includes docs, scripts, tests, tools, image assets, and third-party notices.
- Release tarball includes ecosystem artifacts:
	- `examples/` sample configs and session recipes
	- `completions/cupidwmctl.bash`
	- `packaging/arch/PKGBUILD`

Run ecosystem/packaging sanity checks before publishing:

```sh
make test-packaging
```
