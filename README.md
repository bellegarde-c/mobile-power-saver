# Mobile Power Saver

Mobile Power Saver enables power saving on mobile devices:
- Provides power-profile-daemon basic API compatibility
- Puts devices in low power when screen is off
- Suspends services/processes when screen is off
- Freezes applications (only when screen is off for now)

## Settings

GNOME Control Center patches are available here:
https://github.com/droidian/gnome-control-center/commits/droidian/panels/power

![Power Settings](https://adishatz.org/data/mps_battery.png)
![App Settings](https://adishatz.org/data/mps_app_suspend.png)

## Support for userspace freezing

[Overrides](Overrides.md)

## Depends on

- `glib2`
- `meson`
- `ninja`

## Building from Git

```bash
$ meson builddir --prefix=/usr

$ sudo ninja -C builddir install
```
