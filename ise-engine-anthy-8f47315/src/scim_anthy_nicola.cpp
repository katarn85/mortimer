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

#define Uses_SCIM_IMENGINE
#define Uses_SCIM_CONFIG_BASE
#include "scim_anthy_nicola.h"
#include "scim_anthy_factory.h"
#include "scim_anthy_imengine.h"
#include "scim_anthy_utils.h"

using namespace scim_anthy;

NicolaConvertor::NicolaConvertor (AnthyInstance &anthy,
                                  Key2KanaTableSet &tables)
    : m_tables            (tables),
      m_anthy             (anthy),
      m_timer_id          (0),
      m_processing_timeout(false)
{
}

NicolaConvertor::~NicolaConvertor ()
{
    if (m_timer_id > 0)
        m_anthy.timeout_remove (m_timer_id);
}

bool
NicolaConvertor::can_append (const KeyEvent & key,
                             bool             ignore_space)
{
    if (key == m_through_key_event) {
        m_through_key_event = KeyEvent ();
        return false;
    }

    if (m_processing_timeout &&
        m_prev_char_key.empty () && !m_prev_thumb_key.empty())
    {
        emit_key_event (m_prev_thumb_key);
        m_prev_thumb_key = KeyEvent ();
        return false;
    }

    if (key.is_key_release () &&
        (key.code != m_prev_char_key.code &&
         key.code != m_prev_thumb_key.code &&
         key.code != m_repeat_char_key.code &&
         key.code != m_repeat_thumb_key.code))
    {
        return false;
    }

    if (is_repeating ()) {
        if (key.is_key_press () &&
            (key == m_repeat_char_key || key == m_repeat_thumb_key) &&
            m_repeat_char_key.empty ())
        {
            return false;
        }
    }

    // ignore short cut keys of apllication.
    if (key.mask & SCIM_KEY_ControlMask ||
        key.mask & SCIM_KEY_AltMask)
    {
        return false;
    }

    if (isprint (key.get_ascii_code ()) &&
        (ignore_space || !isspace (key.get_ascii_code ())))
    {
        return true;
    }

    if (is_thumb_key (key))
        return true;

    return false;
}

void
NicolaConvertor::search (const KeyEvent key,
                         NicolaShiftType shift_type,
                         WideString &result,
                         String &raw)
{
    raw = key.get_ascii_code ();

    String str1;
    if (get_case_sensitive ())
        str1 = raw;
    else
        str1 = tolower (key.get_ascii_code ());

    std::vector<Key2KanaTable*> &tables = m_tables.get_tables();
    for (unsigned int j = 0; j < tables.size (); j++) {
        if (!tables[j])
            continue;

        Key2KanaRules &rules = tables[j]->get_table ();

        for (unsigned int i = 0; i < rules.size (); i++) {
            String str2 = rules[i].get_sequence ();

            for (unsigned int k = 0;
                 !get_case_sensitive () && k < str2.length ();
                 k++)
            {
                str2[k] = tolower (str2[k]);
            }

            if (str1 == str2) {
                switch (shift_type) {
                case SCIM_ANTHY_NICOLA_SHIFT_RIGHT:
                    result = utf8_mbstowcs (rules[i].get_result (2));
                    break;
                case SCIM_ANTHY_NICOLA_SHIFT_LEFT:
                    result = utf8_mbstowcs (rules[i].get_result (1));
                    break;
                default:
                    result = utf8_mbstowcs (rules[i].get_result (0));
                    break;
                }
                break;
            }
        }
    }

    if (result.empty ()) {
        result = utf8_mbstowcs (raw);
    }
}

bool
NicolaConvertor::handle_voiced_consonant  (WideString & result,
                                           WideString & pending)
{
    VoicedConsonantRule *table = scim_anthy_voiced_consonant_table;

    if (result.empty ())
        return false;

    if (m_pending.empty ()) {
        for (unsigned int i = 0; table[i].string; i++) {
            if (result == utf8_mbstowcs (table[i].string)) {
                pending = m_pending = result;
                result = WideString ();
                return false;
            }
        }

    } else if (result == utf8_mbstowcs ("\xE3\x82\x9B")) {
        // voiced consonant
        for (unsigned int i = 0; table[i].string; i++) {
            if (m_pending == utf8_mbstowcs (table[i].string)) {
                result = utf8_mbstowcs (table[i].voiced);
                m_pending = WideString ();
                return false;
            }
        }
        return true;

    } else if (result == utf8_mbstowcs ("\xE3\x82\x9C")) {
        // half voiced consonant
        for (unsigned int i = 0; table[i].string; i++) {
            if (m_pending == utf8_mbstowcs (table[i].string)) {
                result = utf8_mbstowcs (table[i].half_voiced);
                m_pending = WideString ();
                return false;
            }
        }
        return true;

    } else {
        m_pending = WideString ();
        for (unsigned int i = 0; table[i].string; i++) {
            if (result == utf8_mbstowcs (table[i].string)) {
                pending = m_pending = result;
                result = WideString ();
                return true;
            }
        }
        return true;
    }

    return false;
}

bool
NicolaConvertor::is_char_key (const KeyEvent key)
{
    if (!is_thumb_key (key) && isprint (key.get_ascii_code ()))
        return true;
    else
        return false;
}

bool
NicolaConvertor::is_thumb_key (const KeyEvent key)
{
    if (is_left_thumb_key (key) || is_right_thumb_key (key))
        return true;

    return false;
}

bool
NicolaConvertor::is_left_thumb_key (const KeyEvent key)
{
    return util_match_key_event (m_anthy.get_factory()->m_left_thumb_keys,
                                 key,
                                 0xFFFF);
}

bool
NicolaConvertor::is_right_thumb_key (const KeyEvent key)
{
    return util_match_key_event (m_anthy.get_factory()->m_right_thumb_keys,
                                 key,
                                 0xFFFF);
}

NicolaShiftType
NicolaConvertor::get_thumb_key_type (const KeyEvent key)
{
    if (is_left_thumb_key (key))
        return SCIM_ANTHY_NICOLA_SHIFT_LEFT;
    else if (is_right_thumb_key (key))
        return SCIM_ANTHY_NICOLA_SHIFT_RIGHT;
    else
        return SCIM_ANTHY_NICOLA_SHIFT_NONE;
}

void
NicolaConvertor::on_key_repeat (const KeyEvent key,
                                WideString &result,
                                String &raw)
{
    if (key.is_key_release ()) {
        if (!m_repeat_char_key.empty ())
            emit_key_event (key);
        m_repeat_char_key  = KeyEvent ();
        m_repeat_thumb_key = KeyEvent ();
        m_prev_char_key    = KeyEvent ();
#if 1
        m_prev_thumb_key   = KeyEvent ();
#endif

    } else if (key == m_repeat_char_key || key == m_repeat_thumb_key) {
        if (!m_repeat_char_key.empty ()) {
            search (m_repeat_char_key, get_thumb_key_type (m_repeat_thumb_key),
                    result, raw);
        } else {
            // handle by can_append ();
        }


    } else if (!is_thumb_key (key) && !(key == m_repeat_char_key)) {
        m_repeat_char_key  = KeyEvent ();
        m_repeat_thumb_key = KeyEvent ();
        m_prev_char_key    = key;
        m_prev_thumb_key   = KeyEvent ();
        set_alarm (m_anthy.get_factory()->m_nicola_time);

    } else if (key == m_prev_thumb_key) {
        m_repeat_char_key  = KeyEvent ();
        m_repeat_thumb_key = KeyEvent ();
        m_prev_char_key    = KeyEvent ();
        m_prev_thumb_key   = key;
        set_alarm (m_anthy.get_factory()->m_nicola_time);

    } else {
        m_repeat_char_key  = KeyEvent ();
        m_repeat_thumb_key = KeyEvent ();
        m_prev_char_key    = KeyEvent ();
        m_prev_thumb_key   = KeyEvent ();
    }
}

void
NicolaConvertor::on_both_key_pressed (const KeyEvent key,
                                      WideString & result,
                                      String &raw)
{
    struct timeval cur_time;
    long diff1, diff2;
    gettimeofday (&cur_time, NULL);

    diff1 = m_time_thumb.tv_usec - m_time_char.tv_usec;
    diff2 = cur_time.tv_usec - m_time_thumb.tv_usec;

    if (key.is_key_press () && key == m_prev_thumb_key) {
        search (m_prev_char_key, get_thumb_key_type (m_prev_thumb_key),
                result, raw);
        m_repeat_char_key  = m_prev_char_key;
        m_repeat_thumb_key = m_prev_thumb_key;

    } else if (is_char_key (key)) {
        if (key.is_key_press ()) {
            if (diff2 < diff1) {
                WideString result1, result2;
                String raw1, raw2;
                search (m_prev_char_key, SCIM_ANTHY_NICOLA_SHIFT_NONE,
                        result1, raw1);
                search (key, get_thumb_key_type (m_prev_thumb_key),
                        result2, raw2);
                result = result1 + result2;
                raw = raw1 + raw2;

                // repeat
                m_repeat_char_key  = key;
                m_repeat_thumb_key = m_prev_thumb_key;

            } else {
                search (m_prev_char_key, get_thumb_key_type (m_prev_thumb_key),
                        result, raw);
                m_prev_char_key  = key;
#if 1
                m_prev_thumb_key = KeyEvent ();
#endif
                set_alarm (m_anthy.get_factory()->m_nicola_time);
            }

        } else {
            if (diff2 < m_anthy.get_factory()->m_nicola_time * 1000 &&
                diff1 > diff2)
            {
                search (m_prev_char_key, SCIM_ANTHY_NICOLA_SHIFT_NONE,
                        result, raw);
                m_prev_char_key = KeyEvent ();

            } else {
                search (m_prev_char_key, get_thumb_key_type (m_prev_thumb_key),
                        result, raw);
                m_prev_char_key  = KeyEvent ();
#if 1
                m_prev_thumb_key = KeyEvent ();
#endif
            }
        }

    } else if (is_thumb_key (key)) {
        if (key.is_key_press ()) {
            search (m_prev_char_key, get_thumb_key_type (m_prev_thumb_key),
                    result, raw);
            m_prev_char_key  = KeyEvent ();
            m_prev_thumb_key = key;
            gettimeofday (&m_time_thumb, NULL);
            set_alarm (m_anthy.get_factory()->m_nicola_time);

        } else {
            search (m_prev_char_key, get_thumb_key_type (m_prev_thumb_key),
                    result, raw);
            m_prev_char_key  = KeyEvent ();
            m_prev_thumb_key = KeyEvent ();
        }

    } else {
        search (m_prev_char_key, get_thumb_key_type (m_prev_thumb_key),
                result, raw);
        m_prev_char_key  = KeyEvent ();
        m_prev_thumb_key = KeyEvent ();
    }
}

void
NicolaConvertor::on_thumb_key_pressed (const KeyEvent key,
                                       WideString & result,
                                       String &raw)
{
    if (key.is_key_press () && key == m_prev_thumb_key) {
#if 1
        m_repeat_thumb_key = key;
#endif

    } else if (is_thumb_key (key) && key.is_key_release ()) {
        emit_key_event (m_prev_thumb_key); // key press event
        emit_key_event (key);              // key release event
        m_prev_thumb_key = KeyEvent ();

    } else if (is_thumb_key (key) & key.is_key_press ()) {
        emit_key_event (m_prev_thumb_key);
        m_prev_thumb_key = key;
        gettimeofday (&m_time_thumb, NULL);

    } else if (is_char_key (key) && key.is_key_press ()) {
        m_prev_char_key = key;
        gettimeofday (&m_time_char, NULL);

        search (m_prev_char_key, get_thumb_key_type (m_prev_thumb_key),
                result, raw);

        // repeat
        m_repeat_char_key  = m_prev_char_key;
        m_repeat_thumb_key = m_prev_thumb_key;

    } else {
        m_prev_thumb_key = KeyEvent ();
    }
}

void
NicolaConvertor::on_char_key_pressed (const KeyEvent key,
                                      WideString & result,
                                      String &raw)
{
    if (key.is_key_press () && key == m_prev_char_key) {
        search (m_prev_char_key, SCIM_ANTHY_NICOLA_SHIFT_NONE,
                result, raw);
        m_repeat_char_key = m_prev_char_key;

    } else if (is_char_key (key) && key.is_key_press ()) {
        search (m_prev_char_key, SCIM_ANTHY_NICOLA_SHIFT_NONE,
                result, raw);
        m_prev_char_key = key;
        gettimeofday (&m_time_char, NULL);
        set_alarm (m_anthy.get_factory()->m_nicola_time);

    } else if (is_thumb_key (key) && key.is_key_press ()) {
        m_prev_thumb_key = key;
        gettimeofday (&m_time_thumb, NULL);
        set_alarm (m_anthy.get_factory()->m_nicola_time);

    } else if (key.is_key_release () && key == m_prev_char_key) {
        search (m_prev_char_key, SCIM_ANTHY_NICOLA_SHIFT_NONE,
                result, raw);
        m_prev_char_key = KeyEvent ();

    } else {
        search (m_prev_char_key, SCIM_ANTHY_NICOLA_SHIFT_NONE,
                result, raw);
        m_prev_char_key = KeyEvent ();
    }
}

void
NicolaConvertor::on_no_key_pressed (const KeyEvent key)
{
    if (key.is_key_release ())
        return;

    if (is_char_key (key)) {
        m_prev_char_key = key;
        gettimeofday (&m_time_char, NULL);
        set_alarm (m_anthy.get_factory()->m_nicola_time);
    } else if (is_thumb_key (key)) {
        m_prev_thumb_key = key;
        gettimeofday (&m_time_thumb, NULL);
        set_alarm (m_anthy.get_factory()->m_nicola_time);
    }
}

bool
NicolaConvertor::is_repeating (void)
{
    return !m_repeat_char_key.empty () || !m_repeat_thumb_key.empty ();
}

void
NicolaConvertor::emit_key_event (const KeyEvent & key)
{
    m_through_key_event = key;

    //m_anthy.forward_key_event (key);
    m_anthy.process_key_event (key);
}

static void
timeout_emit_key_event (void *data)
{
    NicolaConvertor *nicola = static_cast<NicolaConvertor*> (data);
    nicola->process_timeout ();
}

void
NicolaConvertor::process_timeout (void)
{
    m_processing_timeout = true;
    if (!m_prev_char_key.empty ())
        m_anthy.process_key_event (m_prev_char_key);
    else if (!m_prev_thumb_key.empty ())
        m_anthy.process_key_event (m_prev_thumb_key);
    m_processing_timeout = false;
}

void
NicolaConvertor::set_alarm (int time_msec)
{
    if (time_msec < 5)
        time_msec = 5;
    if (time_msec > 1000)
        time_msec = 1000;

    m_timer_id = m_anthy.timeout_add (time_msec,
                                      timeout_emit_key_event,
                                      (void *) this);
}

bool
NicolaConvertor::append (const KeyEvent & key,
                         WideString & result,
                         WideString & pending,
                         String &raw)
{
    if (m_timer_id > 0) {
        m_anthy.timeout_remove (m_timer_id);
        m_timer_id = 0;
    }

    if (m_processing_timeout) {
        search (m_prev_char_key,
                get_thumb_key_type (m_prev_thumb_key),
                result, raw);
        if (m_prev_thumb_key.empty ()) {
            m_prev_char_key  = KeyEvent ();
            m_prev_thumb_key = KeyEvent ();
        } else {
            m_repeat_char_key  = m_prev_char_key;
            m_repeat_thumb_key = m_prev_thumb_key;
        }
        return handle_voiced_consonant (result, pending);
    }

    if (key.is_key_press () && util_key_is_keypad (key)) {
        util_keypad_to_string (raw, key);

        // convert key pad string to wide
        WideString wide;
        String ten_key_type = m_anthy.get_factory()->m_ten_key_type;
        if ((ten_key_type == "FollowMode" &&
             (m_anthy.get_input_mode () == SCIM_ANTHY_MODE_LATIN ||
              m_anthy.get_input_mode () == SCIM_ANTHY_MODE_HALF_KATAKANA)) ||
            ten_key_type == "Half")
        {
            wide = utf8_mbstowcs (raw);
        } else {
            util_convert_to_wide (wide, raw);
        }

        result = wide;

        m_repeat_char_key  = KeyEvent ();
        m_repeat_thumb_key = KeyEvent ();
        m_prev_char_key    = KeyEvent ();
        m_prev_thumb_key   = KeyEvent ();

        return handle_voiced_consonant (result, pending);
    }

    if (is_repeating ()) {
        on_key_repeat (key, result, raw);

    } else if (!m_prev_thumb_key.empty () && !m_prev_char_key.empty ()) {
        on_both_key_pressed (key, result, raw);

    } else if (!m_prev_thumb_key.empty ()) {
        on_thumb_key_pressed (key, result, raw);

    } else if (!m_prev_char_key.empty ()) {
        on_char_key_pressed (key, result, raw);

    } else {
        on_no_key_pressed (key);
    }

    return handle_voiced_consonant (result, pending);
}

bool
NicolaConvertor::append (const String & str,
                         WideString   & result,
                         WideString   & pending)
{
    result = utf8_mbstowcs (str);
    m_pending = WideString ();

    return false;
}

void
NicolaConvertor::clear (void)
{
    m_pending = WideString ();
}

bool
NicolaConvertor::is_pending (void)
{
    return !m_pending.empty ();
}

WideString
NicolaConvertor::get_pending (void)
{
    return m_pending;
}

WideString
NicolaConvertor::flush_pending (void)
{
    return WideString ();
}

void
NicolaConvertor::reset_pending (const WideString & result, const String & raw)
{
    VoicedConsonantRule *table = scim_anthy_voiced_consonant_table;

    m_pending = WideString ();

    for (unsigned int i = 0; table[i].string; i++) {
        if (result == utf8_mbstowcs (table[i].string)) {
            m_pending = result;
            return;
        }
    }
}
/*
vi:ts=4:nowrap:ai:expandtab
*/
