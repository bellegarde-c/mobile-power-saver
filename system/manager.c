/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>

#include <gio/gio.h>

#include "bus.h"
#include "cpufreq.h"
#include "config.h"
#include "devfreq.h"
#include "processes.h"
#include "kernel_settings.h"
#include "logind.h"
#include "manager.h"

#ifdef WIFI_ENABLED
#include "wifi.h"
#endif

#include "../common/define.h"
#include "../common/services.h"
#include "../common/utils.h"

#define APPLY_DELAY 500

struct _ManagerPrivate {
    Cpufreq *cpufreq;
    Devfreq *devfreq;
    KernelSettings *kernel_settings;
    Processes *processes;
    Services *services;
#ifdef WIFI_ENABLED
    WiFi *wifi;
#endif

    gboolean screen_off_power_saving;
    GList *screen_off_suspend_processes;
    GList *screen_off_background_processes;
    GList *screen_off_suspend_services;

    char *cgroups_user_services_dir;
    char *cgroups_user_apps_dir;

    gboolean radio_power_saving;
};

G_DEFINE_TYPE_WITH_CODE (
    Manager,
    manager,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Manager)
)

static const char *
get_governor_from_power_profile (PowerProfile power_profile) {
    if (power_profile == POWER_PROFILE_POWER_SAVER)
        return "powersave";
    if (power_profile == POWER_PROFILE_PERFORMANCE)
        return "performance";
    return NULL;
}

static void
on_screen_state_changed (gpointer ignore,
                         gboolean screen_on,
                         gpointer user_data)
{
    Manager *self = MANAGER (user_data);
    GList *system_cgroups = get_subcgroups (CGROUPS_SYSTEM_SERVICES_DIR);
    GList *user_cgroups = get_subcgroups (self->priv->cgroups_user_services_dir);
    GList *apps_cgroups = get_subcgroups (self->priv->cgroups_user_apps_dir);

    if (self->priv->screen_off_power_saving) {
        bus_screen_state_changed (bus_get_default (), screen_on);
        devfreq_set_powersave (self->priv->devfreq, !screen_on);
        kernel_settings_set_powersave (self->priv->kernel_settings, !screen_on);
#ifdef WIFI_ENABLED
        if (self->priv->radio_power_saving)
            wifi_set_powersave (self->priv->wifi, !screen_on);
#endif

        if (screen_on) {
            cpufreq_set_powersave (self->priv->cpufreq, FALSE, TRUE);
            processes_resume (
                self->priv->processes,
                self->priv->screen_off_suspend_processes
            );
            processes_names_set_system_background (
                self->priv->processes,
                self->priv->screen_off_background_processes
            );
            processes_cgroups_set_system_background (
                self->priv->processes,
                system_cgroups
            );
            processes_cgroups_set_system_background (
                self->priv->processes,
                user_cgroups
            );
            processes_cgroups_set_system_background (
                self->priv->processes,
                apps_cgroups
            );
            services_unfreeze (
                self->priv->services,
                self->priv->screen_off_suspend_services
            );
        } else {
            cpufreq_set_powersave (self->priv->cpufreq, TRUE, FALSE);
            processes_update (self->priv->processes);
            processes_suspend (
                self->priv->processes,
                self->priv->screen_off_suspend_processes
            );
            processes_names_set_background (
                self->priv->processes,
                self->priv->screen_off_background_processes
            );
            processes_cgroups_set_background (
                self->priv->processes,
                system_cgroups
            );
            processes_cgroups_set_background (
                self->priv->processes,
                user_cgroups
            );
            processes_cgroups_set_background (
                self->priv->processes,
                apps_cgroups
            );
            services_freeze (
                self->priv->services,
                self->priv->screen_off_suspend_services
            );
        }
    }

    g_list_free_full (system_cgroups, g_free);
    g_list_free_full (user_cgroups, g_free);
}

static void
on_power_saving_mode_changed (Bus         *bus,
                              PowerProfile power_profile,
                              gpointer     user_data)
{
    Manager *self = MANAGER (user_data);
    const char *governor = get_governor_from_power_profile (power_profile);

    cpufreq_set_governor (self->priv->cpufreq, governor);
    devfreq_set_governor (self->priv->devfreq, governor);
}

static void
on_screen_off_power_saving_changed (Bus      *bus,
                                    gboolean  screen_off_power_saving,
                                    gpointer  user_data)
{
    Manager *self = MANAGER (user_data);

    self->priv->screen_off_power_saving = screen_off_power_saving;

    if (!self->priv->screen_off_power_saving) {
        cpufreq_set_powersave (self->priv->cpufreq, FALSE, TRUE);
        devfreq_set_powersave (self->priv->devfreq, FALSE);
    }
}

static void
on_screen_off_suspend_processes_changed (Bus      *bus,
                                         GVariant *value,
                                         gpointer  user_data)
{
    Manager *self = MANAGER (user_data);
    g_autoptr (GVariantIter) iter;
    const char *process;

    g_list_free_full (
        self->priv->screen_off_suspend_processes, g_free
    );
    self->priv->screen_off_suspend_processes = NULL;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &process)) {
        self->priv->screen_off_suspend_processes =
            g_list_append (
                self->priv->screen_off_suspend_processes, g_strdup (process)
            );
    }
    g_variant_unref (value);
}

static void
on_screen_off_background_processes_changed (Bus      *bus,
                                            GVariant *value,
                                            gpointer  user_data)
{
    Manager *self = MANAGER (user_data);
    g_autoptr (GVariantIter) iter;
    const char *process;

    g_list_free_full (
        self->priv->screen_off_background_processes, g_free
    );
    self->priv->screen_off_background_processes = NULL;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &process)) {
        self->priv->screen_off_background_processes =
            g_list_append (
                self->priv->screen_off_background_processes, g_strdup (process)
            );
    }
    g_variant_unref (value);
}

static void
on_devfreq_blacklist_setted (Bus      *bus,
                             GVariant *value,
                             gpointer  user_data)
{
    Manager *self = MANAGER (user_data);
    g_autoptr (GVariantIter) iter;
    const char *device;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &device)) {
        devfreq_blacklist (self->priv->devfreq, device);
    }
    g_variant_unref (value);
}

static void
on_cpuset_blacklist_setted (Bus      *bus,
                            GVariant *value,
                            gpointer  user_data)
{
    Manager *self = MANAGER (user_data);
    GList *blacklist = NULL;
    g_autoptr (GVariantIter) iter;
    const char *item;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &item)) {
        blacklist = g_list_append (blacklist, g_strdup (item));
    }

    processes_cpuset_set_blacklist (self->priv->processes, blacklist);

    g_variant_unref (value);
}

static void
on_cgroups_user_services_dir_setted (Bus      *bus,
                                     GVariant *value,
                                     gpointer  user_data)
{
    Manager *self = MANAGER (user_data);

    if (self->priv->cgroups_user_services_dir != NULL) {
        g_free (self->priv->cgroups_user_services_dir);
    }

    g_variant_get (value, "s", &self->priv->cgroups_user_services_dir);
}

static void
on_cgroups_user_apps_dir_setted (Bus      *bus,
                                 GVariant *value,
                                 gpointer  user_data)
{
    Manager *self = MANAGER (user_data);

    if (self->priv->cgroups_user_apps_dir != NULL) {
        g_free (self->priv->cgroups_user_apps_dir);
    }

    g_variant_get (value, "s", &self->priv->cgroups_user_apps_dir);
}

static void
on_radio_power_saving_changed (Bus      *bus,
                               gboolean  radio_power_saving,
                               gpointer  user_data)
{
    Manager *self = MANAGER (user_data);

    self->priv->radio_power_saving = radio_power_saving;
}

static void
on_little_cluster_powersave_changed (Bus      *bus,
                                     gboolean  enabled,
                                     gpointer  user_data)
{
    Manager *self = MANAGER (user_data);

    cpufreq_set_powersave (self->priv->cpufreq, TRUE, enabled);
}


static void
on_screen_off_suspend_services_changed (Bus      *bus,
                                        GVariant *value,
                                        gpointer  user_data)
{
    Manager *self = MANAGER (user_data);
    g_autoptr (GVariantIter) iter;
    char *service;

    g_list_free_full (
        self->priv->screen_off_suspend_services,
        g_free
    );
    self->priv->screen_off_suspend_services = NULL;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &service)) {
        self->priv->screen_off_suspend_services =
            g_list_append (
                self->priv->screen_off_suspend_services, g_strdup (service)
            );
    }
    g_variant_unref (value);
}

static void
manager_dispose (GObject *manager)
{
    Manager *self = MANAGER (manager);

    g_clear_object (&self->priv->cpufreq);
    g_clear_object (&self->priv->devfreq);
    g_clear_object (&self->priv->kernel_settings);
    g_clear_object (&self->priv->processes);
    g_clear_object (&self->priv->services);
#ifdef WIFI_ENABLED
    g_clear_object (&self->priv->wifi);
#endif

    G_OBJECT_CLASS (manager_parent_class)->dispose (manager);
}

static void
manager_finalize (GObject *manager)
{
    Manager *self = MANAGER (manager);

    g_list_free_full (
        self->priv->screen_off_suspend_processes, g_free
    );
    g_list_free_full (
        self->priv->screen_off_background_processes, g_free
    );
    g_list_free_full (
        self->priv->screen_off_suspend_services, g_free
    );

    if (self->priv->cgroups_user_services_dir != NULL) {
        g_free (self->priv->cgroups_user_services_dir);
    }

    if (self->priv->cgroups_user_apps_dir != NULL) {
        g_free (self->priv->cgroups_user_apps_dir);
    }

    G_OBJECT_CLASS (manager_parent_class)->finalize (manager);
}

static void
manager_class_init (ManagerClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = manager_dispose;
    object_class->finalize = manager_finalize;
}

static void
manager_init (Manager *self)
{
    self->priv = manager_get_instance_private (self);

    self->priv->cpufreq = CPUFREQ (cpufreq_new ());
    self->priv->devfreq = DEVFREQ (devfreq_new ());
    self->priv->kernel_settings = KERNEL_SETTINGS (kernel_settings_new ());
    self->priv->processes = PROCESSES (processes_new ());
    self->priv->services = SERVICES (services_new (G_BUS_TYPE_SYSTEM));
#ifdef WIFI_ENABLED
    self->priv->wifi = WIFI (wifi_new ());
#endif

    self->priv->screen_off_power_saving = TRUE;
    self->priv->radio_power_saving = FALSE;
    self->priv->screen_off_suspend_processes = NULL;
    self->priv->cgroups_user_services_dir = NULL;
    self->priv->cgroups_user_apps_dir = NULL;

    g_signal_connect (
        logind_get_default (),
        "screen-state-changed",
        G_CALLBACK (on_screen_state_changed),
        self
    );

    g_signal_connect (
        bus_get_default (),
        "power-saving-mode-changed",
        G_CALLBACK (on_power_saving_mode_changed),
        self
    );

    g_signal_connect (
        bus_get_default (),
        "screen-off-power-saving-changed",
        G_CALLBACK (on_screen_off_power_saving_changed),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "screen-off-suspend-processes-changed",
        G_CALLBACK (on_screen_off_suspend_processes_changed),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "screen-off-background-processes-changed",
        G_CALLBACK (on_screen_off_background_processes_changed),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "screen-off-suspend-services-changed",
        G_CALLBACK (on_screen_off_suspend_services_changed),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "devfreq-blacklist-setted",
        G_CALLBACK (on_devfreq_blacklist_setted),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "cpuset-blacklist-setted",
        G_CALLBACK (on_cpuset_blacklist_setted),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "cgroups-user-services-dir-setted",
        G_CALLBACK (on_cgroups_user_services_dir_setted),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "cgroups-user-apps-dir-setted",
        G_CALLBACK (on_cgroups_user_apps_dir_setted),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "little-cluster-powersave-changed",
        G_CALLBACK (on_little_cluster_powersave_changed),
        self
    );
    g_signal_connect (
        bus_get_default (),
        "radio-power-saving-changed",
        G_CALLBACK (on_radio_power_saving_changed),
        self
    );
}

/**
 * manager_new:
 *
 * Creates a new #Manager
 *
 * Returns: (transfer full): a new #Manager
 *
 **/
GObject *
manager_new (void)
{
    GObject *manager;

    manager = g_object_new (TYPE_MANAGER, NULL);

    return manager;
}
