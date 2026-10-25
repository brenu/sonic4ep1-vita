# VitaCompanion workflow

The optional [`tools/vita_test.py`](../tools/vita_test.py) helper uses VitaCompanion's FTP and command services to collect logs, deploy a code-only VPK update, verify the transferred files, and request launch. It does not install VitaCompanion, press game buttons, or replace the user's visual/audio checks.

Install and configure VitaCompanion on the Vita following its upstream instructions. Ensure the PC and Vita are on a reachable network, then set the device address in a shell variable (do not commit it):

```sh
export VITA_IP=192.0.2.10  # replace with the Vita's current address
python3 tools/vita_test.py status "$VITA_IP" --ftp-mount ux0
```

To deploy an already-installed app, close Sonic manually first and use `--app-closed` only after confirming it has stopped:

```sh
python3 tools/vita_test.py deploy "$VITA_IP" \
  --ftp-mount ux0 \
  --vpk work/build/sonic4ep1_wip.vpk \
  --app-closed
```

After the Vita test, close Sonic and collect its log and recent crash dumps:

```sh
python3 tools/vita_test.py collect "$VITA_IP" --ftp-mount ux0
```

The helper validates the VPK title ID and required entries, backs up changed remote files and current diagnostics, verifies uploaded bytes, and preserves previous remote executables. It does not change storage configuration or game data. `Launched.` confirms the command was accepted, not that the game booted successfully.

If FTP exposes both `ux0` and `uma0`, select the mount verified in VitaShell. Do not reuse a mount assumption from another Vita setup.
