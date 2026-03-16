# Ecosystem and Packaging

This page collects practical onboarding files and packaging helpers.

## Sample Configs

- sxhkd keybindings sample: `examples/sxhkd/cupidwm.sxhkdrc`
- xinit startup sample: `examples/xinitrc`
- display-manager session notes: `examples/display-manager/README.md`

Copy and adapt locally:

```sh
mkdir -p ~/.config/sxhkd
cp examples/sxhkd/cupidwm.sxhkdrc ~/.config/sxhkd/sxhkdrc
cp examples/xinitrc ~/.xinitrc
chmod +x ~/.xinitrc
```

## Shell Completion

A bash completion file for `cupidwmctl` is included:

- `completions/cupidwmctl.bash`

Enable for current shell:

```sh
source completions/cupidwmctl.bash
```

Install system-wide (bash-completion):

```sh
sudo install -Dm644 completions/cupidwmctl.bash \
  /usr/share/bash-completion/completions/cupidwmctl
```

## Packaging Recipes

Current distro recipe:

- Arch Linux PKGBUILD: `packaging/arch/PKGBUILD`

Build locally from the repository checkout:

```sh
cd packaging/arch
makepkg -si
```

This recipe stages a clean source copy under `packaging/arch/src/` so it does
not depend on a prebuilt source tarball in that directory.

From this repository checkout, build/install with:

```sh
cd packaging/arch
makepkg -si
```

The recipe builds from the local workspace root and does not require a
pre-generated source tarball in `packaging/arch`.

The PKGBUILD installs:

- `cupidwm`, `cupidwmctl`, manpages, and desktop session file
- bash completion for `cupidwmctl`
- example config files under `/usr/share/doc/cupidwm/examples`

## Validation

Run ecosystem artifact checks:

```sh
make test-packaging
```
