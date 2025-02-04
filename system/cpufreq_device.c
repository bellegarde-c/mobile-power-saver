/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>

#include <gio/gio.h>

#include "cpufreq_device.h"
#include "../common/define.h"

/* struct _CpufreqDevicePrivate { */
/* }; */

G_DEFINE_TYPE_WITH_CODE (
    CpufreqDevice,
    cpufreq_device,
    TYPE_FREQ_DEVICE,
    /* G_ADD_PRIVATE (CpufreqDevice) */
)

static void
cpufreq_device_dispose (GObject *cpufreq_device)
{
    G_OBJECT_CLASS (cpufreq_device_parent_class)->dispose (cpufreq_device);
}

static void
cpufreq_device_finalize (GObject *cpufreq_device)
{
    G_OBJECT_CLASS (cpufreq_device_parent_class)->finalize (cpufreq_device);
}

static void
cpufreq_device_class_init (CpufreqDeviceClass *klass)
{
    GObjectClass *freq_device_class;

    freq_device_class = G_OBJECT_CLASS (klass);
    freq_device_class->dispose = cpufreq_device_dispose;
    freq_device_class->finalize = cpufreq_device_finalize;
}

static void
cpufreq_device_init (CpufreqDevice *self)
{
    self->priv = cpufreq_device_get_instance_private (self);

    freq_device_set_sysfs_settings (
        FREQ_DEVICE (self), CPUFREQ_POLICIES_DIR, "scaling_governor"
    );
}

/**
 * cpufreq_device_new:
 *
 * Creates a new #CpufreqDevice
 *
 * Returns: (transfer full): a new #CpufreqDevice
 *
 **/
GObject *
cpufreq_device_new (void)
{
    GObject *cpufreq_device;

    cpufreq_device = g_object_new (TYPE_CPUFREQ_DEVICE, NULL);

    return cpufreq_device;
}

/**
 * cpufreq_is_little:
 *
 * check #CpufreqDevice type
 *
 * @param #CpufreqDevice
 *
 * Returns: True if #CpufreqDevice is little cluster
 *
 **/
gboolean
cpufreq_is_little (CpufreqDevice *self)
{
    const char *devname = freq_device_get_name (FREQ_DEVICE (self));

    return g_strcmp0 (devname, "policy0") == 0;
}