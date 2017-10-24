/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2005 Takuro Ashie <ashie@homa.ne.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __SCIM_ANTHY_HELPER_H__
#define __SCIM_ANTHY_HELPER_H__

#include <scim_trans_commands.h>

#define SCIM_ANTHY_HELPER_UUID "24a65e2b-10a8-4d4c-adc9-266678cb1a38"

#define SCIM_ANTHY_TRANS_CMD_NEW_IC         SCIM_TRANS_CMD_USER_DEFINED + 1
#define SCIM_ANTHY_TRANS_CMD_DELETE_IC      SCIM_TRANS_CMD_USER_DEFINED + 2
#define SCIM_ANTHY_TRANS_CMD_GET_SELECTION  SCIM_TRANS_CMD_USER_DEFINED + 3
#define SCIM_ANTHY_TRANS_CMD_TIMEOUT_ADD    SCIM_TRANS_CMD_USER_DEFINED + 4
#define SCIM_ANTHY_TRANS_CMD_TIMEOUT_REMOVE SCIM_TRANS_CMD_USER_DEFINED + 5
#define SCIM_ANTHY_TRANS_CMD_TIMEOUT_NOTIFY SCIM_TRANS_CMD_USER_DEFINED + 6

#endif /* __SCIM_ANTHY_HELPER_H__ */
