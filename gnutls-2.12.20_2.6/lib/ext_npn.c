/*
 * Copyright 2011 Red Hat, Inc.
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <gnutls_int.h>

#ifdef ENABLE_TIZEN_NPN

#include <gnutls_errors.h>
#include <ext_npn.h>
#include <gnutls_handshake.h>
#include <gnutls_mbuffers.h>

static int _gnutls_npn_recv_params (gnutls_session_t state,
                                   const opaque * data, size_t data_size);
static int _gnutls_npn_send_params (gnutls_session_t state, opaque * data, size_t data_size);
static void _gnutls_npn_deinit_data (extension_priv_data_t priv);

extension_entry_st ext_mod_npn = {
  .name = "NEXT PROTOCOL NEGOTIATION",
  .type = GNUTLS_EXTENSION_NEXT_PROTOCOL_NEGOTIATION,
  .parse_type = GNUTLS_EXT_APPLICATION,

  .recv_func = _gnutls_npn_recv_params,
  .send_func = _gnutls_npn_send_params,
  .pack_func = NULL,
  .unpack_func = NULL,
  .deinit_func = _gnutls_npn_deinit_data,
};

typedef struct {
  int peer_using_npn;
  gnutls_datum_t protocols;
  gnutls_datum_t next_protocol;
  int next_protocol_mutually_supported;
} npn_ext_st;

static npn_ext_st *
npn_get_priv (gnutls_session_t session)
{
  extension_priv_data_t epriv;
  int ret;

  ret = _gnutls_ext_get_session_data (session, GNUTLS_EXTENSION_NEXT_PROTOCOL_NEGOTIATION, &epriv);
  if (ret == 0)
    return epriv.ptr;
  else
    return NULL;
}

static int
npn_validate_protocols (const void *protocols,
      size_t length)
{
  const uint8_t *data = protocols;
  int plen;

  while (length)
    {
      plen = *data;
      if (plen == 0 || plen + 1 > length)
  return GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;

      data += plen + 1;
      length -= plen + 1;
    }

  return 0;
}

static int
npn_protocols_contain (gnutls_datum *protocols,
           const void *protocol,
           size_t length)
{
  uint8_t *my_protocols = protocols->data;
  int len = protocols->size;
  int plen;

  while (len)
    {
      plen = *(my_protocols++);

      if (plen == length &&
    memcmp (protocol, my_protocols, plen) == 0)
  return 1;

      my_protocols += plen;
      len -= plen + 1;
    }

  return 0;
}

static int
npn_recv_client (gnutls_session_t session,
     const opaque *data, size_t data_size,
     npn_ext_st *priv)
{
  uint16_t len;
  const uint8_t *protocols;
  int ret, plen;

  ret = npn_validate_protocols (data, data_size);
  if (ret < 0)
    {
      gnutls_assert ();
      return GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;
    }

  if (data_size == 0)
    return 0;

  /* Try to find a mutually-supported protocol */
  protocols = data;
  len = data_size;
  while (len)
    {
      plen = *(protocols++);

      if (npn_protocols_contain (&priv->protocols, protocols, plen))
  {
    priv->next_protocol.size = plen;
    priv->next_protocol.data = gnutls_malloc (plen);
    memcpy (priv->next_protocol.data, protocols, plen);
    priv->next_protocol_mutually_supported = 1;
    break;
  }

      protocols += plen;
      len -= plen + 1;
    }

  /* If not, just use the first protocol  */
  if (!priv->next_protocol_mutually_supported)
    {
      protocols = priv->protocols.data;
      plen = *(protocols++);

      priv->next_protocol.size = plen;
      priv->next_protocol.data = gnutls_malloc (plen);
      memcpy (priv->next_protocol.data, protocols, plen);
    }

  return 0;
}

static int
npn_recv_server (gnutls_session_t session,
     const opaque *data, size_t data_size,
     npn_ext_st *priv)
{
  if (data_size != 0)
    {
      gnutls_assert ();
      return GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;
    }

  return 0;
}

static int
_gnutls_npn_recv_params (gnutls_session_t session,
       const opaque * data, size_t data_size)
{
  npn_ext_st *priv;
  int ret;

  priv = npn_get_priv (session);
  if (priv == NULL)
    return 0;

  priv->peer_using_npn = 1;

  if (session->security_parameters.entity == GNUTLS_CLIENT)
    return npn_recv_client (session, data, data_size, priv);
  else
    return npn_recv_server (session, data, data_size, priv);
}

static int
npn_send_client (gnutls_session_t session,
     gnutls_buffer_st *extdata,
     npn_ext_st *priv)
{
  return GNUTLS_E_INT_RET_0;
}

static int
npn_send_server (gnutls_session_t session,
     gnutls_buffer_st *extdata,
     npn_ext_st *priv)
{
  int ret;

  if (!priv->peer_using_npn)
    return 0;

  if (priv->protocols.data == NULL)
    return GNUTLS_E_INT_RET_0;

  ret = _gnutls_buffer_append_data_prefix (extdata,
             priv->protocols.data,
             priv->protocols.size);
  return ret;
}

static int
_gnutls_npn_send_params (gnutls_session_t session, opaque * data, size_t data_size)
{
  npn_ext_st *priv;
  int ret;
  gnutls_buffer_st extdata;

  priv = npn_get_priv (session);
  if (priv == NULL)
    return 0;

  /* Only send on initial handshake */
  if (session->internals.initial_negotiation_completed)
    return 0;

  if (session->security_parameters.entity == GNUTLS_CLIENT)
  {
    return npn_send_client (session, NULL, priv);
  }
  else
  {
  _gnutls_buffer_init(&extdata);

    ret = _gnutls_buffer_append_data(&extdata, data, data_size);

    return npn_send_server (session, &extdata, priv);
  }
}

int
_gnutls_send_next_protocol (gnutls_session_t session, int again)
{
  mbuffer_st *bufel = NULL;
  uint8_t *data = NULL, *p;
  int data_size = 0;
  int ret, pad_len, len;
  npn_ext_st *priv = NULL;

  if (again == 0)
    {
      priv = npn_get_priv (session);
      if (priv == NULL)
        return 0;

      if (!priv->next_protocol.size)
        return 0;

      pad_len = 32 - ((priv->next_protocol.size + 2) % 32);
      len = priv->next_protocol.size + 2 + pad_len;
      bufel = _gnutls_handshake_alloc (len, len);
      if (!bufel)
        {
          gnutls_assert ();
          return GNUTLS_E_MEMORY_ERROR;
        }

      data = _mbuffer_get_udata_ptr (bufel);
      p = data;

      *p++ = priv->next_protocol.size;
      memcpy (p, priv->next_protocol.data, priv->next_protocol.size);
      p += priv->next_protocol.size;

      *p++ = pad_len;
      memset (p, 0, pad_len);

      data_size = len;
    }
  return _gnutls_send_handshake (session, data_size ? bufel : NULL,
         GNUTLS_HANDSHAKE_NEXT_PROTOCOL);
}

int
_gnutls_recv_next_protocol (gnutls_session_t session)
{
  uint8_t *p;
  int data_size;
  gnutls_buffer_st buf;
  int ret;
  npn_ext_st *priv = NULL;

  priv = npn_get_priv (session);
  if (priv == NULL)
    return 0;

  ret = _gnutls_recv_handshake (session, &p , &data_size,
                                GNUTLS_HANDSHAKE_NEXT_PROTOCOL,
                                MANDATORY_PACKET);
  if (ret < 0)
  {
    //return gnutls_assert_val_fatal (ret);
    gnutls_assert ();
    return GNUTLS_E_INTERNAL_ERROR;
   }

  //p = buf.data;
  //data_size = buf.length;

  DECR_LENGTH_COM (data_size, 2, ret = GNUTLS_E_UNEXPECTED_PACKET_LENGTH; goto error);
  priv->next_protocol.size = _gnutls_read_uint16 (p);
  p += 2;

  DECR_LENGTH_COM (data_size, priv->next_protocol.size, ret = GNUTLS_E_UNEXPECTED_PACKET_LENGTH; goto error);
  priv->next_protocol.data = gnutls_malloc (priv->next_protocol.size);
  if (!priv->next_protocol.data)
    {
      gnutls_assert ();
      ret = GNUTLS_E_MEMORY_ERROR;
      goto error;
    }
  memcpy (priv->next_protocol.data, p, priv->next_protocol.size);

  if (npn_protocols_contain (&priv->protocols, priv->next_protocol.data, priv->next_protocol.size))
    priv->next_protocol_mutually_supported = 1;

  ret = 0;

error:
  _gnutls_buffer_clear (&buf);

  return ret;
}


static void
_gnutls_npn_deinit_data (extension_priv_data_t priv)
{
  gnutls_free (priv.ptr);
}

int
gnutls_negotiate_next_protocol (gnutls_session_t session,
        const void *protocols,
        size_t protocols_length)
{
  extension_priv_data_t epriv;
  npn_ext_st *priv;
  int ret;

  ret = _gnutls_ext_get_session_data (session, GNUTLS_EXTENSION_NEXT_PROTOCOL_NEGOTIATION, &epriv);
  if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
    {
      priv = gnutls_calloc (1, sizeof (npn_ext_st));
      if (priv == NULL)
  {
    gnutls_assert ();
    return GNUTLS_E_MEMORY_ERROR;
  }

      epriv.ptr = priv;
      _gnutls_ext_set_session_data (session, GNUTLS_EXTENSION_NEXT_PROTOCOL_NEGOTIATION, epriv);
    }
  else if (ret < 0)
    return ret;

  priv = epriv.ptr;
  if (protocols_length > 0)
    {
      ret = npn_validate_protocols (protocols, protocols_length);
      if (ret < 0)
  return ret;

      priv->protocols.data = gnutls_malloc (protocols_length);
      priv->protocols.size = protocols_length;
      memcpy (priv->protocols.data, protocols, protocols_length);
    }

  return 0;
}

int
gnutls_get_next_protocol (gnutls_session_t session,
        void *protocol, size_t *protocol_length,
        unsigned int *supported)
{
  npn_ext_st *priv;
  int ret;

  priv = npn_get_priv (session);
  if (priv == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
    }

  if (priv->next_protocol.size == 0)
    return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;

  if (*protocol_length < priv->next_protocol.size)
    {
      *protocol_length = priv->next_protocol.size;
      return GNUTLS_E_SHORT_MEMORY_BUFFER;
    }

  *protocol_length = priv->next_protocol.size;
  memcpy (protocol, priv->next_protocol.data, priv->next_protocol.size);

  if (supported)
    *supported = priv->next_protocol_mutually_supported;

  return 0;
}

#else

int
gnutls_negotiate_next_protocol (gnutls_session_t session,
        const void *protocols,
        size_t protocols_length)
{
  return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
}

int
gnutls_get_next_protocol (gnutls_session_t session,
        void *protocol, size_t *protocol_length,
        unsigned int *supported)
{
  return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
}
#endif /* ENABLE_TIZEN_NPN */
