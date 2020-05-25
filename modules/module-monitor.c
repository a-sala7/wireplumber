/* WirePlumber
 *
 * Copyright © 2019 Wim Taymans
 * Copyright © 2019 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <wp/wp.h>

#include <pipewire/pipewire.h>
#include <spa/utils/keys.h>
#include <spa/monitor/device.h>
#include <spa/pod/builder.h>

#include "module-monitor/reserve-device.h"
#include "module-monitor/reserve-node.h"

G_DEFINE_QUARK (wp-module-monitor-id, id);
G_DEFINE_QUARK (wp-module-monitor-children, children);
G_DEFINE_QUARK (wp-module-monitor-data, data);

typedef enum {
  FLAG_LOCAL_NODES = (1 << 0),
  FLAG_USE_ADAPTER = (1 << 1),
  FLAG_ACTIVATE_DEVICES = (1 << 2),
  FLAG_DBUS_RESERVATION = (1 << 3),
} MonitorFlags;

static const struct {
  MonitorFlags flag;
  const gchar *name;
} flag_names[] = {
  { FLAG_LOCAL_NODES, "local-nodes" },
  { FLAG_USE_ADAPTER, "use-adapter" },
  { FLAG_ACTIVATE_DEVICES, "activate-devices" },
  { FLAG_DBUS_RESERVATION, "dbus-reservation" }
};

enum {
  PROP_0,
  PROP_FACTORY,
  PROP_FLAGS,
};

struct _WpMonitor
{
  WpPlugin parent;

  /* Props */
  gchar *factory;
  MonitorFlags flags;

  WpSpaDevice *monitor;
};

G_DECLARE_FINAL_TYPE (WpMonitor, wp_monitor, WP, MONITOR, WpPlugin)
G_DEFINE_TYPE (WpMonitor, wp_monitor, WP_TYPE_PLUGIN)

static void on_object_info (WpSpaDevice * device,
    guint id, GType type, const gchar * spa_factory,
    WpProperties * props, WpProperties * parent_props,
    WpMonitor * self);

static void
setup_device_props (WpProperties *p)
{
  const gchar *s, *d, *api;

  api = wp_properties_get (p, SPA_KEY_DEVICE_API);

  /* set the device name if it's not already set */
  if (!wp_properties_get (p, SPA_KEY_DEVICE_NAME)) {
    if ((s = wp_properties_get (p, SPA_KEY_DEVICE_BUS_ID)) == NULL) {
      if ((s = wp_properties_get (p, SPA_KEY_DEVICE_BUS_PATH)) == NULL) {
        s = "unknown";
      }
    }

    if (!g_strcmp0 (api, "alsa")) {
      /* what we call a "device" in pipewire, in alsa it's a "card",
         so make it clear to avoid confusion */
      wp_properties_setf (p, PW_KEY_DEVICE_NAME, "alsa_card.%s", s);
    } else {
      wp_properties_setf (p, PW_KEY_DEVICE_NAME, "%s_device.%s", api, s);
    }
  }

  /* set the device description if it's not already set */
  if (!wp_properties_get (p, PW_KEY_DEVICE_DESCRIPTION)) {
    d = NULL;

    if (!g_strcmp0 (api, "alsa")) {
      if ((s = wp_properties_get (p, PW_KEY_DEVICE_FORM_FACTOR))
            && !g_strcmp0 (s, "internal"))
        d = "Built-in Audio";
      else if ((s = wp_properties_get (p, PW_KEY_DEVICE_CLASS))
            && !g_strcmp0 (s, "modem"))
        d = "Modem";
    }

    if (!d)
      d = wp_properties_get (p, PW_KEY_DEVICE_PRODUCT_NAME);
    if (!d)
      d = "Unknown device";

    wp_properties_set (p, PW_KEY_DEVICE_DESCRIPTION, d);
  }

  /* set the icon name for ALSA - TODO for other APIs */
  if (!wp_properties_get (p, PW_KEY_DEVICE_ICON_NAME)
        && !g_strcmp0 (api, "alsa"))
  {
    d = NULL;

    if ((s = wp_properties_get (p, PW_KEY_DEVICE_FORM_FACTOR))) {
      if (!g_strcmp0 (s, "microphone"))
        d = "audio-input-microphone";
      else if (!g_strcmp0 (s, "webcam"))
        d = "camera-web";
      else if (!g_strcmp0 (s, "computer"))
        d = "computer";
      else if (!g_strcmp0 (s, "handset"))
        d = "phone";
      else if (!g_strcmp0 (s, "portable"))
        d = "multimedia-player";
      else if (!g_strcmp0 (s, "tv"))
        d = "video-display";
      else if (!g_strcmp0 (s, "headset"))
        d = "audio-headset";
      else if (!g_strcmp0 (s, "headphone"))
        d = "audio-headphones";
      else if (!g_strcmp0 (s, "speaker"))
        d = "audio-speakers";
      else if (!g_strcmp0 (s, "hands-free"))
        d = "audio-handsfree";
    }
    if (!d)
      if ((s = wp_properties_get (p, PW_KEY_DEVICE_CLASS))
            && !g_strcmp0 (s, "modem"))
          d = "modem";

    if (!d)
      d = "audio-card";

    s = wp_properties_get (p, PW_KEY_DEVICE_BUS);

    wp_properties_setf (p, PW_KEY_DEVICE_ICON_NAME,
        "%s-analog%s%s", d, s ? "-" : "", s);
  }
}

static void
setup_node_props (WpProperties *dev_props, WpProperties *node_props)
{
  const gchar *api, *name, *description, *factory;

  /* Make the device properties directly available on the node */
  wp_properties_update_keys (node_props, dev_props,
      SPA_KEY_DEVICE_API,
      SPA_KEY_DEVICE_NAME,
      SPA_KEY_DEVICE_ALIAS,
      SPA_KEY_DEVICE_NICK,
      SPA_KEY_DEVICE_DESCRIPTION,
      SPA_KEY_DEVICE_ICON,
      SPA_KEY_DEVICE_ICON_NAME,
      SPA_KEY_DEVICE_PLUGGED_USEC,
      SPA_KEY_DEVICE_BUS_ID,
      SPA_KEY_DEVICE_BUS_PATH,
      SPA_KEY_DEVICE_BUS,
      SPA_KEY_DEVICE_SUBSYSTEM,
      SPA_KEY_DEVICE_SYSFS_PATH,
      SPA_KEY_DEVICE_VENDOR_ID,
      SPA_KEY_DEVICE_VENDOR_NAME,
      SPA_KEY_DEVICE_PRODUCT_ID,
      SPA_KEY_DEVICE_PRODUCT_NAME,
      SPA_KEY_DEVICE_SERIAL,
      SPA_KEY_DEVICE_CLASS,
      SPA_KEY_DEVICE_CAPABILITIES,
      SPA_KEY_DEVICE_FORM_FACTOR,
      PW_KEY_DEVICE_INTENDED_ROLES,
      NULL);

  /* get some strings that we are going to need below */
  api = wp_properties_get (node_props, SPA_KEY_DEVICE_API);
  factory = wp_properties_get (node_props, SPA_KEY_FACTORY_NAME);

  name = wp_properties_get (node_props, SPA_KEY_DEVICE_NAME);
  if (G_UNLIKELY (!name))
    name = wp_properties_get (node_props, SPA_KEY_DEVICE_NICK);
  if (G_UNLIKELY (!name))
    name = wp_properties_get (node_props, SPA_KEY_DEVICE_ALIAS);
  if (G_UNLIKELY (!name))
    name = "unknown-device";

  description = wp_properties_get (node_props, SPA_KEY_DEVICE_DESCRIPTION);
  if (!description)
    description = name;

  /* set ALSA specific properties */
  if (!g_strcmp0 (api, "alsa:pcm")) {
    const gchar *str;

    /* compose the node name */
    str = wp_properties_get (node_props, SPA_KEY_API_ALSA_PCM_ID);
    wp_properties_setf (node_props, PW_KEY_NODE_NAME, "%s/%s/%s",
        factory, name, str);

    /* compose the node description */
    str = wp_properties_get (node_props, SPA_KEY_API_ALSA_PCM_NAME);
    wp_properties_setf (node_props, PW_KEY_NODE_DESCRIPTION, "%s: %s",
        description, str);

    wp_properties_update_keys (node_props, dev_props,
        SPA_KEY_API_ALSA_CARD,
        SPA_KEY_API_ALSA_CARD_ID,
        SPA_KEY_API_ALSA_CARD_COMPONENTS,
        SPA_KEY_API_ALSA_CARD_DRIVER,
        SPA_KEY_API_ALSA_CARD_NAME,
        SPA_KEY_API_ALSA_CARD_LONGNAME,
        SPA_KEY_API_ALSA_CARD_MIXERNAME,
        NULL);

  /* set BLUEZ 5 specific properties */
  } else if (!g_strcmp0 (api, "bluez5")) {
    const gchar *profile =
        wp_properties_get (node_props, SPA_KEY_API_BLUEZ5_PROFILE);

    /* compose the node name */
    wp_properties_setf (node_props, PW_KEY_NODE_NAME, "%s/%s/%s",
        factory, name, profile);

    /* compose the node description */
    wp_properties_setf (node_props, PW_KEY_NODE_DESCRIPTION, "%s (%s)",
        description, profile);

    wp_properties_update_keys (node_props, dev_props,
        SPA_KEY_API_BLUEZ5_PATH,
        SPA_KEY_API_BLUEZ5_ADDRESS,
        NULL);

  /* set node properties for other APIs */
  } else {
    wp_properties_setf (node_props, PW_KEY_NODE_NAME, "%s/%s",
        factory, name);
    wp_properties_set (node_props, PW_KEY_NODE_DESCRIPTION, description);
  }
}

static void
augment_done (GObject * proxy, GAsyncResult * res, gpointer user_data)
{
  WpMonitor *self = user_data;

  g_autoptr (GError) error = NULL;
  if (!wp_proxy_augment_finish (WP_PROXY (proxy), res, &error)) {
    wp_warning_object (self, "%s", error->message);
  }
}

static void
free_children (GList * children)
{
  g_list_free_full (children, g_object_unref);
}

static void
find_child (GObject * parent, guint32 id, GList ** children, GList ** link,
    GObject ** child)
{
  *children = g_object_steal_qdata (parent, children_quark ());

  /* Find the child */
  for (*link = *children; *link != NULL; *link = g_list_next (*link)) {
    *child = G_OBJECT ((*link)->data);
    guint32 child_id = GPOINTER_TO_UINT (g_object_get_qdata (*child, id_quark ()));
    if (id == child_id)
      break;
  }
}

static void
on_node_event_info (WpProxy * proxy, GParamSpec *spec, gpointer data)
{
  WpReserveNode *node_data = data;
  const struct pw_node_info *info = wp_proxy_get_info (proxy);

  g_return_if_fail (node_data);

  /* handle the different states */
  switch (info->state) {
  case PW_NODE_STATE_IDLE:
    /* Release reservation after 3 seconds */
    wp_reserve_node_timeout_release (node_data, 3000);
    break;
  case PW_NODE_STATE_RUNNING:
    /* Clear pending timeout if any and acquire reservation */
    wp_reserve_node_acquire (node_data);
    break;
  case PW_NODE_STATE_SUSPENDED:
    break;
  default:
    break;
  }
}

static void
add_reserve_node_data (WpMonitor * self, WpProxy *node, WpProxy *device)
{
  WpReserveDevice *device_data = NULL;
  g_autoptr (WpReserveNode) node_data = NULL;

  /* Only add reservation data on nodes whose device has reservation data */
  device_data = g_object_get_qdata (G_OBJECT (device), data_quark ());
  if (!device_data)
    return;

  /* Create the node reservation data */
  node_data = wp_reserve_node_new (node, device_data);

  /* Handle the info signal */
  g_signal_connect_object (WP_NODE (node), "notify::info",
      (GCallback) on_node_event_info, node_data, 0);

  /* Set the reserve node data on the node */
  g_object_set_qdata_full (G_OBJECT (node), data_quark (),
      g_steal_pointer (&node_data), g_object_unref);
}

static void
create_node (WpMonitor * self, WpProxy * parent, GList ** children,
    guint id, const gchar * spa_factory, WpProperties * props,
    WpProperties * parent_props)
{
  g_autoptr (WpCore) core = wp_proxy_get_core (parent);
  WpProxy *node = NULL;
  const gchar *pw_factory_name;

  wp_debug_object (self, "%s new node %u (%s)", self->factory, id, spa_factory);

  /* use the adapter instead of spa-node-factory if requested */
  pw_factory_name =
      (self->flags & FLAG_USE_ADAPTER) ? "adapter" : "spa-node-factory";

  props = wp_properties_copy (props);
  wp_properties_set (props, SPA_KEY_FACTORY_NAME, spa_factory);

  /* add device id property */
  if (wp_proxy_get_features (parent) & WP_PROXY_FEATURE_BOUND) {
    guint32 device_id = wp_proxy_get_bound_id (parent);
    wp_properties_setf (props, PW_KEY_DEVICE_ID, "%u", device_id);
  }

  setup_node_props (parent_props, props);

  /* create the node */
  node = (self->flags & FLAG_LOCAL_NODES) ?
      (WpProxy *) wp_impl_node_new_from_pw_factory (core, pw_factory_name, props) :
      (WpProxy *) wp_node_new_from_factory (core, pw_factory_name, props);
  if (!node)
    return;

  /* export to pipewire by requesting FEATURE_BOUND */
  wp_proxy_augment (node, WP_PROXY_FEATURE_BOUND, NULL, augment_done, self);

  g_object_set_qdata (G_OBJECT (node), id_quark (), GUINT_TO_POINTER (id));
  *children = g_list_prepend (*children, node);

  add_reserve_node_data (self, node, parent);
}

static void
add_reserve_device_data (WpMonitor * self, WpProxy *device)
{
  g_autoptr (WpCore) core = wp_proxy_get_core (WP_PROXY (device));
  g_autoptr (WpProperties) props = wp_proxy_get_properties (device);
  const char *card_id = NULL;
  const char *app_dev_name = NULL;
  g_autoptr (WpDbusDeviceReservation) reservation = NULL;
  g_autoptr (WpReserveDevice) device_data = NULL;

  if ((self->flags & FLAG_DBUS_RESERVATION) == 0)
    return;

  card_id = wp_properties_get (props, SPA_KEY_API_ALSA_CARD);
  if (!card_id)
    return;

  app_dev_name = wp_properties_get (props, SPA_KEY_API_ALSA_PATH);

  /* Create the dbus device reservation */
  reservation = wp_dbus_device_reservation_new (atoi(card_id),
      "PipeWire", 10, app_dev_name);

  /* Create the reserve device data */
  device_data = wp_reserve_device_new (device, reservation);

  /* Set the reserve device data on the device */
  g_object_set_qdata_full (G_OBJECT (device), data_quark (),
      g_steal_pointer (&device_data), g_object_unref);
}

static void
device_created (GObject * proxy, GAsyncResult * res, gpointer user_data)
{
  WpMonitor * self = user_data;
  g_autoptr (GError) error = NULL;

  if (!wp_proxy_augment_finish (WP_PROXY (proxy), res, &error)) {
    wp_warning_object (self, "%s", error->message);
    return;
  }

  if (self->flags & FLAG_DBUS_RESERVATION) {
    add_reserve_device_data (self, WP_PROXY (proxy));
  } else if (self->flags & FLAG_ACTIVATE_DEVICES) {
    g_autoptr (WpSpaPod) profile = wp_spa_pod_new_object (
      "Profile", "Profile",
      "index", "i", 1,
      NULL);
    wp_proxy_set_param (WP_PROXY (proxy), "Profile", profile);
  }
}

static void
create_device (WpMonitor * self, WpProxy * parent, GList ** children,
    guint id, const gchar * spa_factory, WpProperties * props)
{
  g_autoptr (WpCore) core = wp_proxy_get_core (parent);
  WpSpaDevice *device;

  wp_debug_object (self, "%s new device %u", self->factory, id);

  props = wp_properties_copy (props);
  setup_device_props (props);

  if (!(device = wp_spa_device_new_from_spa_factory (core, spa_factory, props)))
    return;

  g_signal_connect (device, "object-info", (GCallback) on_object_info, self);
  wp_proxy_augment (WP_PROXY (device),
      WP_PROXY_FEATURE_BOUND | WP_SPA_DEVICE_FEATURE_ACTIVE,
      NULL, device_created, self);

  g_object_set_qdata (G_OBJECT (device), id_quark (), GUINT_TO_POINTER (id));
  *children = g_list_prepend (*children, device);
}

static void
on_object_info (WpSpaDevice * device,
    guint id, GType type, const gchar * spa_factory,
    WpProperties * props, WpProperties * parent_props,
    WpMonitor * self)
{
  GList *children = NULL;
  GList *link = NULL;
  GObject *child = NULL;

  /* Find the child */
  find_child (G_OBJECT (device), id, &children, &link, &child);

  /* new object, construct... */
  if (type != G_TYPE_NONE && !link) {
    if (type == WP_TYPE_DEVICE) {
      create_device (self, WP_PROXY (device), &children, id, spa_factory, props);
    } else if (type == WP_TYPE_NODE) {
      create_node (self, WP_PROXY (device), &children, id, spa_factory, props,
          parent_props);
    } else {
      wp_debug_object (self, "%s got device object-info for unknown object "
          "type %s", self->factory, g_type_name (type));
    }
  }
  /* object removed, delete... */
  else if (type == G_TYPE_NONE && link) {
    g_object_unref (child);
    children = g_list_delete_link (children, link);
  }

  /* put back the children */
  g_object_set_qdata_full (G_OBJECT (device), children_quark (), children,
      (GDestroyNotify) free_children);
}

static void
wp_monitor_activate (WpPlugin * plugin)
{
  WpMonitor *self = WP_MONITOR (plugin);
  g_autoptr (WpCore) core = wp_plugin_get_core (plugin);

  /* create the monitor and handle onject-info callback */
  self->monitor = wp_spa_device_new_from_spa_factory (core, self->factory,
      NULL);
  g_signal_connect (self->monitor, "object-info", (GCallback) on_object_info,
      self);

  /* no FEATURE_BOUND here; exporting the monitor device is buggy */
  wp_proxy_augment (WP_PROXY (self->monitor), WP_SPA_DEVICE_FEATURE_ACTIVE,
      NULL, augment_done, self);
}

static void
wp_monitor_deactivate (WpPlugin * plugin)
{
  WpMonitor *self = WP_MONITOR (plugin);

  g_clear_object (&self->monitor);
}

static void
wp_monitor_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  WpMonitor *self = WP_MONITOR (object);

  switch (property_id) {
  case PROP_FACTORY:
    g_clear_pointer (&self->factory, g_free);
    self->factory = g_value_dup_string (value);
    break;
  case PROP_FLAGS:
    self->flags = g_value_get_uint (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
wp_monitor_get_property (GObject * object, guint property_id, GValue * value,
    GParamSpec * pspec)
{
  WpMonitor *self = WP_MONITOR (object);

  switch (property_id) {
  case PROP_FACTORY:
    g_value_set_string (value, self->factory);
    break;
  case PROP_FLAGS:
    g_value_set_uint (value, self->flags);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
wp_monitor_finalize (GObject * object)
{
  WpMonitor *self = WP_MONITOR (object);

  g_clear_pointer (&self->factory, g_free);

  G_OBJECT_CLASS (wp_monitor_parent_class)->finalize (object);
}

static void
wp_monitor_init (WpMonitor * self)
{
}

static void
wp_monitor_class_init (WpMonitorClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  WpPluginClass *plugin_class = (WpPluginClass *) klass;

  object_class->finalize = wp_monitor_finalize;
  object_class->set_property = wp_monitor_set_property;
  object_class->get_property = wp_monitor_get_property;

  plugin_class->activate = wp_monitor_activate;
  plugin_class->deactivate = wp_monitor_deactivate;

  /* Properties */
  g_object_class_install_property (object_class, PROP_FACTORY,
      g_param_spec_string ("factory", "factory",
          "The monitor factory name", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FLAGS,
      g_param_spec_uint ("flags", "flags",
          "The monitor flags", 0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

WP_PLUGIN_EXPORT void
wireplumber__module_init (WpModule * module, WpCore * core, GVariant * args)
{
  const gchar *factory = NULL;
  MonitorFlags flags = 0;

  /* Get the factory */
  if (!g_variant_lookup (args, "factory", "s", &factory)) {
    wp_warning ("Failed to load monitor: no 'factory' key specified");
    return;
  }

  /* Get the flags */
  GVariantIter *iter;
  if (g_variant_lookup (args, "flags", "as", &iter)) {
    gchar *flag_str = NULL;
    while (g_variant_iter_loop (iter, "s", &flag_str)) {
      for (gint i = 0; i < SPA_N_ELEMENTS (flag_names); i++) {
        if (!g_strcmp0 (flag_str, flag_names[i].name))
          flags |= flag_names[i].flag;
      }
    }
    g_variant_iter_free (iter);
  }

  wp_plugin_register (g_object_new (wp_monitor_get_type (),
          "module", module,
          "factory", factory,
          "flags", flags,
          NULL));
}
