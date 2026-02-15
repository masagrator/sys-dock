# sys-dock

A Nintendo Switch system module that patches the docked DisplayPort link training to allow higher bandwidth modes, enabling higher refresh rates.

> [!IMPORTANT]
> **This sysmodule is only useful for the OLED Switch.** Exclusively the OLED model has all 4 DP lanes wired to the USB-C port, and performs software downgrades that this module can disable. All other models have only 2 DP lanes physically wired, and perform no software downgrades, so this module is useless on them.

> [!WARNING]
> This sysmodule changes how your connection with the dock works. It may have unexpected results that can damage hardware. Contributors of this repository are not taking any responsibility for damage resulted from using this software.

## Simple Guide

- Up to 75 Hz at 1080p and 120 Hz at 720p already works in [FPSLocker](https://github.com/masagrator/FPSLocker), no need for this module. Go to `FPSLocker > Display settings > Docked Settings`.

- For up to 120 Hz at 1080p on OLED Switch, follow below:

  1. Install this system module by extracting the release ZIP at the root of your SD card.
  2. Use the "sys-dock" overlay to enable patches, trying each step until it works:
     1. Enable `no_bw_downgrade` patch, reboot, then try FPSLocker again.
     2. Didn't work? Also enable `no_lane_downgrade`.
     3. Still didn't work and using a 3rd party dock? Also enable `force_dp_mode_c`.
     4. Still didn't work? Enable `force_bw_downgrade` (this automatically disables `no_bw_downgrade` if active).


While up to 240 Hz is theoretically possible on the OLED Switch, it is not enabled in FPSLocker due to high risk of crashing at system applets.

## Bandwidth Requirements

| Resolution | Required DP Mode                |
| ---------- | ------------------------------- |
| 1080p75    | HBR x2 (default on OLED Switch) |
| 1080p120   | HBR2 x2 or HBR x4               |
| 1080p240   | HBR2 x4                         |

## Patch Details

| Name               | Description                                                  |
| ------------------ | ------------------------------------------------------------ |
| no_bw_downgrade    | Disables HBR2 (5.4 Gbps) → HBR (2.7 Gbps) DP bandwidth downgrade when using 2 DP lanes on OLED models. All other cases already use full bandwidth.<br/>Unlocks **HBR2 x2** on OLED. |
| no_lane_downgrade  | Disables 4 → 2 DP lane downgrade on OLED.<br />Unlocks **HBR2 x4**, or **HBR x4** on OLED if `force_bw_downgrade` is also enabled. |
| force_dp_mode_c    | Forces DP Alt Mode pin assignment C, even if not advertised by your dock/hub. This can enable 4 DP lanes on OLED Switch and 3rd party docks that emulate a non-OLED dock but still support pin assignment C. |
| force_bw_downgrade | Forces HBR2 (5.4 Gbps) → HBR (2.7 Gbps) DP bandwidth downgrade.<br />Unlocks **HBR x4** on OLED when combined with `no_lane_downgrade`. |

## Config

**sys-dock** features a simple config at `/config/sys-dock/config.ini`, generated on first run. It can be manually edited or updated using the overlay.

## Overlay

The overlay can be used to change config options and to see what patches are applied.

- **Unpatched** — the patch target wasn't found (likely not applicable).
- **Patched (green)** — patched by sys-dock.
- **Patched (yellow)** — already patched by sigpatches or a custom Atmosphere build.

## Credits / Thanks

The author of this toolset wants to stay anonymous. Me — masagrator — was only asked to host and maintain this.

This software was made based on work by:

- MrDude
- BornToHonk (farni)
- TeJay
- ArchBox
- [minIni](https://github.com/compuphase/minIni)
- [libtesla](https://github.com/WerWolv/libtesla)
