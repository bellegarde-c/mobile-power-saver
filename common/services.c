/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>

#include <gio/gio.h>

#include "bus.h"
#include "services.h"
#include "../common/define.h"
#include "../common/utils.h"


struct _ServicesPrivate {
    GBusType service_type;
};


G_DEFINE_TYPE_WITH_CODE (
    Services,
    services,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Services)
)


static gchar *
services_get_cgroups_path (Services *self)
{
    gchar *path;

    if (self->priv->service_type == G_BUS_TYPE_SESSION)
        path = g_strdup_printf(
            CGROUPS_USER_SERVICES_FREEZE_DIR, getuid(), getuid()
        );
    else
        path = g_strdup (CGROUPS_SYSTEM_SERVICES_FREEZE_DIR);

    return path;
}


static void
services_dispose (GObject *services)
{
    G_OBJECT_CLASS (services_parent_class)->dispose (services);
}


static void
services_finalize (GObject *services)
{
    G_OBJECT_CLASS (services_parent_class)->finalize (services);
}


static void
services_class_init (ServicesClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = services_dispose;
    object_class->finalize = services_finalize;
}


static void
services_init (Services *self)
{
    self->priv = services_get_instance_private (self);
}


/**
 * services_new:
 *
 * Creates a new #Services
 *
 * @param #GBusType: target service type
 *
 * Returns: (transfer full): a new #Services
 *
 **/
GObject *
services_new (GBusType service_type)
{
    GObject *services;

    services = g_object_new (TYPE_SERVICES, NULL);

    SERVICES (services)->priv->service_type = service_type;

    return services;
}


/**
 * services_freeze:
 *
 * Freeze services
 *
 * @param #Services
 * @param services: services to start
 *
 **/
void
services_freeze (Services *self,
                 GList   *services)
{
    g_autofree gchar *dirname = services_get_cgroups_path (self);
    const gchar *service;

    GFOREACH (services, service) {
        g_autofree gchar *path = g_build_filename (
            dirname, service, "cgroup.freeze", NULL
        );
        write_to_file (path, "1");
    }
}


/**
 * services_unfreeze:
 *
 * Unfreeze services
 *
 * @param #Services
 * @param services: services to stop
 *
 **/
void
services_unfreeze (Services *self,
                   GList   *services)
{
    g_autofree gchar *dirname = services_get_cgroups_path (self);
    const gchar *service;

    GFOREACH (services, service) {
        g_autofree gchar *path = g_build_filename (
            dirname, service, "cgroup.freeze", NULL
        );
        write_to_file (path, "0");
    }
}