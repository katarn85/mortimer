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

#ifndef __SCIM_ANTHY_NICOLA_H__
#define __SCIM_ANTHY_NICOLA_H__

#define Uses_SCIM_ICONV
#define Uses_SCIM_EVENT
#include <scim.h>
#include <sys/time.h>

#include "scim_anthy_key2kana_base.h"
#include "scim_anthy_key2kana_table.h"

using namespace scim;

class AnthyInstance;

namespace scim_anthy {

typedef enum {
    SCIM_ANTHY_NICOLA_SHIFT_NONE,
    SCIM_ANTHY_NICOLA_SHIFT_LEFT,
    SCIM_ANTHY_NICOLA_SHIFT_RIGHT,
} NicolaShiftType;

class NicolaConvertor : public Key2KanaConvertorBase
{
public:
    NicolaConvertor                     (AnthyInstance    & anthy,
                                         Key2KanaTableSet & tables);
    virtual ~NicolaConvertor            ();

    bool       can_append               (const KeyEvent   & key,
                                         bool               ignore_space = false);
    bool       append                   (const KeyEvent   & key,
                                         WideString       & result,
                                         WideString       & pending,
                                         String           & raw);
    bool       append                   (const String     & raw,
                                         WideString       & result,
                                         WideString       & pending);
    void       clear                    (void);

    bool       is_pending               (void);
    WideString get_pending              (void);
    WideString flush_pending            (void);
    void       reset_pending            (const WideString & result,
                                         const String     & raw);

public:
    void       process_timeout          (void);

private:
    void       search                   (const KeyEvent     key,
                                         NicolaShiftType    shift_type,
                                         WideString       & result,
                                         String           & raw);
    bool       handle_voiced_consonant  (WideString       & result,
                                         WideString       & pending);
    bool       is_char_key              (const KeyEvent     key);
    bool       is_thumb_key             (const KeyEvent     key);
    bool       is_left_thumb_key        (const KeyEvent     key);
    bool       is_right_thumb_key       (const KeyEvent     key);
    NicolaShiftType
               get_thumb_key_type       (const KeyEvent     key);

    void       on_key_repeat            (const KeyEvent     key,
                                         WideString       & result,
                                         String           & raw);
    void       on_both_key_pressed      (const KeyEvent     key,
                                         WideString       & result,
                                         String           & raw);
    void       on_thumb_key_pressed     (const KeyEvent     key,
                                         WideString       & result,
                                         String           & raw);
    void       on_char_key_pressed      (const KeyEvent     key,
                                         WideString       & result,
                                         String           & raw);
    void       on_no_key_pressed        (const KeyEvent     key);

    bool       is_repeating             (void);
    void       emit_key_event           (const KeyEvent   & key);
    void       set_alarm                (int                time_msec);

private:
    Key2KanaTableSet &m_tables;

    AnthyInstance    &m_anthy;

    // state
    KeyEvent          m_prev_char_key;
    KeyEvent          m_prev_thumb_key;

    KeyEvent          m_repeat_char_key;
    KeyEvent          m_repeat_thumb_key;
    bool              m_is_repeating;

    struct timeval    m_time_char;
    struct timeval    m_time_thumb;

    KeyEvent          m_through_key_event;

    uint32            m_timer_id;
    bool              m_processing_timeout;

    WideString        m_pending;
};

}
#endif /* __SCIM_ANTHY_NICOLA_H__ */
/*
vi:ts=4:nowrap:ai:expandtab
*/
