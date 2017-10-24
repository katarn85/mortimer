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

#ifndef __SCIM_ANTHY_KANA_H__
#define __SCIM_ANTHY_KANA_H__

#define Uses_SCIM_ICONV
#define Uses_SCIM_EVENT
#include <scim.h>

#include "scim_anthy_key2kana_base.h"
#include "scim_anthy_default_tables.h"
#include "scim_anthy_key2kana_table.h"

using namespace scim;

class AnthyInstance;

namespace scim_anthy {

class KanaConvertor : public Key2KanaConvertorBase
{
public:
               KanaConvertor      (AnthyInstance    & anthy);
    virtual   ~KanaConvertor      ();

    bool       can_append         (const KeyEvent   & key,
                                   bool               ignore_space = false);
    bool       append             (const KeyEvent   & key,
                                   WideString       & result,
                                   WideString       & pending,
                                   String           & raw);
    bool       append             (const String     & raw,
                                   WideString       & result,
                                   WideString       & pending);
    void       clear              (void);

    bool       is_pending         (void);
    WideString get_pending        (void);
    WideString flush_pending      (void);
    void       reset_pending      (const WideString & result,
                                   const String     & raw);

private:
    AnthyInstance &m_anthy;

    // state
    String         m_pending;
};

}
#endif /* __SCIM_ANTHY_KANA_H__ */
/*
vi:ts=4:nowrap:ai:expandtab
*/
