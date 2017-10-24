#include <Elementary.h>
#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif
#include <math.h>
#include <Ecore_Ipc.h>
#include <Ecore_X.h>
#include <string.h>
#include <Eina.h>
#define ELM_INTERNAL_API_ARGESFSDFEFC
#include <elm_widget.h>
#define EDJE_EDIT_IS_UNSTABLE_AND_I_KNOW_ABOUT_IT
#include "Edje_Edit.h"

typedef enum
{
   EA_UTIL_IPC_MESSAGE_WIDGET_TREE_DUMP_REQUEST,
   EA_UTIL_IPC_MESSAGE_WIDGET_TREE_DUMP_REPLY,

   EA_UTIL_IPC_MESSAGE_OBJECT_TREE_DUMP_REQUEST,
   EA_UTIL_IPC_MESSAGE_OBJECT_TREE_DUMP_REPLY,

   EA_UTIL_IPC_MESSAGE_EVENT_PROFILE_REQUEST,
   EA_UTIL_IPC_MESSAGE_EVENT_PROFILE_REPLY,

   EA_UTIL_IPC_MESSAGE_MEMORY_PROFILE_REQUEST,
   EA_UTIL_IPC_MESSAGE_MEMORY_PROFILE_REPLY

} Ea_Utility_IPC_Message_Type;



typedef struct _Ea_Util_Mgr
{
   Evas *evas;
   FILE *fp;

   Eina_List *edje_part_name_list;
   int x,y,w,h;

} Ea_Util_Mgr;

typedef struct _Edje_Info
{
   const Evas_Object *obj;

   char part_name[256];
   const char *image_name;
   const char *color_class;

} Edje_Info;


#define FILE_LOG_PATH   "/opt/home/app/efl_util_debug.log"
static const char ELM_WIDGET_SMART_NAME[] = "elm_widget";

static Ecore_Ipc_Server *ipc_server = NULL;
static int _utility_count = 0, _utility_ipc_count = 0;
static Ecore_Event_Handler *ecore_ipc_event_handlers[10];


#define SPANS_COMMON(x1, w1, x2, w2) \
   (!(( (int)((x2) + (int)(w2)) <= (int)(x1)) || (int)((x2) >= (int)((x1) + (int)(w1)))))

#define RECTS_INTERSECT(x, y, w, h, xx, yy, ww, hh) \
   ((SPANS_COMMON((x), (w), (xx), (ww))) && (SPANS_COMMON((y), (h), (yy), (hh))))


static Eina_Bool
evas_object_is_visible_get(Evas_Object *obj)
{
   int r, g, b, a;
   Evas_Coord x, y, w, h;
   Evas_Object *clip;
   int vis = 0;

   evas_object_color_get(obj, &r, &g, &b, &a);
   evas_object_geometry_get(obj, &x, &y, &w, &h);

   if (evas_object_visible_get(obj)) vis = 1;
   clip = evas_object_clip_get(obj);
   if (clip) evas_object_color_get(obj, &r, &g, &b, &a);
   if (clip && !evas_object_visible_get(clip)) vis = 0;

   if (clip && a == 0) vis = 0;

   return vis;
}

static Eina_Bool
evas_object_type_match(const Evas_Object *obj, const char *type)
{
   int ret;
   ret = strcmp(evas_object_type_get(obj), type);

   if (!ret) return EINA_TRUE;
   return EINA_FALSE;
}

static Eina_Bool
evas_object_is_swallow_rect(const Evas_Object *obj)
{
   int r, g, b, a;

   evas_object_color_get(obj, &r, &g, &b, &a);
   if (!r && !g && !b && !a
       && evas_object_pointer_mode_get(obj) == EVAS_OBJECT_POINTER_MODE_NOGRAB
       && evas_object_pass_events_get(obj))
     return EINA_TRUE;

   return EINA_FALSE;
}

static void
_extract_edje_file_name(Ea_Util_Mgr *util_mgr, const char *full_path)
{
   char name[255];
   int i, cnt = 0;

   if (!full_path) return;

   for (i = 0; i < strlen(full_path); i++)
     {
        if (full_path[i] == '/')
          {
             cnt = 0;
             continue;
          }
        name[cnt++] = full_path[i];
     }
   name[cnt] = '\0';
   fprintf(util_mgr->fp,"[%s]", name);
}

static void
_edje_file_info_save(Ea_Util_Mgr *util_mgr, const Evas_Object *obj)
{
   Evas_Object *ed;
   const Evas_Object *pobj;
   Eina_List *parts, *l;
   Edje_Info *edje_info;
   const char *file, *group, *pname, *ret;
   double val;

   if(!evas_object_type_match(obj, "edje")) return;

   edje_object_file_get(obj, &file, &group);

   ed = edje_edit_object_add(util_mgr->evas); // evas new ??
   if (edje_object_file_set(ed, file, group))
     {
        parts = edje_edit_parts_list_get(ed);
        EINA_LIST_FOREACH(parts, l, pname)
          {
             if ((pobj = edje_object_part_object_get(obj, pname)))
               {
                  edje_info = malloc(sizeof(Edje_Info));
                  edje_info->obj = pobj;
                  strcpy(edje_info->part_name, pname);
                  ret = edje_object_part_state_get(obj, edje_info->part_name, &val);

                  if (ret)
                    {
                       edje_info->color_class = edje_edit_state_color_class_get(ed, edje_info->part_name , ret, val);
                       edje_info->image_name = edje_edit_state_image_get(ed, edje_info->part_name , ret, val);
                    }

                  util_mgr->edje_part_name_list = eina_list_append(util_mgr->edje_part_name_list, edje_info);

               }
          }
        edje_edit_string_list_free(parts);
     }
   evas_object_del(ed);
}

static void
_obj_tree_items(Ea_Util_Mgr *util_mgr, Evas_Object *obj, int lvl)
{
   Eina_List *children, *l;
   Evas_Object *child, *smart_parent_obj;
   Evas_Coord x, y, w, h;
   Eina_Bool is_clip = EINA_FALSE;
   int i, r, g, b, a;
   static int num = 0;
   const char *ret;
   double val;
   const char *file, *key, *group, *text;
   Edje_Info *edje_info;

   // visible check
   if (!evas_object_is_visible_get(obj)) return;

   // viewport check
   evas_object_geometry_get(obj, &x, &y, &w, &h);

   if (!RECTS_INTERSECT(x,y,w,h, util_mgr->x, util_mgr->y, util_mgr->w, util_mgr->h)) return;

   // clipper check
   if (evas_object_clipees_get(obj)) is_clip = EINA_TRUE;
   if (is_clip) goto next;

   //   if (evas_object_smart_type_check_ptr(obj, ELM_WIDGET_SMART_NAME))
   if(elm_widget_is(obj)) fprintf(util_mgr->fp,"\n\033[36m");
   if (evas_object_type_match(obj, "edje")) fprintf(util_mgr->fp,"\033[33;1m");

   fprintf(util_mgr->fp,"%3d : %-10p [%4d %4d %4d %4d]", ++num, obj, x, y, w, h);

   for (i = 0; i < lvl * 2; i++)
     fprintf(util_mgr->fp, " ");

   if (evas_object_is_visible_get(obj)) fprintf(util_mgr->fp,"+ ");
   else fprintf(util_mgr->fp,"- ");

   // evas object type check
   if (evas_object_is_swallow_rect(obj)) fprintf(util_mgr->fp,"[%d] swallow ", lvl);
   else if (evas_object_type_match(obj, "rectangle"))
     {
        evas_object_color_get(obj, &r, &g, &b, &a);
        fprintf(util_mgr->fp,"[%d] rect [%02X %02X %02X %02X] ", lvl, r, g, b, a);
     }
   else fprintf(util_mgr->fp,"[%d] %s ", lvl, evas_object_type_get(obj));

   smart_parent_obj = evas_object_smart_parent_get(obj);

   // image info save
   if (!strcmp(evas_object_type_get(obj), "elm_icon") ||
       !strcmp(evas_object_type_get(obj), "elm_image"))
     {
        elm_image_file_get(obj, &file, &key);
        evas_object_data_set(obj, "image_name", file);
     }

   // image name check
   if (smart_parent_obj && evas_object_type_match(obj, "image")
       && (evas_object_type_match(smart_parent_obj, "elm_icon")
           || evas_object_type_match(smart_parent_obj, "elm_image")))
     {
        if (ret = evas_object_data_get(smart_parent_obj, "image_name"))
          {
             _extract_edje_file_name(util_mgr, ret);
             evas_object_data_del(smart_parent_obj, "edje_image_name");
          }
     }

   // edje info save
   if(evas_object_type_match(obj, "edje"))
     {
        edje_object_file_get(obj, &file, &group);
        if (group) fprintf(util_mgr->fp,"[%s] ", group);

        _extract_edje_file_name(util_mgr, file);
        _edje_file_info_save(util_mgr, obj);
     }

   // edje info check
   if( !is_clip && smart_parent_obj
       && !elm_widget_is(obj) && evas_object_type_match(smart_parent_obj, "edje"))
     {
        EINA_LIST_FOREACH(util_mgr->edje_part_name_list, l, edje_info)
          {
             if(edje_info->obj == obj)
               {
                  if (edje_info->color_class)
                    fprintf(util_mgr->fp,"[%s] ", edje_info->color_class);

                  ret = edje_object_part_state_get(evas_object_smart_parent_get(obj), edje_info->part_name, &val);
                  fprintf(util_mgr->fp,"[%s : \"%s\" %2.1f]  ", edje_info->part_name , ret, val);

                  if (edje_info->image_name)
                    fprintf(util_mgr->fp,"[%s] ", edje_info->image_name);
                  break;
               }
          }
     }

   text = NULL;
   if (!strcmp(evas_object_type_get(obj), "text"))
     text = eina_stringshare_add(evas_object_text_text_get(obj));
   else if (!strcmp(evas_object_type_get(obj), "textblock"))
     {
        ret = evas_object_textblock_text_markup_get(obj);
        text = eina_stringshare_add(evas_textblock_text_markup_to_utf8(obj, ret));
     }
   if (text && strlen(text) > 0)
     {
        fprintf(util_mgr->fp,"\n\t\t\t\t\t     ");
        fprintf(util_mgr->fp,"\033[32;1m");

        for (i = 0; i < lvl * 2; i++)
          fprintf(util_mgr->fp," ");

        fprintf(util_mgr->fp,"==> [%s]", text);
     }


   fprintf(util_mgr->fp,"\n\033[37;1m");
next:
   children = evas_object_smart_members_get(obj);
   EINA_LIST_FREE(children, child)
      _obj_tree_items(util_mgr, child, lvl + 1);
}


static void
_object_tree(Ea_Util_Mgr *util_mgr)
{
   Eina_List *objs;
   Evas_Object *obj;
   int x, y, w, h;

   objs = evas_objects_in_rectangle_get(util_mgr->evas, SHRT_MIN, SHRT_MIN, USHRT_MAX, USHRT_MAX, EINA_TRUE, EINA_TRUE);

   ecore_evas_geometry_get(ecore_evas_ecore_evas_get(util_mgr->evas), &x, &y, &w, &h);

   fprintf(util_mgr->fp,"\n---------------------------------  Object Tree Info  ----------------------------------------- \n");
   fprintf(util_mgr->fp, "\033[33;1m Evas : %p [%d %d %d %d] \n\n", util_mgr->evas, x, y, w, h);
   fprintf(util_mgr->fp,"\033[37;1m No : Address    [   x    y    w    h] ");
   fprintf(util_mgr->fp,"\n-----------------------------------------------------------------------------------------------\n");

   EINA_LIST_FREE(objs, obj)
      _obj_tree_items(util_mgr, obj, 0);

   fprintf(util_mgr->fp,"\n-----------------------------------------------------------------------------------------------\n");
}

static void
_sub_widget_tree(Ea_Util_Mgr *util_mgr, const Evas_Object *obj, int lvl)
{
   Evas_Coord x, y, w, h;
   int i;

   evas_object_geometry_get(obj, &x, &y, &w, &h);

   if (elm_widget_is(obj))
     fprintf(util_mgr->fp,"%-10p [%4d %4d %4d %4d] ",
             obj, x, y, w, h);

   if (elm_widget_is(obj))
     {
        for (i = 0; i < lvl * 3; i++)
          fprintf(util_mgr->fp," ");
     }

   if (elm_widget_is(obj))
     {
        const Eina_List *l, *list;

        if (elm_object_focus_get(obj))
          fprintf(util_mgr->fp,"\033[33;1m [f] ");
        else
          fprintf(util_mgr->fp,"    ");

        if (evas_object_visible_get(obj))
          fprintf(util_mgr->fp,"+ %s \n", evas_object_type_get(obj));
        else
          fprintf(util_mgr->fp,"- %s \n", evas_object_type_get(obj));

        list = elm_widget_sub_object_list_get(obj);

        fprintf(util_mgr->fp,"\033[37;1m");

        EINA_LIST_FOREACH(list, l, obj)
          {
             _sub_widget_tree(util_mgr, obj, lvl + 1);
          }
     }
}

static void
_widget_tree(Ea_Util_Mgr *util_mgr)
{
   fprintf(util_mgr->fp,"\n-------------------------- Widget Tree Info ------------------------------- \n");
   fprintf(util_mgr->fp,"           [   x    y    w    h]  [Focus]  Widget \n");
   fprintf(util_mgr->fp,"--------------------------------------------------------------------------- \n");

   Evas_Object *top = evas_object_top_get(util_mgr->evas);
   _sub_widget_tree(util_mgr, top, 0);
}


static void _utility_tree_process(Ea_Utility_IPC_Message_Type msg)
{
   Eina_List *ecore_evas_list;
   Ecore_Evas *ee;
   Evas *evas;

   Ea_Util_Mgr *util_mgr = calloc(1, sizeof(Ea_Util_Mgr));
   if (!util_mgr)
     {
        printf("Fail : utility manager memory alloc\n");
        goto exit;
     }
   util_mgr->fp = fopen(FILE_LOG_PATH, "wr");
   if (!util_mgr->fp)
     {
        printf("Fail : %s file open fail \n", FILE_LOG_PATH);
        goto exit;
     }

   ecore_evas_list = ecore_evas_ecore_evas_list_get();

   EINA_LIST_FREE(ecore_evas_list, ee)
     {
        evas = ecore_evas_get(ee);
        util_mgr->evas = evas;

        ecore_x_window_geometry_get(ecore_x_window_root_first_get(), &util_mgr->x, &util_mgr->y, &util_mgr->w, &util_mgr->h);

        if (msg == EA_UTIL_IPC_MESSAGE_WIDGET_TREE_DUMP_REQUEST)
          _widget_tree(util_mgr);
        else if (msg == EA_UTIL_IPC_MESSAGE_OBJECT_TREE_DUMP_REQUEST)
          _object_tree(util_mgr);
     }

exit:
   if (!util_mgr) return;
   if (util_mgr->fp) fclose(util_mgr->fp);
   if (util_mgr->edje_part_name_list)
     eina_list_free(util_mgr->edje_part_name_list);  // need to check
   free(util_mgr);
}

static Eina_Bool
_ipc_client_add_cb(void *data, int type, void *event)
{
   Ecore_Ipc_Server  *server;
   Ecore_Ipc_Event_Client_Data *e;

   server = (Ecore_Ipc_Server *) data;
   e = (Ecore_Ipc_Event_Client_Data *) event;

   if (server != ecore_ipc_client_server_get(e->client))
     return EINA_TRUE;

   printf("EFL DBG Tool is connected @ %p \n", server);
   return EINA_TRUE;
}

static Eina_Bool
_ipc_client_data_cb(void *data, int type, void *event)
{

   Ecore_Ipc_Event_Client_Data *e = event;
   Ea_Utility_IPC_Message_Type *message, reply;

   message = (Ea_Utility_IPC_Message_Type*)e->data;

   if (*message == EA_UTIL_IPC_MESSAGE_WIDGET_TREE_DUMP_REQUEST)
     {
        _utility_tree_process(*message);

        reply = EA_UTIL_IPC_MESSAGE_WIDGET_TREE_DUMP_REPLY;
        ecore_ipc_client_send(e->client, 0, 0, 0, 0, 0, &reply, sizeof(reply));

     }
   else if (*message == EA_UTIL_IPC_MESSAGE_OBJECT_TREE_DUMP_REQUEST)
     {
        _utility_tree_process(*message);

        reply = EA_UTIL_IPC_MESSAGE_OBJECT_TREE_DUMP_REPLY;
        ecore_ipc_client_send(e->client, 0, 0, 0, 0, 0, &reply, sizeof(reply));
     }
   else if (*message == EA_UTIL_IPC_MESSAGE_EVENT_PROFILE_REQUEST)
     {
     }

   return 1;
}

EAPI int
_utilily_ipc_del()
{
   int i;
   if (_utility_ipc_count)
     {
        ecore_ipc_shutdown();

        for(i = 0; i < 10; i++)
          {
             if (ecore_ipc_event_handlers[i])
               ecore_event_handler_del(ecore_ipc_event_handlers[i]);
          }
        if (ipc_server)
          {
             ecore_ipc_server_del(ipc_server);
             ipc_server = NULL;
          }
     }
   _utility_ipc_count = 0;
   return 1;
}

static Eina_Bool
_ipc_client_del_cb(void *data, int type, void *event)
{
   _utilily_ipc_del();
   return ECORE_CALLBACK_PASS_ON;
}

EAPI int
_utilily_ipc_add()
{
   if (_utility_ipc_count) return 0;

   if (ecore_ipc_init() < 1)
     return 0;

   _utility_ipc_count++;

   ipc_server = ecore_ipc_server_add(ECORE_IPC_LOCAL_SYSTEM, "efl_util", 516, NULL);
   if (ipc_server) printf("Server is listening on \n");
   else
     {
        printf("CONNECT FAIL ! \n");
        return 0;
     }

   ecore_ipc_event_handlers[0] = ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_ADD, _ipc_client_add_cb, ipc_server);
   ecore_ipc_event_handlers[1] = ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_DATA, _ipc_client_data_cb, ipc_server);
   ecore_ipc_event_handlers[2] = ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_DEL, _ipc_client_del_cb, ipc_server);

   return 1;
}

static int
_utility_init()
{
   int ret;
   if (_utility_count) return 0;

   if (!(ret = _utilily_ipc_add()))
     {
        printf("_utilily_ipc_add ret = %d \n", ret);
        _utilily_ipc_del();
     }

   _utility_count++;
   return _utility_count;
}

static int
_utility_shutdown()
{
   if (!_utility_count) return _utility_count;

   _utilily_ipc_del();
   _utility_count = 0;

   return _utility_count;
}


EAPI int
elm_modapi_init(void *m __UNUSED__)
{
   _utility_init();

   return 1;
}

EAPI int
elm_modapi_shutdown(void *m __UNUSED__)
{
   _utility_shutdown();

   return 1;
}
