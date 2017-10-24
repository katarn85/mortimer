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

#include "scim_anthy_kana.h"
#include "scim_anthy_factory.h"
#include "scim_anthy_imengine.h"
#include "scim_anthy_default_tables.h"
#include "scim_anthy_utils.h"

using namespace scim_anthy;

static bool
has_voiced_consonant (String str)
{
    VoicedConsonantRule *table = scim_anthy_voiced_consonant_table;

    for (unsigned int i = 0; table[i].string; i++) {
        if (!strcmp (str.c_str (), table[i].string) &&
            table[i].voiced && *table[i].voiced)
        {
            return true;
        }
    }

    return false;
}

static bool
has_half_voiced_consonant (String str)
{
    VoicedConsonantRule *table = scim_anthy_voiced_consonant_table;

    for (unsigned int i = 0; table[i].string; i++) {
        if (!strcmp (str.c_str (), table[i].string) &&
            table[i].half_voiced && *table[i].half_voiced)
        {
            return true;
        }
    }

    return false;
}

String
to_voiced_consonant (String str)
{
    VoicedConsonantRule *table = scim_anthy_voiced_consonant_table;

    for (unsigned int i = 0; table[i].string; i++) {
        if (!strcmp (str.c_str (), table[i].string))
            return String (table[i].voiced);
    }

    return str;
}

String
to_half_voiced_consonant (String str)
{
    VoicedConsonantRule *table = scim_anthy_voiced_consonant_table;

    for (unsigned int i = 0; table[i].string; i++) {
        if (!strcmp (str.c_str (), table[i].string))
            return String (table[i].half_voiced);
    }

    return str;
}

KanaConvertor::KanaConvertor (AnthyInstance &anthy)
    : m_anthy (anthy)
{
}

KanaConvertor::~KanaConvertor ()
{
}

bool
KanaConvertor::can_append (const KeyEvent & key,
                           bool             ignore_space)
{
    // ignore key release.
    if (key.is_key_release ())
        return false;

    // ignore short cut keys of apllication.
    if (key.mask & SCIM_KEY_ControlMask ||
        key.mask & SCIM_KEY_AltMask)
    {
        return false;
    }

    if (key.code == SCIM_KEY_overline ||
        (key.code >= SCIM_KEY_kana_fullstop &&
         key.code <= SCIM_KEY_semivoicedsound))
    {
        return true;
    }

#if 0
    if (key.code == SCIM_KEY_KP_Equal ||
        (key.code >= SCIM_KEY_KP_Multiply &&
         key.code <= SCIM_KEY_KP_9))
    {
        return true;
    }
#endif

    return false;
}

bool
KanaConvertor::append (const KeyEvent & key,
                       WideString & result,
                       WideString & pending,
                       String &raw)
{
    KeyCodeToCharRule *table = scim_anthy_keypad_table;

    // handle keypad code
    if (key.code == SCIM_KEY_KP_Equal ||
        (key.code >= SCIM_KEY_KP_Multiply &&
         key.code <= SCIM_KEY_KP_9))
    {
        String ten_key_type = m_anthy.get_factory()->m_ten_key_type;

        for (unsigned int i = 0; table[i].code; i++) {
            if (table[i].code == key.code) {
                if (ten_key_type == "Wide")
                    util_convert_to_wide (result, table[i].kana);
                else
                    result = utf8_mbstowcs (table[i].kana);
                raw = table[i].kana;

                return false;
            }
        }
    }

    table = scim_anthy_kana_table;

    // handle voiced sound
    if (key.code == SCIM_KEY_voicedsound &&
        !m_pending.empty () && has_voiced_consonant (m_pending))
    {
        result = utf8_mbstowcs (to_voiced_consonant (m_pending));
        raw    = key.get_ascii_code ();
        m_pending = String ();
        return false;
    }

    // handle semi voiced sound
    if (key.code == SCIM_KEY_semivoicedsound &&
        !m_pending.empty () && has_half_voiced_consonant (m_pending))
    {
        result = utf8_mbstowcs (to_half_voiced_consonant (m_pending));
        raw    = key.get_ascii_code ();
        m_pending = String ();
        return false;
    }

    // kana key code
    for (unsigned int i = 0; table[i].code; i++) {
        if (table[i].code == key.code) {
            bool retval = m_pending.empty () ? false : true;

            if (has_voiced_consonant (table[i].kana)) {
                result = WideString ();
                pending = utf8_mbstowcs (table[i].kana);
                m_pending = table[i].kana;
            } else {
                result = utf8_mbstowcs (table[i].kana);
                m_pending = String ();
            }
            raw = key.get_ascii_code ();

            return retval;
        }
    }

    String s;
    s += key.get_ascii_code ();
    raw    = s;

    return append (raw, result, pending);
}

bool
KanaConvertor::append (const String & str,
                       WideString   & result,
                       WideString   & pending)
{
    result = utf8_mbstowcs (str);
    m_pending = String ();

    return false;
}

void
KanaConvertor::clear (void)
{
    m_pending = String ();
}

bool
KanaConvertor::is_pending (void)
{
    return !m_pending.empty ();
}

WideString
KanaConvertor::get_pending (void)
{
    return WideString (utf8_mbstowcs (m_pending));
}

WideString
KanaConvertor::flush_pending (void)
{
    return WideString ();
}

void
KanaConvertor::reset_pending (const WideString &result, const String &raw)
{
    m_pending = String ();
    if (has_voiced_consonant (utf8_wcstombs (result)))
        m_pending = utf8_wcstombs (result);
}
/*
vi:ts=4:nowrap:ai:expandtab
*/
