﻿/* WirePlumber
 *
 * Copyright © 2020 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <wp/wp.h>

G_DEFINE_QUARK (wp-module-node-suspension-source, source);

struct _WpNodeSuspension
{
  WpPlugin parent;

  WpObjectManager *nodes_om;
};

G_DECLARE_FINAL_TYPE (WpNodeSuspension, wp_node_suspension, WP, NODE_SUSPENSION,
    WpPlugin)
G_DEFINE_TYPE (WpNodeSuspension, wp_node_suspension, WP_TYPE_PLUGIN)

static void
source_clear (GSource *source)
{
  if (source) {
    g_source_destroy (source);
    g_source_unref (source);
  }
}

static gboolean
timeout_suspend_node_callback (WpNode *node)
{
  GSource *source = NULL;
  g_return_val_if_fail (node, G_SOURCE_REMOVE);

  /* Suspend and unref the source */
  wp_node_send_command (node, WP_NODE_COMMAND_SUSPEND);
  source = g_object_steal_qdata (G_OBJECT (node), source_quark ());
  if (source)
    g_source_unref (source);

  return G_SOURCE_REMOVE;
}

static void
on_node_state_changed (WpNode * node, WpNodeState old, WpNodeState curr,
    WpNodeSuspension * self)
{
  g_autoptr (WpCore) core = wp_plugin_get_core (WP_PLUGIN (self));
  g_return_if_fail (core);

  /* Always clear the current source if any */
  {
    GSource *source = g_object_steal_qdata (G_OBJECT (node), source_quark ());
    source_clear (source);
  }

  /* Add a timeout source if idle for at least 3 seconds */
  switch (curr) {
  case WP_NODE_STATE_IDLE: {
    g_autoptr (GSource) source = NULL;
    wp_core_timeout_add_closure (core, &source, 3000, g_cclosure_new_object (
        G_CALLBACK (timeout_suspend_node_callback), G_OBJECT (node)));
    g_object_set_qdata_full (G_OBJECT (node), source_quark (),
        g_steal_pointer (&source), (GDestroyNotify)source_clear);
    break;
  }
  default:
    break;
  }
}

static void
on_node_added (WpObjectManager *om, WpProxy *proxy, gpointer d)
{
  WpNodeSuspension *self = WP_NODE_SUSPENSION (d);

  /* handle the state-changed callback */
  g_signal_connect_object (WP_NODE (proxy), "state-changed",
      G_CALLBACK (on_node_state_changed), self, 0);
}

static void
wp_node_suspension_activate (WpPlugin * plugin)
{
  WpNodeSuspension *self = WP_NODE_SUSPENSION (plugin);
  g_autoptr (WpCore) core = wp_plugin_get_core (WP_PLUGIN (self));

  /* Create the nodes object manager and handle the node added signal */
  self->nodes_om = wp_object_manager_new ();
  wp_object_manager_add_interest (self->nodes_om, WP_TYPE_NODE, NULL);
  wp_object_manager_request_proxy_features (self->nodes_om, WP_TYPE_NODE,
      WP_PROXY_FEATURES_STANDARD);
  g_signal_connect_object (self->nodes_om, "object-added",
      G_CALLBACK (on_node_added), self, 0);
  wp_core_install_object_manager (core, self->nodes_om);
}

static void
wp_node_suspension_deactivate (WpPlugin * plugin)
{
  WpNodeSuspension *self = WP_NODE_SUSPENSION (plugin);

  g_clear_object (&self->nodes_om);
}

static void
wp_node_suspension_init (WpNodeSuspension * self)
{
}

static void
wp_node_suspension_class_init (WpNodeSuspensionClass * klass)
{
  WpPluginClass *plugin_class = (WpPluginClass *) klass;

  plugin_class->activate = wp_node_suspension_activate;
  plugin_class->deactivate = wp_node_suspension_deactivate;
}

WP_PLUGIN_EXPORT void
wireplumber__module_init (WpModule * module, WpCore * core, GVariant * args)
{
  wp_plugin_register (g_object_new (wp_node_suspension_get_type (),
      "module", module,
      NULL));
}