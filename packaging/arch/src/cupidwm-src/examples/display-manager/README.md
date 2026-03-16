# Display Manager Session Recipes

cupidwm installs an Xsession desktop entry at:

- `/usr/share/xsessions/cupidwm.desktop`

This is installed automatically by:

```sh
sudo make install
```

## GDM / SDDM / LightDM

1. Install cupidwm (`sudo make install`).
2. Log out to your display manager.
3. Select session `cupidwm` from the session chooser.
4. Log in.

## Custom local desktop entry

If your distro uses a custom session path, copy the desktop file:

```sh
sudo install -Dm644 cupidwm.desktop /usr/local/share/xsessions/cupidwm.desktop
```

Some environments also read:

- `/usr/share/wayland-sessions/` (not applicable for this X11 WM)
- `/usr/local/share/xsessions/`

Use whichever path your display manager is configured to scan.
