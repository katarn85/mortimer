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

#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include "scim_anthy_utils.h"
#include "scim_anthy_default_tables.h"

namespace scim_anthy {

bool
util_match_key_event (const KeyEventList &list, const KeyEvent &key,
                      uint16 ignore_mask)
{
    KeyEventList::const_iterator kit;

    for (kit = list.begin (); kit != list.end (); kit++) {
        uint16 mod1, mod2;

        mod1 = kit->mask;
        mod2 = key.mask;
        mod1 &= ~ignore_mask;
        mod2 &= ~ignore_mask;

        if (key.code == kit->code && mod1 == mod2)
             return true;
    }
    return false;
}

void
util_split_string (String &str, std::vector<String> &str_list,
                   char *delim, int num)
{
    String::size_type start = 0, end;

    for (int i = 0; (num > 0 && i < num) || start < str.length (); i++) {
        end = str.find (delim, start);
        if ((num > 0 && i == num - 1) || (end == String::npos))
            end = str.length ();

        if (start < str.length ()) {
            str_list.push_back (str.substr (start, end - start));
            start = end + strlen (delim);
        } else {
            str_list.push_back (String ());
        }
    }
}

void
util_convert_to_wide (WideString & wide, const String & str)
{
    if (str.length () < 0)
        return;

    for (unsigned int i = 0; i < str.length (); i++) {
        int c = str[i];
        char cc[2]; cc[0] = c; cc[1] = '\0';
        bool found = false;

        for (unsigned int j = 0; scim_anthy_wide_table[j].code; j++) {
            if ( scim_anthy_wide_table[j].code &&
                *scim_anthy_wide_table[j].code == c)
            {
                wide += utf8_mbstowcs (scim_anthy_wide_table[j].wide);
                found = true;
                break;
            }
        }

        if (!found)
            wide += utf8_mbstowcs (cc);
    }
}

void
util_convert_to_half (String & half, const WideString & str)
{
    if (str.length () < 0)
        return;

    for (unsigned int i = 0; i < str.length (); i++) {
        WideString wide = str.substr (i, 1);
        bool found = false;

        for (unsigned int j = 0; scim_anthy_wide_table[j].code; j++) {
            if (scim_anthy_wide_table[j].wide &&
                wide == utf8_mbstowcs (scim_anthy_wide_table[j].wide))
            {
                half += scim_anthy_wide_table[j].code;
                found = true;
                break;
            }
        }

        if (!found)
            half += utf8_wcstombs (wide);
    }
}

void
util_convert_to_katakana (WideString & kata,
                          const WideString & hira,
                          bool half)
{
    if (hira.length () < 0)
        return;

    for (unsigned int i = 0; i < hira.length (); i++) {
        WideString tmpwide;
        bool found = false;

        HiraganaKatakanaRule *table = scim_anthy_hiragana_katakana_table;

        for (unsigned int j = 0; table[j].hiragana; j++) {
            tmpwide = utf8_mbstowcs (table[j].hiragana);
            if (hira.substr(i, 1) == tmpwide) {
                if (half)
                    kata += utf8_mbstowcs (table[j].half_katakana);
                else
                    kata += utf8_mbstowcs (table[j].katakana);
                found = true;
                break;
            }
        }

        if (!found)
            kata += hira.substr(i, 1);
    }
}

void
util_create_attributes (AttributeList &attrs,
                        unsigned int start,
                        unsigned int length,
                        String type,
                        unsigned int fg_color,
                        unsigned int bg_color)
{
    if (type == "None") {
        return;
    } else if (type == "Underline") {
        attrs.push_back (Attribute (start, length,
                                    SCIM_ATTR_DECORATE, 
                                    SCIM_ATTR_DECORATE_UNDERLINE));
    } else if (type == "Reverse") {
        attrs.push_back (Attribute (start, length,
                                    SCIM_ATTR_DECORATE, 
                                    SCIM_ATTR_DECORATE_REVERSE));
    } else if (type == "Highlight") {
        attrs.push_back (Attribute (start, length,
                                    SCIM_ATTR_DECORATE, 
                                    SCIM_ATTR_DECORATE_HIGHLIGHT));
    } else {
        if (type == "Color" || type == "FGColor")
            attrs.push_back (Attribute (start, length,
                                        SCIM_ATTR_FOREGROUND,
                                        fg_color));
        if (type == "Color" || type == "BGColor")
            attrs.push_back (Attribute (start, length,
                                        SCIM_ATTR_BACKGROUND,
                                        bg_color));
    }
}

bool
util_key_is_keypad (const KeyEvent &key)
{
    switch (key.code) {
    case SCIM_KEY_KP_Equal:
    case SCIM_KEY_KP_Multiply:
    case SCIM_KEY_KP_Add:
    case SCIM_KEY_KP_Separator:
    case SCIM_KEY_KP_Subtract:
    case SCIM_KEY_KP_Decimal:
    case SCIM_KEY_KP_Divide:
    case SCIM_KEY_KP_0:
    case SCIM_KEY_KP_1:
    case SCIM_KEY_KP_2:
    case SCIM_KEY_KP_3:
    case SCIM_KEY_KP_4:
    case SCIM_KEY_KP_5:
    case SCIM_KEY_KP_6:
    case SCIM_KEY_KP_7:
    case SCIM_KEY_KP_8:
    case SCIM_KEY_KP_9:
        return true;
    default:
        return false;
    }
}

void
util_keypad_to_string (String &str, const KeyEvent &key)
{
    char raw[2];

    switch (key.code) {
    case SCIM_KEY_KP_Equal:
        raw[0] = '=';
        break;

    case SCIM_KEY_KP_Multiply:
        raw[0] = '*';
        break;

    case SCIM_KEY_KP_Add:
        raw[0] = '+';
        break;

    case SCIM_KEY_KP_Separator:
        raw[0] = ',';
        break;

    case SCIM_KEY_KP_Subtract:
        raw[0] = '-';
        break;

    case SCIM_KEY_KP_Decimal:
        raw[0] = '.';
        break;

    case SCIM_KEY_KP_Divide:
        raw[0] = '/';
        break;

    case SCIM_KEY_KP_0:
    case SCIM_KEY_KP_1:
    case SCIM_KEY_KP_2:
    case SCIM_KEY_KP_3:
    case SCIM_KEY_KP_4:
    case SCIM_KEY_KP_5:
    case SCIM_KEY_KP_6:
    case SCIM_KEY_KP_7:
    case SCIM_KEY_KP_8:
    case SCIM_KEY_KP_9:
        raw[0] = '0' + key.code - SCIM_KEY_KP_0;
        break;

    default:
        if (isprint (key.get_ascii_code()))
            raw[0] = key.get_ascii_code();
        else
            raw[0] = '\0';
        break;
    }

    raw[1] = '\0';
    str = raw;
}

void
util_launch_program (const char *command)
{
    if (!command) return;

    /* split string */
    unsigned int len = strlen (command);
    char tmp[len + 1];
    strncpy (tmp, command, len);
    tmp[len] = '\0';

    char *str = tmp;
    std::vector<char *> array;

    for (unsigned int i = 0; i < len + 1; i++) {
        if (!tmp[i] || isspace (tmp[i])) {
            if (*str) {
                tmp[i] = '\0';
                array.push_back (str);
            }
            str = tmp + i + 1;
        }
    }

    if (array.size () <= 0) return;
    array.push_back (NULL);

    char *args[array.size()];
    for (unsigned int i = 0; i < array.size (); i++)
        args[i] = array[i];


    /* exec command */
	pid_t child_pid;

	child_pid = fork();
	if (child_pid < 0) {
		perror("fork");
	} else if (child_pid == 0) {		 /* child process  */
		pid_t grandchild_pid;

		grandchild_pid = fork();
		if (grandchild_pid < 0) {
			perror("fork");
			_exit(1);
		} else if (grandchild_pid == 0) { /* grandchild process  */
			execvp(args[0], args);
			perror("execvp");
			_exit(1);
		} else {
			_exit(0);
		}
	} else {                              /* parent process */
		int status;
		waitpid(child_pid, &status, 0);
	}
}

}
