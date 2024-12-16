/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 * Copyright (C) 2023-2024 Bardia Moshiri <fakeshell@bardia.tech>
 */

#include <stdio.h>
#include <gbinder.h>
#include <stdlib.h>
#include <stdint.h>

#include "binder_radio.h"
#include "../common/define.h"

#define HIDL_SET_FEATURE_CODE 128
#define AIDL_SET_FEATURE_CODE 14
#define SERIAL_NUMBER 1

enum IDLFeature {
    FEATURE_POWERSAVE    = 1,
    FEATURE_LOW_DATA     = 2
};

G_DEFINE_TYPE_WITH_CODE (
    BinderRadio,
    binder_radio,
    TYPE_BINDER_CLIENT,
    NULL
)

static void
hidl_set_feature (BinderClient *self,
                  enum IDLFeature feature,
                  gboolean enabled) {
    int status;
    GBinderLocalRequest* req = gbinder_client_new_request(self->client);
    GBinderWriter writer;

    gbinder_local_request_init_writer(req, &writer);
    /* serial */
    gbinder_writer_append_int32(&writer, SERIAL_NUMBER);
    gbinder_writer_append_int32(&writer, feature);
    gbinder_writer_append_bool(&writer, enabled);
    /* sendDeviceState */
    gbinder_client_transact_sync_reply(
        self->client, HIDL_SET_FEATURE_CODE, req, &status
    );
    gbinder_local_request_unref(req);
}

static void
aidl_set_feature (BinderClient *self,
                  enum IDLFeature feature,
                  gboolean enabled) {
     int status;
    GBinderLocalRequest* req = gbinder_client_new_request(self->client);
    GBinderWriter writer;

    gbinder_local_request_init_writer(req, &writer);
    /* serial */
    gbinder_writer_append_int32(&writer, SERIAL_NUMBER);
    gbinder_writer_append_int32(&writer, feature);
    gbinder_writer_append_bool(&writer, enabled);
    /* sendDeviceState */
    gbinder_client_transact_sync_reply(
        self->client, AIDL_SET_FEATURE_CODE, req, &status
    );
    gbinder_local_request_unref(req);
}

static void
binder_radio_dispose (GObject *binder_radio)
{
    G_OBJECT_CLASS (binder_radio_parent_class)->dispose (binder_radio);
}

static void
binder_radio_finalize (GObject *binder_radio)
{
    G_OBJECT_CLASS (binder_radio_parent_class)->finalize (binder_radio);
}

static void
binder_radio_init_binder (BinderClient *self,
                          const gchar  *hidl_service,
                          const gchar  *hidl_client,
                          const gchar  *aidl_service,
                          const gchar  *aidl_client)
{
    BINDER_CLIENT_CLASS (binder_radio_parent_class)->init_binder (
        self,
        hidl_service,
        hidl_client,
        aidl_service,
        aidl_client
    );
}

static void
binder_radio_set_power_profile (BinderClient *self,
                                PowerProfile  power_profile) {
    gboolean enabled = power_profile == POWER_PROFILE_POWER_SAVER;

    if (self->type == BINDER_SERVICE_MANAGER_TYPE_HIDL) {
        hidl_set_feature (self, FEATURE_POWERSAVE, enabled);
        hidl_set_feature (self, FEATURE_LOW_DATA, enabled);
    } else {
        aidl_set_feature (self, FEATURE_POWERSAVE, enabled);
        aidl_set_feature (self, FEATURE_LOW_DATA, enabled);
    }
}

static void
binder_radio_class_init (BinderRadioClass *klass)
{
    GObjectClass *object_class;
    BinderClientClass *binder_client_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = binder_radio_dispose;
    object_class->finalize = binder_radio_finalize;

    binder_client_class = BINDER_CLIENT_CLASS (klass);
    binder_client_class->init_binder = binder_radio_init_binder;
    binder_client_class->set_power_profile = binder_radio_set_power_profile;
}

static void
binder_radio_init (BinderRadio *self)
{
    BinderClientClass *klass = BINDER_CLIENT_GET_CLASS (self);

    klass->init_binder (
        BINDER_CLIENT (self),
        "android.hardware.radio@1.0::IRadio/slot1",
        "android.hardware.radio@1.0::IRadio",
        "android.hardware.radio.modem.IRadioModem/default",
        "android.hardware.radio.modem.IRadioModem"
    );
}

/**
 * binder_radio_new:
 *
 * Creates a new #BinderRadio

 * Returns: (transfer full): a new #BinderRadio
 *
 **/
GObject *
binder_radio_new (void)
{
    GObject *binder_radio;

    binder_radio = g_object_new (TYPE_BINDER_RADIO, NULL);

    return binder_radio;
}