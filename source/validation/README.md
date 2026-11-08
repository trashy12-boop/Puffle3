The `rpcs3-session.py` script records the actual sandbox test procedure. It
contains this session's absolute tool and fixture paths, so it is evidence,
not a turnkey RPCS3 installation script. It boots the native ELF, launches the
legal fixtures, emulates controller keys, exercises pause/restart/settings,
loads AVM2 trace, audio and save fixtures, and captures screenshots.

Firmware is downloaded separately from Sony; no firmware is included.
