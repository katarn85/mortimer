/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2004 Hiroyuki Ikezoe
 *  Copyright (C) 2004 Takuro Ashie
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

#include "scim_anthy_key2kana.h"
#include "scim_anthy_factory.h"
#include "scim_anthy_imengine.h"
#include "scim_anthy_utils.h"

using namespace scim_anthy;

Key2KanaConvertor::Key2KanaConvertor (AnthyInstance    & anthy,
                                      Key2KanaTableSet & tables)
    : m_anthy                   (anthy),
      m_tables                  (tables),
      m_is_in_pseudo_ascii_mode (false)
{
    set_case_sensitive (false);
    set_pseudo_ascii_mode (0);
}

Key2KanaConvertor::~Key2KanaConvertor ()
{
}

bool
Key2KanaConvertor::can_append (const KeyEvent & key,
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

    if (isprint(key.get_ascii_code ()) &&
        (ignore_space || !isspace(key.get_ascii_code ())))
        return true;

    if (util_key_is_keypad (key))
        return true;

    return false;
}

bool
Key2KanaConvertor::append (const KeyEvent & key,
                           WideString & result,
                           WideString & pending,
                           String &raw)
{
    if (!can_append (key))
        return false;

    m_last_key = key;

    util_keypad_to_string (raw, key);

    if (util_key_is_keypad (key)) {
        bool retval = false;
        WideString wide;
        String ten_key_type = m_anthy.get_factory()->m_ten_key_type;

        // convert key pad string to wide
        if ((ten_key_type == "FollowMode" &&
             (m_anthy.get_input_mode () == SCIM_ANTHY_MODE_LATIN ||
              m_anthy.get_input_mode () == SCIM_ANTHY_MODE_HALF_KATAKANA)) ||
            ten_key_type == "Half")
        {
            wide = utf8_mbstowcs (raw);
        } else {
            util_convert_to_wide (wide, raw);
        }

        // join to previous string
        if (!m_exact_match.is_empty()) {
            if (!m_exact_match.get_result(0).empty() &&
                m_exact_match.get_result(1).empty())
            {
                result = utf8_mbstowcs (m_exact_match.get_result(0));
            } else {
                retval = true; /* commit prev pending */
            }
            result += wide;
        } else {
            if (m_pending.length () > 0)
                retval = true; /* commit prev pending */
            result = wide;
        }

        m_pending.clear ();
        m_exact_match.clear ();

        return retval;

    } else {
        // the key isn't keypad
        return append (raw, result, pending);
    }
}

bool
Key2KanaConvertor::append (const String & str,
                           WideString & result, WideString & pending)
{
    WideString widestr = utf8_mbstowcs (str);
    WideString matching_str = m_pending + widestr;
    Key2KanaRule exact_match;
    bool has_partial_match = false;
    bool retval = false;

    if (m_pseudo_ascii_mode != 0 && process_pseudo_ascii_mode (widestr)) {
        m_pending += widestr;
        pending = m_pending;
        return false;
    }
    if (!m_case_sensitive) {
        String half = utf8_wcstombs (matching_str);
        for (unsigned int i = 0; i < half.length (); i++)
            half[i] = tolower (half[i]);
        matching_str = utf8_mbstowcs (half);
    }

    /* find matched table */
    if ((m_anthy.get_typing_method () == SCIM_ANTHY_TYPING_METHOD_KANA) &&
        (m_last_key.mask & /*SCIM_KEY_QuirkKanaRoMask*/ (1<<14)) &&
        (m_anthy.get_factory()->m_kana_layout_ro_key.length () > 0))
    {
        // Special treatment for Kana "Ro" key.
        // This code is a temporary solution. It doesn't care some minor cases.
        std::vector<String> kana_ro_result;
        scim_split_string_list (kana_ro_result,
                                m_anthy.get_factory()->m_kana_layout_ro_key);
        Key2KanaRule kana_ro_rule("\\", kana_ro_result);
        result = utf8_mbstowcs (kana_ro_rule.get_result (0));
        m_pending.clear ();
        m_exact_match.clear ();
        if (matching_str == utf8_mbstowcs ("\\")) {
            return false;
        } else {
            return true;
        }

    } else {
        std::vector<Key2KanaTable*> &tables = m_tables.get_tables();
        for (unsigned int j = 0; j < tables.size(); j++) {
            if (!tables[j])
                continue;

            Key2KanaRules &rules = tables[j]->get_table ();

            for (unsigned int i = 0; i < rules.size(); i++) {
                /* matching */
                String seq = rules[i].get_sequence ();
                if (!m_case_sensitive) {
                    for (unsigned int j = 0; j < seq.length (); j++)
                        seq[j] = tolower (seq[j]);
                }
                WideString romaji = utf8_mbstowcs(seq);
                if (romaji.find (matching_str) == 0) {
                    if (romaji.length () == matching_str.length ()) {
                        /* exact match */
                        exact_match = rules[i];
                    } else {
                        /* partial match */
                        has_partial_match = true;
                    }
                }
            }
        }
    }

    /* return result */
    if (has_partial_match) {
        m_exact_match = exact_match;
        result.clear ();
        m_pending += widestr;
        pending   =  m_pending;

    } else if (!exact_match.is_empty()) {
        if (!exact_match.get_result(1).empty())
            m_exact_match = exact_match;
        else
            m_exact_match.clear ();
        m_pending = utf8_mbstowcs (exact_match.get_result (1));
        result    = utf8_mbstowcs (exact_match.get_result (0));
        pending   = m_pending;

    } else {
        if (!m_exact_match.is_empty()) {
            if (!m_exact_match.get_result(0).empty() &&
                m_exact_match.get_result(1).empty())
            {
                result = utf8_mbstowcs (m_exact_match.get_result(0));
            } else {
                retval = true; /* commit prev pending */
            }
            m_pending.clear ();
            m_exact_match.clear ();

            WideString tmp_result;
            append(str, tmp_result, pending);
            result += tmp_result;

        } else {
            if (m_pending.length () > 0) {
                retval     = true; /* commit prev pending */
                m_pending  = widestr;
                pending    = m_pending;

            } else {
                result     = widestr;
                pending.clear();
                m_pending.clear ();
            }
        }
    }

    return retval;
}

void
Key2KanaConvertor::clear (void)
{
    m_pending.clear ();
    m_exact_match.clear ();
    m_last_key = KeyEvent ();
    reset_pseudo_ascii_mode();
}

bool
Key2KanaConvertor::is_pending (void)
{
    if (m_pending.length () > 0)
        return true;
    else
        return false;
}

WideString
Key2KanaConvertor::get_pending (void)
{
    return m_pending;
}

WideString
Key2KanaConvertor::flush_pending (void)
{
    WideString result;
    if (!m_exact_match.is_empty ()) {
        if (!m_exact_match.get_result(0).empty() &&
            m_exact_match.get_result(1).empty())
        {
            result = utf8_mbstowcs (m_exact_match.get_result(0));
        } else if (!m_exact_match.get_result(1).empty()) {
            result += utf8_mbstowcs (m_exact_match.get_result(1));
        } else if (m_pending.length () > 0) {
            result += m_pending;
        }
    }
    clear ();
    return result;
}

void
Key2KanaConvertor::reset_pending (const WideString &result, const String &raw)
{
    m_last_key = KeyEvent ();

    for (unsigned int i = 0; i < raw.length (); i++) {
        WideString res, pend;
        append (raw.substr(i, 1), res, pend);
    }
}

bool
Key2KanaConvertor::process_pseudo_ascii_mode (const WideString & wstr)
{
    for (unsigned int i = 0; i < wstr.length (); i++) {
        if ((wstr[i] >= 'A' && wstr[i] <= 'Z') ||
            iswspace(wstr[i]))
        {
            m_is_in_pseudo_ascii_mode = true;
        } else if (wstr[i] >= 0x80) {
            m_is_in_pseudo_ascii_mode = false;
        }
    }

    return m_is_in_pseudo_ascii_mode;
}

void
Key2KanaConvertor::reset_pseudo_ascii_mode (void)
{
    if (m_is_in_pseudo_ascii_mode)
        m_pending.clear();
    m_is_in_pseudo_ascii_mode = false;
}

/*
vi:ts=4:nowrap:ai:expandtab
*/
