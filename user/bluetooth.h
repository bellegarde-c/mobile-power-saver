/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <glib.h>
#include <glib-object.h>

#define TYPE_BLUETOOTH \
    (bluetooth_get_type ())
#define BLUETOOTH(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST \
    ((obj), TYPE_BLUETOOTH, Bluetooth))
#define BLUETOOTH_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST \
    ((cls), TYPE_BLUETOOTH, BluetoothClass))
#define IS_BLUETOOTH(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE \
    ((obj), TYPE_BLUETOOTH))
#define IS_BLUETOOTH_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE \
    ((cls), TYPE_BLUETOOTH))
#define BLUETOOTH_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS \
    ((obj), TYPE_BLUETOOTH, BluetoothClass))

G_BEGIN_DECLS

typedef struct _Bluetooth Bluetooth;
typedef struct _BluetoothClass BluetoothClass;
typedef struct _BluetoothPrivate BluetoothPrivate;

struct _Bluetooth {
    GObject parent;
    BluetoothPrivate *priv;
};

struct _BluetoothClass {
    GObjectClass parent_class;
};

GType           bluetooth_get_type      (void) G_GNUC_CONST;

GObject*        bluetooth_new           (void);
void            bluetooth_set_powersave (Bluetooth     *bluetooth,
                                         gboolean  powersave);

G_END_DECLS

#endif

