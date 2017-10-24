/*
 * Copyright 2011 Red Hat, Inc
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

#ifndef EXT_NEXT_PROTOCOL_NEGOTIATION_H
#define EXT_NEXT_PROTOCOL_NEGOTIATION_H

#ifdef ENABLE_TIZEN_NPN

#include <gnutls_extensions.h>

extern extension_entry_st ext_mod_npn;

int _gnutls_send_next_protocol (gnutls_session_t session, int again);
int _gnutls_recv_next_protocol (gnutls_session_t session);

#endif /* ENABLE_TIZEN_NPN */
#endif /* EXT_NEXT_PROTOCOL_NEGOTIATION_H */

