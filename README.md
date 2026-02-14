# sys-dock

A Nintendo Switch system module that patches various things to allow you to increase your docked display bandwidth for higher refresh rate.

> [!WARNING]
> This sysmodule changes how your connection with dock works. It may have unexpected results that can damage hardware. Contributors of this repository are not taking any responsibility for damage resulted from using this software.

## Simple Guide

- Up to 75 hz at 1080p and 120 hz at 720p already works in [FPSLocker](https://github.com/masagrator/FPSLocker), no need for this module. Go to `FPSLocker > Display settings > Docked Settings`.

- For up to 120 hz at 1080p on OLED Switch, follow below:

   1. Install this system module by extracting the release ZIP at the root of your SD card.
   2. Use the "sys-dock" overlay to enable at least one patch:
      1. Enable `no_bw_downgrade` patch, reboot, then try FPSLocker again.
      2. The above didn't work? Add `no_lane_downgrade` patch.
      3. (Use this only with 3rd party docks) The above didn't work? Add `force_dp_mode_c` patch.
      4. The above didn't work, or you get horizontal corruption lines? Add `force_bw_downgrade` patch (this will automatically disable `no_bw_downgrade` patch if active).
   
   While up to 240 hz is theoretically possible on the OLED Switch, it is not enabled in FPSLocker due to high risk of crashing at system applets.

- For up to 120 hz at 1080p on non-OLED Switch, it may or not work in your case. Fixing issue for people who cannot get above 75 hz is still under investigation.

## Requirements

| Resolution | Mode                                        |
| ---------- | ------------------------------------------- |
| 1080p75    | HBR x2 (default on OLED)                    |
| 1080p120   | HBR2 x2 or HBR x4                           |
| 1080p240   | HBR2 x4                                     |

## Patch details

| Name               | Description                                                  |
| ------------------ | ------------------------------------------------------------ |
| no_lane_downgrade  | Disables 4 -> 2 DP lane downgrade for all models.<br />- Unlocks **HBR x4** if `force_bw_downgrade` is also enabled.<br />- Otherwise unlocks **HBR2 x4**.<br />NOTE: Rebooting while docked will default to 2 lanes, re-dock to switch to 4. |
| no_bw_downgrade    | Disables HBR2 (5.4gbps) -> HBR (2.7gbps) DP bandwidth downgrade when 2 DP lanes on OLED models. All other cases use full bandwidth.<br />- Unlocks **HBR2 x2** on OLED. |
| force_bw_downgrade | Forces HBR2 (5.4gbps) -> HBR (2.7gbps) DP bandwidth downgrade.<br />- Unlocks **HBR x4** if `no_lane_downgrade` is also enabled. |
| force_dp_mode_c    | Forces DP pin mode C, even if not advertised as supported by your hub. This can enable 4 DP lanes on 3rd party hubs that emulate a non-OLED dock, but still support pin mode C under the hood. |

## Config

**sys-dock** features a simple config. This can be manually edited or updated using the overlay.

The configuration file can be found in `/config/sys-dock/config.ini`. The file is generated once the module is ran for the first time.

## Overlay

The overlay can be used to change the config options and to see what patches are applied.

- Unpatched means the patch wasn't applied (likely not found).
- Patched (green) means it was patched by sys-dock.
- Patched (yellow) means it was already patched, likely by sigpatches or a custom Atmosphere build.

## Credits / Thanks

Author of this whole toolset wants to stay anonymous. Me - masagrator - was only asked to host this and maintain.

This software was made based on stuff done by:

- MrDude
- BornToHonk (farni)
- TeJay
- ArchBox
- [minIni](https://github.com/compuphase/minIni)
- [libtesla](https://github.com/WerWolv/libtesla)
