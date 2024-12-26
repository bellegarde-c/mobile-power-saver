/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>
#include <signal.h>

#include <gio/gio.h>

#include "bus.h"
#include "dozing.h"
#include "settings.h"
#include "../common/services.h"
#include "../common/utils.h"

#define DOZING_PRE_SLEEP          60
#define DOZING_LIGHT_SLEEP        300
#define DOZING_LIGHT_MAINTENANCE  30
#define DOZING_MEDIUM_SLEEP       600
#define DOZING_MEDIUM_MAINTENANCE 50
#define DOZING_FULL_SLEEP         1200
#define DOZING_FULL_MAINTENANCE   80

enum DozingType {
    DOZING_LIGHT,
    DOZING_LIGHT_1,
    DOZING_LIGHT_2,
    DOZING_LIGHT_3,
    DOZING_MEDIUM,
    DOZING_MEDIUM_1,
    DOZING_MEDIUM_2,
    DOZING_FULL
};

struct _DozingPrivate {
    GList *apps;
    Mpris *mpris;
    Services *services;

    guint type;
    guint timeout_id;
};

G_DEFINE_TYPE_WITH_CODE (
    Dozing,
    dozing,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Dozing)
)

static gboolean freeze_apps (Dozing *self);
static gboolean unfreeze_apps (Dozing *self);

static guint
get_maintenance (Dozing *self)
{
    if (self->priv->type < DOZING_MEDIUM)
        return DOZING_LIGHT_MAINTENANCE;
    else if (self->priv->type < DOZING_FULL)
        return DOZING_MEDIUM_MAINTENANCE;
    else
        return DOZING_FULL_MAINTENANCE;
}

static guint
get_sleep (Dozing *self)
{
    if (self->priv->type < DOZING_MEDIUM)
        return DOZING_LIGHT_SLEEP;
    else if (self->priv->type < DOZING_FULL)
        return DOZING_MEDIUM_SLEEP;
    else
        return DOZING_FULL_SLEEP;
}

static void
queue_next_freeze (Dozing *self)
{
    self->priv->timeout_id = g_timeout_add_seconds (
        get_maintenance (self),
        (GSourceFunc) freeze_apps,
        self
    );

    if (self->priv->type < DOZING_FULL)
        self->priv->type += 1;
}

static void
freeze_services (Dozing *self)
{
    Bus *bus = bus_get_default ();

    g_message("Freezing services");

    bus_set_value (bus,
                   "dozing",
                   g_variant_new ("b", TRUE));

    if (settings_suspend_services (settings_get_default ())) {
        GList *blacklist = settings_get_suspend_services_blacklist (
            settings_get_default ()
        );

        services_freeze_all (self->priv->services, blacklist);

        g_list_free_full (blacklist, g_free);
    }
}

static void
unfreeze_services (Dozing *self)
{
    Bus *bus = bus_get_default ();

    g_message("Unfreezing services");

    bus_set_value (bus,
                   "dozing",
                   g_variant_new ("b", FALSE));

    if (settings_suspend_services (settings_get_default ())) {
        GList *blacklist = settings_get_suspend_services_blacklist (
            settings_get_default ()
        );

        services_unfreeze_all (self->priv->services, blacklist);

        g_list_free_full (blacklist, g_free);
    }
}

static gboolean
freeze_apps (Dozing *self)
{
    Bus *bus = bus_get_default ();
    const char *app;
    gboolean apps_active = FALSE;

    if (self->priv->apps != NULL) {
        g_message("Freezing apps");
        GFOREACH (self->priv->apps, app) {
            if (!mpris_can_freeze (self->priv->mpris, app)) {
                apps_active = TRUE;
                continue;
            }
            if (settings_can_freeze_app (settings_get_default (), app))
                write_to_file (app, "1");
        }
    }

    if (apps_active) {
        g_message ("Phone active: Keep little cluster active");
    } else {
        bus_set_value (bus,
                       "little-cluster-powersave",
                       g_variant_new ("b", TRUE));
    }

    bus_set_value (bus, "suspend-modem", g_variant_new ("b", TRUE));
    freeze_services (self);

    g_clear_handle_id (&self->priv->timeout_id, g_source_remove);
    self->priv->timeout_id = g_timeout_add_seconds (
        get_sleep (self),
        (GSourceFunc) unfreeze_apps,
        self
    );

    return FALSE;
}

static gboolean
unfreeze_apps (Dozing *self)
{
    Bus *bus = bus_get_default ();
    const char *app;

    if (self->priv->apps == NULL)
        return FALSE;

    g_message("Unfreezing apps");
    GFOREACH (self->priv->apps, app)
        write_to_file (app, "0");

    bus_set_value (bus, "suspend-modem", g_variant_new ("b", FALSE));
    unfreeze_services (self);

    queue_next_freeze (self);

    return FALSE;
}

static void
dozing_dispose (GObject *dozing)
{
    Dozing *self = DOZING (dozing);

    g_clear_object (&self->priv->services);

    G_OBJECT_CLASS (dozing_parent_class)->dispose (dozing);
}

static void
dozing_finalize (GObject *dozing)
{
    Dozing *self = DOZING (dozing);

    g_list_free_full (self->priv->apps, g_free);
    g_clear_handle_id (&self->priv->timeout_id, g_source_remove);

    G_OBJECT_CLASS (dozing_parent_class)->finalize (dozing);
}

static void
dozing_class_init (DozingClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = dozing_dispose;
    object_class->finalize = dozing_finalize;
}

static void
dozing_init (Dozing *self)
{
    self->priv = dozing_get_instance_private (self);

    self->priv->mpris = MPRIS (mpris_new ());
    self->priv->services = SERVICES (services_new (G_BUS_TYPE_SESSION));

    self->priv->apps = NULL;
    self->priv->type = DOZING_LIGHT;
}

/**
 * dozing_new:
 *
 * Creates a new #Dozing
 *
 * Returns: (transfer full): a new #Dozing
 *
 **/
GObject *
dozing_new (Mpris *mpris)
{
    GObject *dozing;

    dozing = g_object_new (TYPE_DOZING, NULL);

    DOZING (dozing)->priv->mpris = mpris;

    return dozing;
}

/**
 * dozing_start:
 *
 * Start dozing (freezing/unfreezing apps)
 *
 * @param #Dozing
 */
void
dozing_start (Dozing  *self) {
    g_clear_handle_id (&self->priv->timeout_id, g_source_remove);

    self->priv->apps = get_applications();

    self->priv->type = DOZING_LIGHT;
    self->priv->timeout_id = g_timeout_add_seconds (
        DOZING_PRE_SLEEP,
        (GSourceFunc) freeze_apps,
        self
    );
}

/**
 * dozing_stop:
 *
 * Stop dozing
 *
 * @param #Dozing
 */
void
dozing_stop (Dozing  *self) {
    Bus *bus = bus_get_default ();
    const char *app;

    g_clear_handle_id (&self->priv->timeout_id, g_source_remove);

    bus_set_value (bus, "suspend-modem", g_variant_new ("b", FALSE));
    unfreeze_services (self);

    g_message("Unfreezing apps");
    GFOREACH (self->priv->apps, app)
        write_to_file (app, "0");

    g_list_free_full (self->priv->apps, g_free);
    self->priv->apps = NULL;
}
