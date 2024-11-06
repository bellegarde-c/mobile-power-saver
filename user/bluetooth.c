/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>

#include <gio/gio.h>

#include "bluetooth.h"
#include "settings.h"
#include "../common/define.h"
#include "../common/utils.h"

#define BLUEZ_DBUS_NAME               "org.bluez"
#define BLUEZ_DBUS_PATH               "/org/bluez/hci0"
#define BLUEZ_DBUS_ADAPTER_INTERFACE  "org.bluez.Adapter1"
#define BLUEZ_DBUS_DEVICE_INTERFACE   "org.bluez.Device1"

struct _BluetoothPrivate {
    GDBusObjectManager *object_manager;
    GDBusProxy *bluez_proxy;

    GList *connections;

    gboolean powered;
    gboolean connected;
    gboolean powersaving;
};

G_DEFINE_TYPE_WITH_CODE (
    Bluetooth,
    bluetooth,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Bluetooth)
)

static void
on_bluez_object_added (GDBusObjectManager *self,
                       GDBusObject        *object,
                       gpointer            user_data);
static void
on_bluez_proxy_properties (GDBusProxy  *proxy,
                           GVariant    *changed_properties,
                           char       **invalidated_properties,
                           gpointer     user_data);

static gboolean
can_powersave (Bluetooth *self)
{
    GList *applications = get_applications();
    const char *application;
    gboolean can_powersave = TRUE;

    /*
     * FIXME: Allow to set more apps via gsettings
     */
    GFOREACH (applications, application) {
        if (!settings_can_bluetooth_powersave (settings_get_default(),
                                               application)) {
            can_powersave = FALSE;
            break;
        }
    }
    g_list_free_full (applications, g_free);

    return can_powersave;
}

static void
check_existing_connections (Bluetooth *self)
{
    GList *connections, *c;

    connections = g_dbus_object_manager_get_objects(
        G_DBUS_OBJECT_MANAGER (self->priv->object_manager)
    );

    for (c = connections; c; c = g_list_next(c)) {
        on_bluez_object_added(self->priv->object_manager, c->data, self);
    }
    g_list_free_full (connections, (GDestroyNotify) g_object_unref);
}

static void
on_bluez_object_added (GDBusObjectManager *object_manager,
                       GDBusObject        *object,
                       gpointer            user_data)
{
    Bluetooth *self = BLUETOOTH (user_data);
    const char *path = g_dbus_object_get_object_path (object);
    g_autoptr (GDBusProxy) proxy = NULL;
    g_autoptr (GRegex) regex = g_regex_new (
        ".*dev_([0-9A-Fa-f]{2}_){5}([0-9A-Fa-f]{2})$",
        G_REGEX_DEFAULT,
        G_REGEX_MATCH_DEFAULT,
        NULL
    );
    g_autoptr (GVariant) value = NULL;
    g_autoptr (GError) error = NULL;
    gboolean connected;
    gboolean paired;

    if (!g_regex_match (regex, path, G_REGEX_MATCH_DEFAULT, NULL))
        return;

    proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SYSTEM,
        0,
        NULL,
        BLUEZ_DBUS_NAME,
        path,
        BLUEZ_DBUS_DEVICE_INTERFACE,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't get Bluez object: %s", error->message);
        return;
    }

    value = g_dbus_proxy_get_cached_property (proxy, "Paired");
    g_variant_get (value, "b", &paired);

    if (!paired)
        return;

    g_variant_unref (value);
    value = g_dbus_proxy_get_cached_property (proxy, "Connected");
    g_variant_get (value, "b", &connected);

    if (connected)
        self->priv->connected = TRUE;

    g_signal_connect (
        proxy,
        "g-properties-changed",
        G_CALLBACK (on_bluez_proxy_properties),
        self
    );

    self->priv->connections = g_list_append (
        self->priv->connections, g_steal_pointer (&proxy)
    );
}

static void
on_bluez_object_removed (GDBusObjectManager *object_manager,
                         GDBusObject        *object,
                         gpointer            user_data)
{
    Bluetooth *self = BLUETOOTH (user_data);
    const char *object_path = g_dbus_object_get_object_path (object);
    GDBusProxy *proxy;

    GFOREACH (self->priv->connections, proxy) {
        const char *proxy_path = g_dbus_proxy_get_object_path (proxy);
        if (g_strcmp0 (proxy_path, object_path) == 0) {
            self->priv->connections = g_list_remove (
                self->priv->connections, proxy
            );
            g_clear_object (&proxy);
            break;
        }
    }
}

static void
on_bluez_proxy_properties (GDBusProxy  *proxy,
                           GVariant    *changed_properties,
                           char       **invalidated_properties,
                           gpointer     user_data)
{
    Bluetooth *self = BLUETOOTH (user_data);
    GVariant *value;
    char *property;
    GVariantIter i;

    g_variant_iter_init (&i, changed_properties);
    while (g_variant_iter_next (&i, "{&sv}", &property, &value)) {
        if (g_strcmp0 (property, "Powered") == 0) {
            if (!self->priv->powersaving)
                g_variant_get (value, "b", &self->priv->powered);
        } else if (g_strcmp0 (property, "Connected") == 0) {
            g_variant_get (value, "b", &self->priv->connected);
        }
        g_variant_unref (value);
    }
}

static void
bluetooth_dispose (GObject *bluetooth)
{
    Bluetooth *self = BLUETOOTH (bluetooth);

    g_clear_object (&self->priv->object_manager);
    g_clear_object (&self->priv->bluez_proxy);

    G_OBJECT_CLASS (bluetooth_parent_class)->dispose (bluetooth);
}

static void
bluetooth_finalize (GObject *bluetooth)
{
    G_OBJECT_CLASS (bluetooth_parent_class)->finalize (bluetooth);
}

static void
bluetooth_class_init (BluetoothClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = bluetooth_dispose;
    object_class->finalize = bluetooth_finalize;
}

static void
bluetooth_init (Bluetooth *self)
{
    g_autoptr (GVariant) value = NULL;
    g_autoptr (GError) error = NULL;

    self->priv = bluetooth_get_instance_private (self);

    self->priv->connected = FALSE;
    self->priv->powered = FALSE;
    self->priv->powersaving = FALSE;
    self->priv->connections = NULL;

    self->priv->bluez_proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SYSTEM,
        0,
        NULL,
        BLUEZ_DBUS_NAME,
        BLUEZ_DBUS_PATH,
        BLUEZ_DBUS_ADAPTER_INTERFACE,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't contact Bluez: %s", error->message);
        return;
    }

    g_signal_connect (
        self->priv->bluez_proxy,
        "g-properties-changed",
        G_CALLBACK (on_bluez_proxy_properties),
        self
    );

    value = g_dbus_proxy_get_cached_property (
        self->priv->bluez_proxy, "Powered"
    );
    g_variant_get (value, "b", &self->priv->powered);

    self->priv->object_manager = g_dbus_object_manager_client_new_sync (
        g_dbus_proxy_get_connection (self->priv->bluez_proxy),
        G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
        BLUEZ_DBUS_NAME,
        "/",
        NULL,
        NULL,
        NULL,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't get object manager: %s", error->message);
        return;
    }

    g_signal_connect(
        self->priv->object_manager,
        "object-added",
        G_CALLBACK(on_bluez_object_added),
        self
    );
    g_signal_connect(
        self->priv->object_manager,
        "object-removed",
        G_CALLBACK(on_bluez_object_removed),
        self
    );
    check_existing_connections (self);
}

/**
 * bluetooth_new:
 *
 * Creates a new #Bluetooth
 *
 * Returns: (transfer full): a new #Bluetooth
 *
 **/
GObject *
bluetooth_new (void)
{
    GObject *bluetooth;

    bluetooth = g_object_new (TYPE_BLUETOOTH, NULL);

    return bluetooth;
}

/**
 * bluetooth_set_powersave:
 *
 * Set bluetooth devices to powersave
 *
 * @param #Bluetooth
 * @param powersave: True to enable powersave
 */
void
bluetooth_set_powersave (Bluetooth *self,
                         gboolean   powersave)
{
    g_autoptr (GDBusProxy) proxy = NULL;
    g_autoptr (GError) error = NULL;

    if (!self->priv->powered || self->priv->connected)
        return;

    if (!can_powersave (self))
        return;

    g_debug ("Set Bluetooth powersave: %b", powersave);

    proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SYSTEM,
        0,
        NULL,
        BLUEZ_DBUS_NAME,
        BLUEZ_DBUS_PATH,
        DBUS_PROPERTIES_INTERFACE,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't contact Bluez: %s", error->message);
        return;
    }

    self->priv->powersaving = powersave;
    g_dbus_proxy_call_sync (
        proxy,
        "Set",
        g_variant_new (
            "(ssv)",
            BLUEZ_DBUS_ADAPTER_INTERFACE,
            "Powered",
            g_variant_new ("b", !powersave)
        ),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't set device powered state: %s", error->message);
        return;
    }
}