/* WirePlumber
 *
 * Copyright © 2020 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __WPLUA_H__
#define __WPLUA_H__

#include <glib-object.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

G_BEGIN_DECLS

/**
 * WP_DOMAIN_LUA:
 *
 * A #GError domain for errors that occurred within the context of the
 * WirePlumber lua library.
 */
#define WP_DOMAIN_LUA (wp_domain_lua_quark ())
GQuark wp_domain_lua_quark (void);

/**
 * WpLuaError:
 * @WP_LUA_ERROR_COMPILATION: a compilation error, i.e. invalid Lua code
 * @WP_LUA_ERROR_RUNTIME: a runtime error, i.e. misbehaving Lua code
 *
 * Error codes that can appear in a #GError when the error domain
 * is %WP_DOMAIN_LUA
 */
typedef enum {
  WP_LUA_ERROR_COMPILATION,
  WP_LUA_ERROR_RUNTIME,
} WpLuaError;


lua_State * wplua_new (void);
void wplua_free (lua_State * L);

void wplua_enable_sandbox (lua_State * L);

void wplua_register_type_methods (lua_State * L, GType type,
    lua_CFunction constructor, const luaL_Reg * methods);

/* push -> transfer full; get -> transfer none */
void wplua_pushobject (lua_State * L, gpointer object);
gpointer wplua_toobject (lua_State *L, int idx);
gpointer wplua_checkobject (lua_State *L, int idx, GType type);
gboolean wplua_isobject (lua_State *L, int idx, GType type);

/* push -> transfer full; get -> transfer none */
void wplua_pushboxed (lua_State * L, GType type, gpointer object);
gpointer wplua_toboxed (lua_State *L, int idx);
gpointer wplua_checkboxed (lua_State *L, int idx, GType type);
gboolean wplua_isboxed (lua_State *L, int idx, GType type);

/* transfer floating */
GClosure * wplua_function_to_closure (lua_State *L, int idx);
void wplua_lua_to_gvalue (lua_State *L, int idx, GValue *v);
int wplua_gvalue_to_lua (lua_State *L, const GValue *v);

gboolean wplua_load_buffer (lua_State * L, const gchar *buf, gsize size,
    GError **error);
gboolean wplua_load_uri (lua_State * L, const gchar *uri, GError **error);
gboolean wplua_load_path (lua_State * L, const gchar *path, GError **error);

G_END_DECLS

#endif