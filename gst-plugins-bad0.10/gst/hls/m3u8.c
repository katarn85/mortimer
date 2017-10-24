/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 *
 * m3u8.c:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <glib.h>

#include <gst/glib-compat-private.h>
#include "gstfragmented.h"
#include "m3u8.h"

#define GST_CAT_DEFAULT fragmented_debug

static GstM3U8 *gst_m3u8_new (void);
static void gst_m3u8_free (GstM3U8 * m3u8);
static gboolean gst_m3u8_update (GstM3U8 * m3u8, gchar * data,
    gboolean * updated);
static GstM3U8MediaFile *gst_m3u8_media_file_new (gchar * uri,
    gchar * title, GstClockTime duration, guint sequence);
static void gst_m3u8_media_file_free (GstM3U8MediaFile * self);
static GstM3U8Key *gst_m3u8_key_new (GstM3U8EncryptionMethod method,
    gchar * uri, guint8 *iv, guint sequence);
static void gst_m3u8_key_free (GstM3U8Key * self);
static guint _get_start_sequence (GstM3U8Client * client);

static GstM3U8 *
gst_m3u8_new (void)
{
  GstM3U8 *m3u8;

  m3u8 = g_new0 (GstM3U8, 1);

  return m3u8;
}

static void
gst_m3u8_set_uri (GstM3U8 * self, gchar * uri)
{
  g_return_if_fail (self != NULL);

  if (self->uri)
    g_free (self->uri);
  self->uri = uri;
}

static void
gst_m3u8_free (GstM3U8 * self)
{
  g_return_if_fail (self != NULL);

  g_free (self->uri);
  g_free (self->allowcache);
  g_free (self->codecs);
  g_free (self->cipher_ctx);

  g_list_foreach (self->files, (GFunc) gst_m3u8_media_file_free, NULL);
  g_list_free (self->files);

  g_list_foreach (self->keys, (GFunc) gst_m3u8_key_free, NULL);
  g_list_free (self->keys);

  g_free (self->last_data);
  g_list_foreach (self->lists, (GFunc) gst_m3u8_free, NULL);
  g_list_free (self->lists);

  g_free (self);
}

static GstM3U8MediaFile *
gst_m3u8_media_file_new (gchar * uri, gchar * title, GstClockTime duration,
    guint sequence)
{
  GstM3U8MediaFile *file;

  file = g_new0 (GstM3U8MediaFile, 1);
  file->uri = uri;
  file->title = title;
  file->duration = duration;
  file->sequence = sequence;

  return file;
}

static void
gst_m3u8_media_file_free (GstM3U8MediaFile * self)
{
  g_return_if_fail (self != NULL);

  g_free (self->title);
  g_free (self->uri);
  g_free (self);
}

static GstM3U8Key *
gst_m3u8_key_new (GstM3U8EncryptionMethod method, gchar * uri,
  guint8 *iv, guint sequence)
{
  GstM3U8Key *key;

  key = g_new0 (GstM3U8Key, 1);
  key->method = method;
  key->uri = uri;
  key->iv = iv;
  key->sequence = sequence;
  key->data = NULL;

  return key;
}

static void
gst_m3u8_key_free (GstM3U8Key * self)
{
  g_return_if_fail (self != NULL);

  g_free (self->uri);
  g_free (self->iv);
  g_free (self->data);
  g_free (self);
}

static void
gst_m3u8_key_merge_data (GstM3U8Key * key, GList *old_list)
{
  GList *walk;
  gchar *key_uri = key->uri;
  GstM3U8Key *old_key;

  for (walk = old_list; walk; walk = walk->next) {
    old_key = GST_M3U8_KEY (walk->data);
    if (!g_strcmp0 (old_key->uri, key_uri)) {
      key->data = old_key->data;
      old_key->data = NULL;
      break;
    }
  }
}

static gboolean
int_from_string (gchar * ptr, gchar ** endptr, gint * val)
{
  gchar *end;
  glong ret;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (val != NULL, FALSE);

  errno = 0;
  ret = strtol (ptr, &end, 10);
  if ((errno == ERANGE && (ret == LONG_MAX || ret == LONG_MIN))
      || (errno != 0 && ret == 0)) {
    GST_WARNING ("%s", g_strerror (errno));
    return FALSE;
  }

  if (ret > G_MAXINT) {
    GST_WARNING ("%s", g_strerror (ERANGE));
    return FALSE;
  }

  if (endptr)
    *endptr = end;

  *val = (gint) ret;

  return end != ptr;
}

static gboolean
double_from_string (gchar * ptr, gchar ** endptr, gdouble * val)
{
  gchar *end;
  gdouble ret;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (val != NULL, FALSE);

  errno = 0;
  ret = strtod (ptr, &end);
  if ((errno == ERANGE && (ret == HUGE_VAL || ret == -HUGE_VAL))
      || (errno != 0 && ret == 0)) {
    GST_WARNING ("%s", g_strerror (errno));
    return FALSE;
  }

  if (!isfinite (ret)) {
    GST_WARNING ("%s", g_strerror (ERANGE));
    return FALSE;
  }

  if (endptr)
    *endptr = end;

  *val = (gint) ret;

  return end != ptr;
}

static gboolean
iv_from_string (gchar * ptr, gchar ** endptr, guint8 * val)
{
  guint8 idx;
  guint8 hex;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (val != NULL, FALSE);

  if (*ptr == '0' && (*(ptr + 1) == 'x' || *(ptr + 1) == 'X')) {
    ptr = ptr + 2;  /* skip 0x or 0X */
  }

  for (idx = 0; idx < GST_M3U8_IV_LEN; idx++) {
    hex = *ptr++ - '0';
    if (hex >= 16)
      return FALSE;
    *(val + idx) = hex * 16;
    hex = *ptr++ - '0';
    if (hex >= 16)
      return FALSE;
    *(val + idx) += hex;
  }

  if (endptr)
    *endptr = ptr;

  return TRUE;
}

static gboolean
iv_from_uint (guint uint, guint8 * val)
{
  guint8 idx;

  g_return_val_if_fail (val != NULL, FALSE);

  val = val + GST_M3U8_IV_LEN - 1;
  for (idx = 0; idx < sizeof(guint); idx++) {
    *val-- = (guint8)uint;
    uint = uint >> 8;
  }

  return TRUE;
}

static gboolean
make_valid_uri (gchar *prefix, gchar *suffix, gchar **uri)
{
  gchar *slash;

  if (!prefix) {
    GST_WARNING ("uri prefix not set, can't build a valid uri");
    return FALSE;
  }
  slash = g_utf8_strrchr (prefix, -1, '/');
  if (!slash) {
    GST_WARNING ("Can't build a valid uri");
    return FALSE;
  }

  *slash = '\0';
  *uri = g_strdup_printf ("%s/%s", prefix, suffix);
  *slash = '/';

  return TRUE;
}

static gboolean
parse_attributes (gchar ** ptr, gchar ** a, gchar ** v)
{
  gchar *end, *p;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (*ptr != NULL, FALSE);
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (v != NULL, FALSE);

  /* [attribute=value,]* */

  *a = *ptr;
  end = p = g_utf8_strchr (*ptr, -1, ',');
  if (end) {
    do {
      end = g_utf8_next_char (end);
    } while (end && *end == ' ');
    *p = '\0';
  }

  *v = p = g_utf8_strchr (*ptr, -1, '=');
  if (*v) {
    *v = g_utf8_next_char (*v);
    *p = '\0';
  } else {
    GST_WARNING ("missing = after attribute");
    return FALSE;
  }

  *ptr = end;
  return TRUE;
}

static gint
_m3u8_compare_uri (GstM3U8 * a, gchar * uri)
{
  g_return_val_if_fail (a != NULL, 0);
  g_return_val_if_fail (uri != NULL, 0);

  return g_strcmp0 (a->uri, uri);
}

static gint
gst_m3u8_compare_playlist_by_bitrate (gconstpointer a, gconstpointer b)
{
  return ((GstM3U8 *) (a))->bandwidth - ((GstM3U8 *) (b))->bandwidth;
}

/*
 * @data: a m3u8 playlist text data, taking ownership
 */
static gboolean
gst_m3u8_update (GstM3U8 * self, gchar * data, gboolean * updated)
{
  gint val;
  GstClockTime duration;
  gchar *title, *end;
//  gboolean discontinuity;
  GstM3U8 *list;
  GList *new_keys = NULL;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (updated != NULL, FALSE);

  *updated = TRUE;

  /* check if the data changed since last update */
  if (self->last_data && g_str_equal (self->last_data, data)) {
    GST_DEBUG ("Playlist is the same as previous one");
    *updated = FALSE;
    g_free (data);
    return TRUE;
  }

  if (!g_str_has_prefix (data, "#EXTM3U")) {
    GST_WARNING ("Data doesn't start with #EXTM3U");
    *updated = FALSE;
    g_free (data);
    return FALSE;
  }

  g_free (self->last_data);
  self->last_data = data;

  if (self->files) {
    g_list_foreach (self->files, (GFunc) gst_m3u8_media_file_free, NULL);
    g_list_free (self->files);
    self->files = NULL;
  }

  list = NULL;
  duration = 0;
  title = NULL;
  data += 7;
  while (TRUE) {
    end = g_utf8_strchr (data, -1, '\n');
    if (end)
      *end = '\0';

    if (data[0] != '#') {
      gchar *r;

      if (duration <= 0 && list == NULL) {
        GST_LOG ("%s: got line without EXTINF or EXTSTREAMINF, dropping", data);
        goto next_line;
      }

      if (!gst_uri_is_valid (data)) {
        if (!make_valid_uri(self->uri, data, &data))
          goto next_line;
      } else {
        data = g_strdup (data);
      }

      r = g_utf8_strchr (data, -1, '\r');
      if (r)
        *r = '\0';

      if (list != NULL) {
        if (g_list_find_custom (self->lists, data,
                (GCompareFunc) _m3u8_compare_uri)) {
          GST_DEBUG ("Already have a list with this URI");
          gst_m3u8_free (list);
          g_free (data);
        } else {
          gst_m3u8_set_uri (list, data);
          self->lists = g_list_append (self->lists, list);
        }
        list = NULL;
      } else {
        GstM3U8MediaFile *file;
        file =
            gst_m3u8_media_file_new (data, title, duration,
            self->mediasequence++);
        duration = 0;
        title = NULL;
        self->files = g_list_append (self->files, file);
      }

    } else if (g_str_has_prefix (data, "#EXT-X-ENDLIST")) {
      self->endlist = TRUE;
    } else if (g_str_has_prefix (data, "#EXT-X-VERSION:")) {
      if (int_from_string (data + 15, &data, &val))
        self->version = val;
    } else if (g_str_has_prefix (data, "#EXT-X-STREAM-INF:")) {
      gchar *v, *a;

      if (list != NULL) {
        GST_WARNING ("Found a list without a uri..., dropping");
        gst_m3u8_free (list);
      }

      list = gst_m3u8_new ();
      data = data + 18;
      while (data && parse_attributes (&data, &a, &v)) {
        if (g_str_equal (a, "BANDWIDTH")) {
          if (!int_from_string (v, NULL, &list->bandwidth))
            GST_WARNING ("Error while reading BANDWIDTH");
        } else if (g_str_equal (a, "PROGRAM-ID")) {
          if (!int_from_string (v, NULL, &list->program_id))
            GST_WARNING ("Error while reading PROGRAM-ID");
        } else if (g_str_equal (a, "CODECS")) {
          g_free (list->codecs);
          list->codecs = g_strdup (v);
        } else if (g_str_equal (a, "RESOLUTION")) {
          if (!int_from_string (v, &v, &list->width))
            GST_WARNING ("Error while reading RESOLUTION width");
          if (!v || *v != '=') {
            GST_WARNING ("Missing height");
          } else {
            v = g_utf8_next_char (v);
            if (!int_from_string (v, NULL, &list->height))
              GST_WARNING ("Error while reading RESOLUTION height");
          }
        }
      }
    } else if (g_str_has_prefix (data, "#EXT-X-TARGETDURATION:")) {
      if (int_from_string (data + 22, &data, &val))
        self->targetduration = val * GST_SECOND;
    } else if (g_str_has_prefix (data, "#EXT-X-MEDIA-SEQUENCE:")) {
      if (int_from_string (data + 22, &data, &val))
        self->mediasequence = val;
    } else if (g_str_has_prefix (data, "#EXT-X-DISCONTINUITY")) {
      /* discontinuity = TRUE; */
    } else if (g_str_has_prefix (data, "#EXT-X-PROGRAM-DATE-TIME:")) {
      /* <YYYY-MM-DDThh:mm:ssZ> */
      GST_DEBUG ("FIXME parse date");
    } else if (g_str_has_prefix (data, "#EXT-X-ALLOW-CACHE:")) {
      g_free (self->allowcache);
      self->allowcache = g_strdup (data + 19);
    } else if (g_str_has_prefix (data, "#EXTINF:")) {
      gdouble fval;
      if (!double_from_string (data + 8, &data, &fval)) {
        GST_WARNING ("Can't read EXTINF duration");
        goto next_line;
      }
      duration = fval * (gdouble) GST_SECOND;
      if (duration > self->targetduration)
        GST_WARNING ("EXTINF duration > TARGETDURATION");
      if (!data || *data != ',')
        goto next_line;
      data = g_utf8_next_char (data);
      if (data != end) {
        g_free (title);
        title = g_strdup (data);
      }
    } else if (g_str_has_prefix (data, "#EXT-X-KEY:")) {
      gchar *v, *a;
      GstM3U8Key *key;
      GstM3U8EncryptionMethod encryption;
      gchar *key_uri;
      guint8 *iv;

      data = data + 11;
      encryption = GST_M3U8_ENCRYPTED_NONE;
      key_uri = NULL;
      iv = NULL;
      while (data && parse_attributes (&data, &a, &v)) {
        if (g_str_equal (a, "METHOD")) {
          if (g_str_equal (v, "NONE")) {
            encryption = GST_M3U8_ENCRYPTED_NONE;
          } else if (g_str_equal (v, "AES-128")) {
            encryption = GST_M3U8_ENCRYPTED_AES_128;
          } else {
            GST_WARNING ("Unsuppported encryption method..., skipping");
            goto next_line;
          }
        } else if (g_str_equal (a, "URI")) {
          gchar *dq, *r;

          if (*v == '"') {
            v = v + 1;  /* skip first double quote in uri */
          }
          dq = g_utf8_strrchr (v, -1, '"');
          if (dq)
            *dq = '\0';
          if (!gst_uri_is_valid (v)) {
            if (!make_valid_uri(self->uri, v, &key_uri))
              goto next_line;
          } else {
            key_uri = g_strdup(v);
          }
          r = g_utf8_strchr (v, -1, '\r');
          if (r)
            *r = '\0';
        } else if (g_str_equal (a, "IV")) {
          iv = g_malloc0 (GST_M3U8_IV_LEN);
          if (!iv_from_string (data, &data, iv)) {
            GST_WARNING ("Can't read IV");
            goto next_line;
          }
        }
      }
      if ((encryption == GST_M3U8_ENCRYPTED_NONE) || (key_uri == NULL)) {
        g_free(key_uri);
        g_free(iv);
        if (encryption != GST_M3U8_ENCRYPTED_NONE) {
          GST_WARNING ("Key uri not set");
          goto next_line;
        }
      }
      key = gst_m3u8_key_new (encryption, key_uri, iv,
          self->mediasequence);
      gst_m3u8_key_merge_data (key, self->keys);
      new_keys = g_list_prepend (new_keys, key);
    } else {
      GST_LOG ("Ignored line: %s", data);
    }

  next_line:
    if (!end)
      break;
    data = g_utf8_next_char (end);      /* skip \n */
  }

  /* redorder playlists by bitrate */
  if (self->lists) {
    gchar *top_variant_uri = NULL;

    if (!self->current_variant)
      top_variant_uri = GST_M3U8 (self->lists->data)->uri;
    else
      top_variant_uri = GST_M3U8 (self->current_variant->data)->uri;

    self->lists =
        g_list_sort (self->lists,
        (GCompareFunc) gst_m3u8_compare_playlist_by_bitrate);

    self->current_variant = g_list_find_custom (self->lists, top_variant_uri,
        (GCompareFunc) _m3u8_compare_uri);
  }

  if (self->keys) {
    g_list_foreach (self->keys, (GFunc) gst_m3u8_key_free, NULL);
    g_list_free (self->keys);
  }
  self->keys = new_keys;

  return TRUE;
}

GstM3U8Client *
gst_m3u8_client_new (const gchar * uri)
{
  GstM3U8Client *client;

  g_return_val_if_fail (uri != NULL, NULL);

  client = g_new0 (GstM3U8Client, 1);
  client->main = gst_m3u8_new ();
  client->current = NULL;
  client->sequence = -1;
  client->update_failed_count = 0;
  client->lock = g_mutex_new ();
  gst_m3u8_set_uri (client->main, g_strdup (uri));

  return client;
}

void
gst_m3u8_client_free (GstM3U8Client * self)
{
  g_return_if_fail (self != NULL);

  gst_m3u8_free (self->main);
  g_mutex_free (self->lock);
  g_free (self);
}

void
gst_m3u8_client_set_current (GstM3U8Client * self, GstM3U8 * m3u8)
{
  g_return_if_fail (self != NULL);

  GST_M3U8_CLIENT_LOCK (self);
  if (m3u8 != self->current) {
    self->current = m3u8;
    self->update_failed_count = 0;
  }
  GST_M3U8_CLIENT_UNLOCK (self);
}

gboolean
gst_m3u8_client_update (GstM3U8Client * self, gchar * data)
{
  GstM3U8 *m3u8;
  gboolean updated = FALSE;
  gboolean ret = FALSE;

  g_return_val_if_fail (self != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (self);
  m3u8 = self->current ? self->current : self->main;

  if (!gst_m3u8_update (m3u8, data, &updated))
    self->update_failed_count++;
  else
    self->update_failed_count = 0;

  if (!updated) {
    goto out;
  }

  /* select the first playlist, for now */
  if (!self->current) {
    if (self->main->lists) {
      self->current = self->main->current_variant->data;
    } else {
      self->current = self->main;
    }
  }

  if (m3u8->files && self->sequence == -1) {
    self->sequence = _get_start_sequence (self);
    GST_DEBUG ("Setting first sequence at %d", self->sequence);
  }

  ret = TRUE;
out:
  GST_M3U8_CLIENT_UNLOCK (self);
  return ret;
}

static gboolean
_find_next (GstM3U8MediaFile * file, GstM3U8Client * client)
{
  GST_DEBUG ("Found fragment %d", file->sequence);
  if (file->sequence >= client->sequence)
    return FALSE;
  return TRUE;
}

static gboolean
_find_key (GstM3U8Key * key, GstM3U8Client * client)
{
  if (key->sequence <= client->sequence)
    return FALSE;
  return TRUE;
}

void
gst_m3u8_client_get_current_position (GstM3U8Client * client,
    GstClockTime * timestamp)
{
  GList *l;
  GList *walk;

  l = g_list_find_custom (client->current->files, client,
      (GCompareFunc) _find_next);

  *timestamp = 0;
#ifdef GST_EXT_HLS_MODIFICATION
  if (!client->current->endlist)
    return;
#endif
  for (walk = client->current->files; walk; walk = walk->next) {
    if (walk == l)
      break;
    *timestamp += GST_M3U8_MEDIA_FILE (walk->data)->duration;
  }
}

gboolean
gst_m3u8_client_get_next_fragment (GstM3U8Client * client,
    gboolean * discontinuity, const gchar ** uri, GstClockTime * duration,
    GstClockTime * timestamp, GstM3U8Key ** key)
{
  GList *l;
  GstM3U8MediaFile *file;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->current != NULL, FALSE);
  g_return_val_if_fail (discontinuity != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (client);
  GST_DEBUG ("Looking for fragment %d", client->sequence);
  l = g_list_find_custom (client->current->files, client,
      (GCompareFunc) _find_next);
  if (l == NULL) {
    GST_M3U8_CLIENT_UNLOCK (client);
    return FALSE;
  }

  gst_m3u8_client_get_current_position (client, timestamp);

  file = GST_M3U8_MEDIA_FILE (l->data);

  *discontinuity = client->sequence != file->sequence;
  client->sequence = file->sequence;
  *key = NULL;
  if (client->current->keys) {
    l = g_list_find_custom (client->current->keys, client,
        (GCompareFunc) _find_key);
    if (l && (GST_M3U8_KEY (l->data)->method != GST_M3U8_ENCRYPTED_NONE))
      *key = GST_M3U8_KEY (l->data);
  }

  client->sequence++;

  *uri = file->uri;
  *duration = file->duration;

  GST_M3U8_CLIENT_UNLOCK (client);
  return TRUE;
}

static void
_sum_duration (GstM3U8MediaFile * self, GstClockTime * duration)
{
  *duration += self->duration;
}

GstClockTime
gst_m3u8_client_get_duration (GstM3U8Client * client)
{
  GstClockTime duration = 0;

  g_return_val_if_fail (client != NULL, GST_CLOCK_TIME_NONE);
  g_return_val_if_fail (client->current != NULL, GST_CLOCK_TIME_NONE);

  GST_M3U8_CLIENT_LOCK (client);
  /* We can only get the duration for on-demand streams */
  if (!client->current->endlist) {
    GST_M3U8_CLIENT_UNLOCK (client);
    return GST_CLOCK_TIME_NONE;
  }

  g_list_foreach (client->current->files, (GFunc) _sum_duration, &duration);
  GST_M3U8_CLIENT_UNLOCK (client);
  return duration;
}

GstClockTime
gst_m3u8_client_get_target_duration (GstM3U8Client * client)
{
  GstClockTime duration = 0;

  g_return_val_if_fail (client != NULL, GST_CLOCK_TIME_NONE);
  g_return_val_if_fail (client->current != NULL, GST_CLOCK_TIME_NONE);

  GST_M3U8_CLIENT_LOCK (client);
  duration = client->current->targetduration;
  GST_M3U8_CLIENT_UNLOCK (client);
  return duration;
}

static guint
_get_start_sequence (GstM3U8Client * client)
{
  GList *l;
  GstClockTime duration_limit, duration_count = 0;
  guint sequence = -1;

  if (client->current->endlist) {
    l = g_list_first (client->current->files);
    sequence = GST_M3U8_MEDIA_FILE (l->data)->sequence;
  } else {
    duration_limit = client->current->targetduration * 3;
    for (l = g_list_last (client->current->files); l; l = l->prev) {
      duration_count += GST_M3U8_MEDIA_FILE (l->data)->duration;
      sequence = GST_M3U8_MEDIA_FILE (l->data)->sequence;
      if (duration_count >= duration_limit) {
        break;
      }
    }
  }
  return sequence;
}

guint
gst_m3u8_client_get_start_sequence (GstM3U8Client * client)
{
  guint sequence;

  GST_M3U8_CLIENT_LOCK (client);
  sequence = _get_start_sequence (client);
  GST_M3U8_CLIENT_UNLOCK (client);
  return sequence;
}

const gchar *
gst_m3u8_client_get_uri (GstM3U8Client * client)
{
  const gchar *uri;

  g_return_val_if_fail (client != NULL, NULL);

  GST_M3U8_CLIENT_LOCK (client);
  uri = client->main->uri;
  GST_M3U8_CLIENT_UNLOCK (client);
  return uri;
}

const gchar *
gst_m3u8_client_get_current_uri (GstM3U8Client * client)
{
  const gchar *uri;

  g_return_val_if_fail (client != NULL, NULL);

  GST_M3U8_CLIENT_LOCK (client);
  uri = client->current->uri;
  GST_M3U8_CLIENT_UNLOCK (client);
  return uri;
}

gboolean
gst_m3u8_client_has_variant_playlist (GstM3U8Client * client)
{
  gboolean ret;

  g_return_val_if_fail (client != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (client);
  ret = (client->main->lists != NULL);
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

gboolean
gst_m3u8_client_is_live (GstM3U8Client * client)
{
  gboolean ret;

  g_return_val_if_fail (client != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (client);
  if (!client->current || client->current->endlist)
    ret = FALSE;
  else
    ret = TRUE;
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

GList *
gst_m3u8_client_get_playlist_for_bitrate (GstM3U8Client * client, guint bitrate)
{
  GList *list, *current_variant;

  GST_M3U8_CLIENT_LOCK (client);
  current_variant = client->main->current_variant;

  /*  Go to the highest possible bandwidth allowed */
  while (GST_M3U8 (current_variant->data)->bandwidth < bitrate) {
    list = g_list_next (current_variant);
    if (!list)
      break;
    current_variant = list;
  }

  while (GST_M3U8 (current_variant->data)->bandwidth > bitrate) {
    list = g_list_previous (current_variant);
    if (!list)
      break;
    current_variant = list;
  }
  GST_M3U8_CLIENT_UNLOCK (client);

  return current_variant;
}

gboolean
gst_m3u8_client_decrypt_init (GstM3U8Client * client, GstM3U8Key * key)
{
  gboolean evp_ret;
  guint *iv = NULL;

  if (!client->current->cipher_ctx)
    client->current->cipher_ctx = g_malloc0 (sizeof(EVP_CIPHER_CTX));

  EVP_CIPHER_CTX_init (client->current->cipher_ctx);
  if (key->method == GST_M3U8_ENCRYPTED_AES_128) {
    if (key->iv == NULL) {
      iv = g_malloc0 (GST_M3U8_IV_LEN);
      if (!iv_from_uint (client->sequence - 1, iv)) {
        GST_WARNING ("Can't convert IV from sequence");
        return FALSE;
      }
    } else {
      iv = key->iv;
    }
    evp_ret = EVP_DecryptInit_ex (client->current->cipher_ctx, EVP_aes_128_cbc(),
        NULL, key->data, iv);
    if (key->iv == NULL)
      g_free (iv);
    return evp_ret;
  } else {
    return FALSE;
  }
}

gboolean
gst_m3u8_client_decrypt_update (GstM3U8Client * client, guint8 * out_data,
    gint * out_size, guint8 * in_data, gint in_size)
{
  g_return_val_if_fail (client->current->cipher_ctx != NULL, FALSE);

  return EVP_DecryptUpdate (client->current->cipher_ctx, out_data, out_size,
      in_data, in_size);
}

gboolean
gst_m3u8_client_decrypt_final (GstM3U8Client * client, guint8 * out_data,
    gint * out_size)
{
  g_return_val_if_fail (client->current->cipher_ctx != NULL, FALSE);

  return EVP_DecryptFinal_ex (client->current->cipher_ctx, out_data, out_size);
}

