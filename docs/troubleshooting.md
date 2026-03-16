# Troubleshooting

## WM does not start

1. Check X display/session:
   - `echo "$DISPLAY"`
2. Run with stderr visible:
   - `./cupidwm`
3. Validate build:
   - `make clean && make`

## Broken local config

Reset your local config to defaults and rebuild:

```sh
cp config.def.h config.h
make clean && make
```

## IPC command fails

1. Verify IPC is enabled in `config.h` (`ipc_enable = 1`).
2. Resolve expected socket path:
   - `cupidwm --print-ipc-socket`
3. Inspect active path from running WM:
   - `xprop -root _CUPIDWM_IPC_SOCKET`
4. Query socket directly:
   - `cupidwmctl --print-socket`
   - `cupidwmctl status`

## Integration tests are skipped

`make check` skips Xephyr suites if test prerequisites are missing.

Install dependencies and re-run:

```sh
./scripts/install-deps.sh --yes
make check
```

## Capturing crash diagnostics

Build sanitizer binary and run the test gate:

```sh
make debug
make check
```

If a crash occurs, ASan/UBSan diagnostics are printed to test logs (`wm.log`)
in the temporary test log directory.
