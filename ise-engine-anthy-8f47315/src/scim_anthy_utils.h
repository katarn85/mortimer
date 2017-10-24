/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2005 Takuro Ashie
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

#ifndef __SCIM_ANTHY_UTILS_H__
#define __SCIM_ANTHY_UTILS_H__

#define Uses_SCIM_ICONV
#define Uses_SCIM_EVENT
#define Uses_SCIM_ATTRIBUTE
#include <scim.h>

using namespace scim;

namespace scim_anthy {

bool util_match_key_event     (const KeyEventList  &list,
                               const KeyEvent      &key,
                               uint16               ignore_mask = 0);
void util_split_string        (String              &str,
                               std::vector<String> &str_list,
                               char                *delim,
                               int                  num);
void util_convert_to_wide     (WideString          &wide,
                               const String        &str);
void util_convert_to_half     (String              &half,
                               const WideString    &str);
void util_convert_to_katakana (WideString          &kata,
                               const WideString    &hira,
                               bool                 half = false);
void util_create_attributes   (AttributeList       &attrs,
                               unsigned int         start,
                               unsigned int         length,
                               String               type,
                               unsigned int         fg_color,
                               unsigned int         bg_color);
bool util_key_is_keypad       (const KeyEvent      &key);
void util_keypad_to_string    (String              &str,
                               const KeyEvent      &key);
void util_launch_program      (const char          *command);

}

#endif /* __SCIM_ANTHY_UTILS_H__ */
