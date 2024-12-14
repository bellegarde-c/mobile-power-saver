/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <gio/gio.h>

#include "bus.h"
#include "config.h"
#include "settings.h"

/* signals */
enum
{
    SETTING_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _SettingsPrivate {
    GSettings *settings;
};

G_DEFINE_TYPE_WITH_CODE (
    Settings,
    settings,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Settings)
)

static void
on_setting_changed (GSettings  *settings,
                    const char *key,
                    gpointer    user_data)
{
    Settings *self = SETTINGS (user_data);
    g_autoptr (GVariant) value = g_settings_get_value (settings, key);

    g_signal_emit(
        self,
        signals[SETTING_CHANGED],
        0,
        key,
        value
    );
}

static gboolean
notify_settings (Settings *self)
{
    g_autoptr(GSettingsSchema) schema = g_settings_schema_source_lookup (
        g_settings_schema_source_get_default (),
        APP_ID,
        TRUE);
    char **keys = g_settings_schema_list_keys (schema);
    gint i;

    for (i = 0; keys[i] != NULL; i++) {
        on_setting_changed (
            self->priv->settings,
            keys[i],
            self
        );
    }

    return FALSE;
}

static void
settings_dispose (GObject *settings)
{
    Settings *self = SETTINGS (settings);

    g_clear_object (&self->priv->settings);

    G_OBJECT_CLASS (settings_parent_class)->dispose (settings);
}

static void
settings_finalize (GObject *settings)
{
    G_OBJECT_CLASS (settings_parent_class)->finalize (settings);
}

static void
settings_class_init (SettingsClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = settings_dispose;
    object_class->finalize = settings_finalize;

    signals[SETTING_CHANGED] = g_signal_new (
        "setting-changed",
        G_OBJECT_CLASS_TYPE (object_class),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL, NULL,
        G_TYPE_NONE,
        2,
        G_TYPE_STRING,
        G_TYPE_VARIANT
    );
}

static void
settings_init (Settings *self)
{
    self->priv = settings_get_instance_private (self);

    self->priv->settings = g_settings_new (APP_ID);

    g_signal_connect (
        self->priv->settings,
        "changed",
        G_CALLBACK (on_setting_changed),
        self
    );

    g_idle_add ((GSourceFunc) notify_settings, self);
}

/**
 * settings_new:
 *
 * Creates a new #Settings
 *
 * Returns: (transfer full): a new #Settings
 *
 **/
GObject *
settings_new (void)
{
    GObject *settings;

    settings = g_object_new (TYPE_SETTINGS, NULL);

    return settings;
}

static Settings *default_settings = NULL;
/**
 * settings_get_default:
 *
 * Gets the default #Settings.
 *
 * Return value: (transfer full): the default #Settings.
 */
Settings *
settings_get_default (void)
{
    if (default_settings == NULL) {
        default_settings = SETTINGS (settings_new ());
    }
    return default_settings;
}

/**
 * settings_free_default:
 *
 * Free the default #Settings.
 *
 */
void
settings_free_default (void)
{
    if (default_settings == NULL) {
        g_clear_object (&default_settings);
        default_settings = NULL;
    }
}

/**
 * settings_can_bluetooth_powersave:
 *
 * Check if an application scope allows Bluetooth power saving
 *
 * @self: a #Settings
 * @app_scope: application cgroup scope
 *
 * Returns: TRUE if Bluetooth powersaving can be enabled
 */
gboolean
settings_can_bluetooth_powersave (Settings   *self,
                                  const char *app_scope)
{
    g_autoptr (GVariant) value = g_settings_get_value (
        self->priv->settings, "bluetooth-power-saving-blacklist"
    );
    g_autoptr (GVariantIter) iter;
    const char *application;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &application)) {
        if (g_strrstr (app_scope, application) != NULL)
            return FALSE;
    }
    return TRUE;
}

/**
 * settings_can_freeze_app:
 *
 * Check if an application scope can be freezed
 *
 * @self: a #Settings
 * @app_scope: application cgroup scope
 *
 * Returns: TRUE if application scope can be freezed
 */
gboolean
settings_can_freeze_app (Settings   *self,
                         const char *app_scope)
{
    g_autoptr (GVariant) value = g_settings_get_value (
        self->priv->settings, "suspend-apps-blacklist"
    );
    g_autoptr (GVariantIter) iter;
    const char *application;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &application)) {
        if (g_strrstr (app_scope, application) != NULL)
            return FALSE;
    }
    return TRUE;
}


/**
 * settings_suspend_services:
 *
 * Check if services should be suspended
 *
 * @self: a #Settings
 *
 * Returns: TRUE if services should be suspended
 */
gboolean
settings_suspend_services (Settings *self)
{
    return g_settings_get_boolean (
        self->priv->settings, "suspend-services"
    );
}

/**
 * settings_get_suspend_services_blacklist
 *
 * Get services those can't be suspended
 *
 * @self: a #Settings
 *
 * Return value: (transfer full): services list.
 */
GList *
settings_get_suspend_services_blacklist (Settings *self)
{
    g_autoptr (GVariant) value = g_settings_get_value (
        self->priv->settings, "suspend-user-services-blacklist"
    );
    g_autoptr (GVariantIter) iter;
    const char *service;
    GList *services = NULL;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &service)) {
        services = g_list_append (services, g_strdup (service));
    }
    return services;
}
