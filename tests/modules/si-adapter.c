/* WirePlumber
 *
 * Copyright © 2020 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "../common/base-test-fixture.h"

typedef struct {
  WpBaseTestFixture base;
} TestFixture;

static void
test_si_adapter_setup (TestFixture * f, gconstpointer user_data)
{
  wp_base_test_fixture_setup (&f->base, 0);

  /* load modules */
  {
    g_autoptr (WpTestServerLocker) lock =
        wp_test_server_locker_new (&f->base.server);

    g_assert_cmpint (pw_context_add_spa_lib (f->base.server.context,
            "fake*", "test/libspa-test"), ==, 0);
    g_assert_cmpint (pw_context_add_spa_lib (f->base.server.context,
            "audiotestsrc", "audiotestsrc/libspa-audiotestsrc"), ==, 0);
    g_assert_nonnull (pw_context_load_module (f->base.server.context,
            "libpipewire-module-spa-node-factory", NULL, NULL));
    g_assert_nonnull (pw_context_load_module (f->base.server.context,
            "libpipewire-module-adapter", NULL, NULL));
  }
  {
    g_autoptr (GError) error = NULL;
    wp_core_load_component (f->base.core,
        "libwireplumber-module-si-adapter", "module", NULL, &error);
    g_assert_no_error (error);
  }
}

static void
test_si_adapter_teardown (TestFixture * f, gconstpointer user_data)
{
  wp_base_test_fixture_teardown (&f->base);
}

static void
test_si_adapter_configure_activate (TestFixture * f,
    gconstpointer user_data)
{
  g_autoptr (WpNode) node = NULL;
  g_autoptr (WpSessionItem) adapter = NULL;

  /* create audiotestsrc adapter node */
  node = wp_node_new_from_factory (f->base.core,
      "adapter",
      wp_properties_new (
          "factory.name", "audiotestsrc",
          "node.name", "audiotestsrc.adapter",
          NULL));
  g_assert_nonnull (node);
  wp_object_activate (WP_OBJECT (node), WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL,
      NULL, (GAsyncReadyCallback) test_object_activate_finish_cb, f);
  g_main_loop_run (f->base.loop);

  /* create adapter */
  adapter = wp_session_item_make (f->base.core, "si-adapter");
  g_assert_nonnull (adapter);
  g_assert_true (WP_IS_SI_ENDPOINT (adapter));

  /* configure */
  {
    WpProperties *props = wp_properties_new_empty ();
    wp_properties_setf (props, "node", "%p", node);
    wp_properties_set (props, "role", "Multimedia");
    g_assert_true (wp_session_item_configure (adapter, props));
    g_assert_true (wp_session_item_is_configured (adapter));
  }

  /* validate configuration */
  {
    const gchar *str = NULL;
    g_autoptr (WpProperties) props = wp_session_item_get_properties (adapter);
    g_assert_nonnull (props);
    str = wp_properties_get (props, "name");
    g_assert_nonnull (str);
    g_assert_cmpstr ("audiotestsrc.adapter", ==, str);
    str = wp_properties_get (props, "media-class");
    g_assert_nonnull (str);
    g_assert_cmpstr ("Audio/Source", ==, str);
    str = wp_properties_get (props, "role");
    g_assert_nonnull (str);
    g_assert_cmpstr ("Multimedia", ==, str);
    str = wp_properties_get (props, "direction");
    g_assert_nonnull (str);
    g_assert_cmpstr ("1", ==, str);
    str = wp_properties_get (props, "priority");
    g_assert_nonnull (str);
    g_assert_cmpstr ("0", ==, str);
    str = wp_properties_get (props, "preferred-n-channels");
    g_assert_nonnull (str);
    g_assert_cmpstr ("0", ==, str);
    str = wp_properties_get (props, "enable-control-port");
    g_assert_nonnull (str);
    g_assert_cmpstr ("0", ==, str);
    str = wp_properties_get (props, "enable-monitor");
    g_assert_nonnull (str);
    g_assert_cmpstr ("0", ==, str);
    str = wp_properties_get (props, "si-factory-name");
    g_assert_nonnull (str);
    g_assert_cmpstr ("si-adapter", ==, str);
  }

  /* activate */
  wp_object_activate (WP_OBJECT (adapter), WP_SESSION_ITEM_FEATURE_ACTIVE,
      NULL, (GAsyncReadyCallback) test_object_activate_finish_cb, f);
  g_main_loop_run (f->base.loop);
  g_assert_cmphex (wp_object_get_active_features (WP_OBJECT (adapter)), ==,
      WP_SESSION_ITEM_FEATURE_ACTIVE);
  g_assert_cmphex (wp_object_get_active_features (WP_OBJECT (node)), ==,
      WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL | WP_NODE_FEATURE_PORTS);

  /* deactivate - configuration should not be altered  */
  wp_object_deactivate (WP_OBJECT (adapter), WP_SESSION_ITEM_FEATURE_ACTIVE);
  g_assert_cmphex (wp_object_get_active_features (WP_OBJECT (adapter)), ==, 0);
  g_assert_true (wp_session_item_is_configured (adapter));

  /* reset */
  wp_session_item_reset (adapter);
  g_assert_false (wp_session_item_is_configured (adapter));
}

static void
test_si_adapter_export (TestFixture * f, gconstpointer user_data)
{
  g_autoptr (WpNode) node = NULL;
  g_autoptr (WpSession) session = NULL;
  g_autoptr (WpSessionItem) adapter = NULL;
  g_autoptr (WpObjectManager) clients_om = NULL;
  g_autoptr (WpClient) self_client = NULL;

  /* find self_client, to be used for verifying endpoint.client.id */
  clients_om = wp_object_manager_new ();
  wp_object_manager_add_interest (clients_om, WP_TYPE_CLIENT, NULL);
  wp_object_manager_request_object_features (clients_om,
      WP_TYPE_CLIENT, WP_PROXY_FEATURE_BOUND);
  g_signal_connect_swapped (clients_om, "objects-changed",
      G_CALLBACK (g_main_loop_quit), f->base.loop);
  wp_core_install_object_manager (f->base.core, clients_om);
  g_main_loop_run (f->base.loop);
  g_assert_nonnull (self_client =
      wp_object_manager_lookup (clients_om, WP_TYPE_CLIENT, NULL));

  /* create session */
  session = WP_SESSION (wp_impl_session_new (f->base.core));
  g_assert_nonnull (session);
  wp_object_activate (WP_OBJECT (session), WP_OBJECT_FEATURES_ALL, NULL,
      (GAsyncReadyCallback) test_object_activate_finish_cb, f);
  g_main_loop_run (f->base.loop);

  /* create adapter */
  node = wp_node_new_from_factory (f->base.core,
      "adapter",
      wp_properties_new (
          "factory.name", "audiotestsrc",
          "node.name", "audiotestsrc.adapter",
          NULL));
  g_assert_nonnull (node);
  wp_object_activate (WP_OBJECT (node), WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL,
      NULL, (GAsyncReadyCallback) test_object_activate_finish_cb, f);
  g_main_loop_run (f->base.loop);
  adapter = wp_session_item_make (f->base.core, "si-adapter");
  g_assert_nonnull (adapter);
  g_assert_true (WP_IS_SI_ENDPOINT (adapter));

  /* configure */
  {
    WpProperties *props = wp_properties_new_empty ();
    wp_properties_setf (props, "node", "%p", node);
    wp_properties_setf (props, "session", "%p", session);
    g_assert_true (wp_session_item_configure (adapter, props));
    g_assert_true (wp_session_item_is_configured (adapter));
  }

  /* activate and export */
  wp_object_activate (WP_OBJECT (adapter),
      WP_SESSION_ITEM_FEATURE_ACTIVE | WP_SESSION_ITEM_FEATURE_EXPORTED,
      NULL, (GAsyncReadyCallback) test_object_activate_finish_cb, f);
  g_main_loop_run (f->base.loop);
  g_assert_cmphex (wp_object_get_active_features (WP_OBJECT (adapter)), ==,
      WP_SESSION_ITEM_FEATURE_ACTIVE | WP_SESSION_ITEM_FEATURE_EXPORTED);

  /* validate */
  {
    g_autoptr (WpEndpoint) ep = NULL;
    g_autoptr (WpProperties) props = NULL;
    gchar *tmp;

    g_assert_nonnull (
        ep = wp_session_item_get_associated_proxy (adapter, WP_TYPE_ENDPOINT));
    g_assert_nonnull (
        props = wp_pipewire_object_get_properties (WP_PIPEWIRE_OBJECT (ep)));

    g_assert_cmpstr (wp_endpoint_get_name (ep), ==, "audiotestsrc.adapter");
    g_assert_cmpstr (wp_endpoint_get_media_class (ep), ==,
        "Audio/Source");
    g_assert_cmpint (wp_endpoint_get_direction (ep), ==, WP_DIRECTION_OUTPUT);
    g_assert_cmpstr (wp_properties_get (props, "endpoint.name"), ==,
        "audiotestsrc.adapter");
    g_assert_cmpstr (wp_properties_get (props, "media.class"), ==,
        "Audio/Source");

    tmp = g_strdup_printf ("%d", wp_proxy_get_bound_id (WP_PROXY (session)));
    g_assert_cmpstr (wp_properties_get (props, "session.id"), ==, tmp);
    g_free (tmp);

    tmp = g_strdup_printf ("%d", wp_proxy_get_bound_id (WP_PROXY (node)));
    g_assert_cmpstr (wp_properties_get (props, "node.id"), ==, tmp);
    g_free (tmp);

    tmp = g_strdup_printf ("%d", wp_proxy_get_bound_id (WP_PROXY (self_client)));
    g_assert_cmpstr (wp_properties_get (props, "endpoint.client.id"), ==, tmp);
    g_free (tmp);
  }

  /* reset */
  wp_session_item_reset (adapter);
  g_assert_false (wp_session_item_is_configured (adapter));
}

gint
main (gint argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  wp_init (WP_INIT_ALL);

  /* configure-activate */

  g_test_add ("/modules/si-adapter/configure-activate",
      TestFixture, NULL,
      test_si_adapter_setup,
      test_si_adapter_configure_activate,
      test_si_adapter_teardown);

 /* export */

 g_test_add ("/modules/si-adapter/export",
      TestFixture, NULL,
      test_si_adapter_setup,
      test_si_adapter_export,
      test_si_adapter_teardown);

  return g_test_run ();
}