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

#define Uses_SCIM_HELPER
#define Uses_SCIM_CONFIG_BASE
#include <map>
#include <scim.h>
#include <gtk/gtk.h>
#include "scim_anthy_intl.h"
#include "scim_anthy_helper.h"

using namespace scim;

#define scim_module_init anthy_imengine_helper_LTX_scim_module_init
#define scim_module_exit anthy_imengine_helper_LTX_scim_module_exit
#define scim_helper_module_number_of_helpers anthy_imengine_helper_LTX_scim_helper_module_number_of_helpers
#define scim_helper_module_get_helper_info anthy_imengine_helper_LTX_scim_helper_module_get_helper_info
#define scim_helper_module_run_helper anthy_imengine_helper_LTX_scim_helper_module_run_helper

static gboolean   helper_agent_input_handler  (GIOChannel          *source,
                                               GIOCondition         condition,
                                               gpointer             user_data);
static void       slot_imengine_event         (const HelperAgent   *agent,
                                               int                  ic,
                                               const String        &uuid,
                                               const Transaction   &trans);
static gint       timeout_func                (gpointer             data);
static void       timeout_ctx_destroy_func    (gpointer             data);

static void       run                         (const String        &display,
                                               const ConfigPointer &config);

HelperAgent helper_agent;

HelperInfo helper_info (SCIM_ANTHY_HELPER_UUID,        // uuid
                        "",                            // name
                        "",                            // icon
                        "",
                        SCIM_HELPER_NEED_SCREEN_INFO);

class TimeoutContext {
public:
    TimeoutContext (int ic, const String &uuid, uint32 id)
        : m_ic   (ic),
          m_uuid (uuid),
          m_id   (id)
        {}
    virtual ~TimeoutContext () {}
public:
    int    m_ic;
    String m_uuid;
    uint32 m_id;
};
typedef std::map <uint32, guint> TimeoutIDList;
std::map <int, TimeoutIDList> timeout_ids;


//Module Interface
extern "C" {
    void scim_module_init (void)
    {
        bindtextdomain (GETTEXT_PACKAGE, SCIM_ANTHY_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

        helper_info.name = String (_("Anthy helper"));
        helper_info.description = String (
            _("A helper module for Anthy IMEngine."));
    }

    void scim_module_exit (void)
    {
    }

    unsigned int scim_helper_module_number_of_helpers (void)
    {
        return 1;
    }

    bool scim_helper_module_get_helper_info (unsigned int idx, HelperInfo &info)
    {
        if (idx == 0) {
            info = helper_info; 
            return true;
        }
        return false;
    }

    void scim_helper_module_run_helper (const String &uuid,
                                        const ConfigPointer &config,
                                        const String &display)
    {
        SCIM_DEBUG_MAIN(1) << "anthy_imengine_helper_LTX_scim_helper_module_run_helper ()\n";

        if (uuid == String (SCIM_ANTHY_HELPER_UUID)) {
            run (display, config);
        }

        SCIM_DEBUG_MAIN(1) << "exit anthy_imengine_helper_LTX_scim_helper_module_run_helper ()\n";
    }
}


static gboolean
helper_agent_input_handler (GIOChannel *source,
                            GIOCondition condition,
                            gpointer user_data)
{
    if (condition == G_IO_IN) {
        HelperAgent *agent = static_cast<HelperAgent*> (user_data);
        if (agent && agent->has_pending_event ())
            agent->filter_event ();
    } else if (condition == G_IO_ERR || condition == G_IO_HUP) {
        gtk_main_quit ();
    }
    return TRUE;
}

static void
slot_exit (const HelperAgent *agent, int ic, const String &uuid)
{
    gtk_main_quit ();
}

static void
slot_imengine_event (const HelperAgent *agent, int ic,
                     const String &uuid, const Transaction &recv)
{
    TransactionReader reader (recv);
    int cmd;

    if (!reader.get_command (cmd))
        return;

    switch (cmd) {
    case SCIM_ANTHY_TRANS_CMD_GET_SELECTION:
    {
        GtkClipboard *primary_selection;
        WideString selection;

        primary_selection = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
        if (primary_selection) {
            gchar *text = gtk_clipboard_wait_for_text (primary_selection);
            if (text) {
                selection = utf8_mbstowcs (text);
                g_free (text);
            }
        }

        Transaction send;
        send.put_command (SCIM_ANTHY_TRANS_CMD_GET_SELECTION);
        send.put_data (selection);
        helper_agent.send_imengine_event (ic, uuid, send);

        break;
    }
    case SCIM_ANTHY_TRANS_CMD_TIMEOUT_ADD:
    {
        uint32 id, time_msec;
        if (!reader.get_data (id) || !reader.get_data (time_msec))
            break;
        TimeoutContext *ctx = new TimeoutContext (ic, uuid, id);
        guint timeout_id = gtk_timeout_add_full (time_msec,
                                                 timeout_func,
                                                 NULL,
                                                 (gpointer) ctx,
                                                 timeout_ctx_destroy_func);
        timeout_ids[ic][id] = timeout_id;
        break;
    }
    case SCIM_ANTHY_TRANS_CMD_TIMEOUT_REMOVE:
    {
        uint32 id;
        if (reader.get_data (id) &&
            timeout_ids.find (ic) != timeout_ids.end () &&
            timeout_ids[ic].find (id) != timeout_ids[ic].end ())
        {
            guint tid = timeout_ids[ic][id];
            gtk_timeout_remove (tid);
        }
        break;
    }
    default:
        break;
    }
}

static gint
timeout_func (gpointer data)
{
    TimeoutContext *ctx = static_cast<TimeoutContext*> (data);

    Transaction send;
    send.put_command (SCIM_ANTHY_TRANS_CMD_TIMEOUT_NOTIFY);
    send.put_data (ctx->m_id);
    helper_agent.send_imengine_event (ctx->m_ic, ctx->m_uuid, send);

    return FALSE;
}

static void
timeout_ctx_destroy_func (gpointer data)
{
    TimeoutContext *ctx = static_cast<TimeoutContext*> (data);
    int ic = ctx->m_ic;
    uint32 id = ctx->m_id;

    if (timeout_ids.find (ic) != timeout_ids.end () &&
        timeout_ids[ic].find (id) != timeout_ids[ic].end ())
    {
        timeout_ids[ic].erase (id);
    }
    delete ctx;
}

static void
run (const String &display, const ConfigPointer &config)
{
    char **argv = new char * [4];
    int    argc = 3;

    argv [0] = "anthy-imengine-helper";
    argv [1] = "--display";
    argv [2] = const_cast<char *> (display.c_str ());
    argv [3] = 0;
 
    setenv ("DISPLAY", display.c_str (), 1);

    gtk_init (&argc, &argv);

    helper_agent.signal_connect_exit (slot (slot_exit));
    helper_agent.signal_connect_process_imengine_event (slot (slot_imengine_event));

    // open connection
    int fd = helper_agent.open_connection (helper_info, display);
    GIOChannel *ch = g_io_channel_unix_new (fd);

    if (fd >= 0 && ch) {
        g_io_add_watch (ch, G_IO_IN,
                        helper_agent_input_handler,
                        (gpointer) &helper_agent);
        g_io_add_watch (ch, G_IO_ERR,
                        helper_agent_input_handler,
                        (gpointer) &helper_agent);
        g_io_add_watch (ch, G_IO_HUP,
                        helper_agent_input_handler,
                        (gpointer) &helper_agent);
    }

    gtk_main ();

    // close connection
    helper_agent.close_connection ();
    fd = -1;
}
