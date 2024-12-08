/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#ifndef DEFINE_H
#define DEFINE_H

#define CPUFREQ_POLICIES_DIR "/sys/devices/system/cpu/cpufreq/"
#define DEVFREQ_DIR "/sys/class/devfreq/"
#define CGROUPS_APPS_DIR "/sys/fs/cgroup/user.slice/user-%d.slice/user@%d.service/app.slice"
#define CGROUPS_USER_SERVICES_DIR "/sys/fs/cgroup/user.slice/user-%d.slice/user@%d.service/session.slice"
#define CGROUPS_SYSTEM_SERVICES_DIR "/sys/fs/cgroup/system.slice"

#define DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"

typedef enum {
    POWER_PROFILE_POWER_SAVER,
    POWER_PROFILE_BALANCED,
    POWER_PROFILE_PERFORMANCE,
    POWER_PROFILE_LAST
} PowerProfile;

#endif
