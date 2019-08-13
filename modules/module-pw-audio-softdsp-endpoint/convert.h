/* WirePlumber
 *
 * Copyright © 2019 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __WIREPLUMBER_AUDIO_CONVERT_H__
#define __WIREPLUMBER_AUDIO_CONVERT_H__

#include <gio/gio.h>
#include <wp/wp.h>

#include "stream.h"

G_BEGIN_DECLS

#define WP_TYPE_AUDIO_CONVERT (wp_audio_convert_get_type ())
G_DECLARE_FINAL_TYPE (WpAudioConvert, wp_audio_convert, WP, AUDIO_CONVERT,
    WpAudioStream)

void wp_audio_convert_new (WpEndpoint *endpoint, guint stream_id,
    const char *stream_name, enum pw_direction direction,
    const struct pw_node_info *target, GAsyncReadyCallback callback,
    gpointer user_data);

const struct pw_node_info *wp_audio_convert_get_target (WpAudioConvert *self);

G_END_DECLS

#endif
