# sys-dock

A Nintendo Switch system module that patches the docked DisplayPort link training to allow higher bandwidth modes, enabling higher refresh rates.

> [!WARNING]
> This sysmodule changes how your connection with the dock works. It may have unexpected results that can damage hardware. Contributors of this repository are not taking any responsibility for damage resulted from using this software.

## Simple Guide

> [!TIP]
> To activate any high refresh rate, you need to install [FPSLocker](https://github.com/masagrator/FPSLocker). Go to `FPSLocker > Display settings > Docked Settings`. By default, at 1080p OLED Switches are limited to 75Hz, while non-OLED Switches are limited to 120Hz. The OLED limit can be unlocked up to 240Hz by using this sysmodule.

First, install this system module by extracting the release ZIP at the root of your SD card.

### OLED Switch Only

  Use the "sys-dock" overlay to enable patches, trying each step until 120Hz works in FPSLocker:
  1. Enable `no_bw_downgrade` patch, reboot, then try FPSLocker again.
  2. Didn't work? Also enable `no_lane_downgrade`.
  3. Still didn't work and using a 3rd party dock? Also enable `force_dp_mode_c`.
  4. Still didn't work? Enable `force_bw_downgrade` (this automatically disables `no_bw_downgrade` if active).
  
> [!NOTE]
> While up to 240Hz is theoretically possible on the OLED Switch, it is not enabled in FPSLocker due to high risk of crashing at system applets.

### All Switches (including OLED)

> [!IMPORTANT]
> You may experience horizontal line corruption at 120Hz and above if you reboot while docked and every time after you resume the console from sleep. This can be mitigated by enabling the `force_full_render_pass` patch. Note that it may add up to a frame of latency. Alternatively, just re-plug your Switch.

## Bandwidth Requirements

| Resolution | Required DP Mode                |
| ---------- | ------------------------------- |
| 1080p75    | HBR x2 (default on OLED Switch) |
| 1080p120   | HBR2 x2 or HBR x4               |
| 1080p240   | HBR2 x4                         |

## Patch Details

| Name                   | Description                                                  |
| ---------------------- | ------------------------------------------------------------ |
| no_bw_downgrade        | Disables HBR2 (5.4 Gbps) → HBR (2.7 Gbps) DP bandwidth downgrade when using 2 DP lanes on OLED models. All other cases already use full bandwidth.<br/>Unlocks **HBR2 x2** on OLED. |
| no_lane_downgrade      | Disables 4 → 2 DP lane downgrade on OLED.<br />Unlocks **HBR2 x4**, or **HBR x4** on OLED if `force_bw_downgrade` is also enabled. |
| force_dp_mode_c        | Forces DP Alt Mode pin assignment C, even if not advertised by your dock/hub. This can enable 4 DP lanes on OLED Switch and 3rd party docks that emulate a non-OLED dock but still support pin assignment C. |
| force_bw_downgrade     | Forces HBR2 (5.4 Gbps) → HBR (2.7 Gbps) DP bandwidth downgrade.<br />Unlocks **HBR x4** on OLED when combined with `no_lane_downgrade`. |
| force_full_render_pass | Forces full render pass to always be used in the Display Controller.<br />Mitigates the horizontal line corruption that may occur when rebooting while docked or after resuming from sleep. May add up to a frame of latency. |

## Config

**sys-dock** features a simple config at `/config/sys-dock/config.ini`, generated on first run. It can be manually edited or updated using the overlay.

## Overlay

The overlay can be used to change config options and to see what patches are applied.

- **Unpatched** — the patch target wasn't found (likely not applicable, or needs update).
- **Patched (green)** — patched by sys-dock.
- **Patched (yellow)** — already patched by some other software.

## Credits / Thanks

The author of this toolset wants to stay anonymous. Me - masagrator - was only asked to host and maintain this.

This software was made based on work by:

- TotalJustice
- impeeza
- MrDude
- BornToHonk (farni)
- TeJay
- ArchBox
- [minIni](https://github.com/compuphase/minIni)
- [libtesla](https://github.com/WerWolv/libtesla)
