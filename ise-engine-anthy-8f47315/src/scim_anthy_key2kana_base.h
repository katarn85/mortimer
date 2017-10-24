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

#ifndef __SCIM_ANTHY_KEY2KANA_BASE_H__
#define __SCIM_ANTHY_KEY2KANA_BASE_H__

#define Uses_SCIM_EVENT
#include <scim.h>

using namespace scim;

namespace scim_anthy {

class Key2KanaConvertorBase
{
public:
    Key2KanaConvertorBase                 ()
        : m_case_sensitive (true)
        {};
    virtual ~Key2KanaConvertorBase        () {};

    virtual bool       can_append         (const KeyEvent   & key,
                                           bool               ignore_space = false) = 0;
    virtual bool       append             (const KeyEvent   & key,
                                           WideString       & result,
                                           WideString       & pending,
                                           String           & raw)       = 0;
    virtual bool       append             (const String     & raw,
                                           WideString       & result,
                                           WideString       & pending)   = 0;
    virtual void       clear              (void)                         = 0;

    virtual bool       is_pending         (void)                         = 0;
    virtual WideString get_pending        (void)                         = 0;
    virtual WideString flush_pending      (void)                         = 0;
    virtual void       reset_pending      (const WideString & result,
                                           const String     & raw)       = 0;

    virtual void       reset_pseudo_ascii_mode       (void)
        {}
    virtual bool       process_pseudo_ascii_mode     (const WideString & wstr)
        { return false; }

    virtual void       set_case_sensitive (bool               sensitive)
        { m_case_sensitive = sensitive; }
    virtual bool       get_case_sensitive (void)
        { return m_case_sensitive; }

protected:
    bool m_case_sensitive;
};

}
#endif /* __SCIM_ANTHY_KEY2KANA_BASE_H__ */
/*
vi:ts=4:nowrap:ai:expandtab
*/
