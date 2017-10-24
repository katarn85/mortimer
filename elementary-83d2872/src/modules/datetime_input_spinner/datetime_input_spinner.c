#ifdef HAVE_CONFIG_H
#include "elementary_config.h"
#endif

#include <Elementary.h>
#include "elm_priv.h"

#define DATETIME_FIELD_COUNT    6
#define FIELD_FORMAT_LEN        3
#define BUFF_SIZE               100

typedef struct _Input_Spinner_Module_Data Input_Spinner_Module_Data;

struct _Input_Spinner_Module_Data
{
   Elm_Datetime_Module_Data mod_data;
};

static void
_field_value_set(struct tm *tim, Elm_Datetime_Field_Type  field_type, int val)
{
   if (field_type >= DATETIME_FIELD_COUNT - 1) return;

   int *timearr[]= { &tim->tm_year, &tim->tm_mon, &tim->tm_mday, &tim->tm_hour, &tim->tm_min };
   *timearr[field_type] = val;
}

static void
_spinner_changed_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Input_Spinner_Module_Data *layout_mod;
   Elm_Datetime_Field_Type  field_type;
   struct tm tim;
   int value ;

   layout_mod = (Input_Spinner_Module_Data *)data;

   elm_datetime_value_get(layout_mod->mod_data.base, &tim);
   field_type = (Elm_Datetime_Field_Type )evas_object_data_get(obj, "_field_type");
   value =  elm_spinner_value_get(obj);

   if (field_type == ELM_DATETIME_YEAR)
     {
       value -=  1900;
     }
   else if (field_type == ELM_DATETIME_MONTH)
     {
        value -= 1;
     }

   _field_value_set(&tim, field_type, value);

   elm_datetime_value_set(layout_mod->mod_data.base, &tim);
}
static void
_spinner_entry_changed_cb(void *data, Evas_Object *obj, void *event_info)
{

   char *str;
   int len, max_len;
   double min, max;
   Eina_List *fields = NULL, *l;

   str = event_info;
   len = strlen(str);
   elm_spinner_min_max_get(obj, &min, &max);
   max_len = log10(abs(max)) + 1;

   if (max_len == len)
     {
        Input_Spinner_Module_Data *layout_mod = data;
        fields = layout_mod->mod_data.fields_sorted_get(layout_mod->mod_data.base);

        Evas_Object *spinner_next = NULL;
        l = eina_list_data_find_list(fields, obj);
        if (l)
          {
             while (eina_list_next(l))
               {
                  spinner_next = eina_list_data_get(eina_list_next(l));
                  if (spinner_next && !evas_object_visible_get(spinner_next))
                    {
                       l = eina_list_next(l);
                       spinner_next = NULL;
                    }
                  else
                    break;
               }
          }

        if (spinner_next)
          {
             elm_layout_signal_emit(obj, "elm,action,entry,toggle", "elm");
             edje_object_message_signal_process(elm_layout_edje_get(obj));
             elm_layout_signal_emit(spinner_next, "elm,action,entry,toggle", "elm");
          }
        //list_free(fields);
     }
}

static void
_ampm_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Input_Spinner_Module_Data *layout_mod;
   struct tm curr_time;

   layout_mod = (Input_Spinner_Module_Data *)data;
   if (!layout_mod) return;

   elm_datetime_value_get(layout_mod->mod_data.base, &curr_time);
   if (curr_time.tm_hour >= 12) curr_time.tm_hour -= 12;
   else curr_time.tm_hour += 12;
   elm_datetime_value_set(layout_mod->mod_data.base, &curr_time);
}

static void
_access_set(Evas_Object *obj, Elm_Datetime_Field_Type field_type)
{
   const char* type = NULL;

   switch (field_type)
     {
       case ELM_DATETIME_YEAR:
        type = "datetime field, year";
        break;

      case ELM_DATETIME_MONTH:
        type = "datetime field, month";
        break;

     case ELM_DATETIME_DATE:
        type = "datetime field, date";
        break;

      case ELM_DATETIME_HOUR:
        type = "datetime field, hour";
        break;

      case ELM_DATETIME_MINUTE:
        type = "datetime field, minute";
        break;

      case ELM_DATETIME_AMPM:
        type = "datetime field, AM PM";
        break;

      default:
        break;
     }

   // _elm_access_text_set
   //   (_elm_access_info_get(obj), ELM_ACCESS_TYPE, type);
   // _elm_access_callback_set
   //   (_elm_access_info_get(obj), ELM_ACCESS_STATE, NULL, NULL);
}

// module fucns for the specific module type
EAPI void
field_format_changed(Elm_Datetime_Module_Data *module_data, Evas_Object *obj)
{
   Input_Spinner_Module_Data *layout_mod;
   Elm_Datetime_Field_Type  field_type;
   int min, max;
   struct tm tim = {0, };
   char buf[BUFF_SIZE] = {0};
   const char *fmt;

   layout_mod = (Input_Spinner_Module_Data *)module_data;
   if (!layout_mod || !obj) return;

   field_type = (Elm_Datetime_Field_Type )evas_object_data_get(obj, "_field_type");

   if ((field_type == ELM_DATETIME_MONTH) || (field_type == ELM_DATETIME_HOUR))
     {
        fmt = layout_mod->mod_data.field_format_get(layout_mod->mod_data.base, field_type);
        layout_mod->mod_data.field_limit_get(layout_mod->mod_data.base, field_type, &min, &max);
        for (int i = min ; i <= max; i++)
          {
            if (field_type == ELM_DATETIME_MONTH)
              tim.tm_mon = i;
            else
              tim.tm_hour = i;

            strftime(buf, sizeof(buf), fmt, &tim);

            if (field_type == ELM_DATETIME_MONTH)
              elm_spinner_special_value_add(obj, i + 1, buf);
            else
              elm_spinner_special_value_add(obj, i, buf);
          }
     }
}

EAPI void
field_value_display(Elm_Datetime_Module_Data *module_data, Evas_Object *obj)
{
   Input_Spinner_Module_Data *layout_mod;
   Elm_Datetime_Field_Type  field_type;
   int min, max, value;
   struct tm tim;
   char buf[BUFF_SIZE] = {0};
   const char *fmt;

   layout_mod = (Input_Spinner_Module_Data *)module_data;
   if (!layout_mod || !obj) return;

   elm_datetime_value_get(layout_mod->mod_data.base, &tim);

   field_type = (Elm_Datetime_Field_Type )evas_object_data_get(obj, "_field_type");
   fmt = layout_mod->mod_data.field_format_get(layout_mod->mod_data.base, field_type);
   strftime(buf, sizeof(buf), fmt, &tim);

   if(field_type == ELM_DATETIME_AMPM)
     {
        if ((tim.tm_hour >= 0) && (tim.tm_hour < 12))
          elm_object_text_set(obj, "AM");
        else
          elm_object_text_set(obj, "PM");
     }
   else if (field_type == ELM_DATETIME_MONTH)
     {
        layout_mod->mod_data.field_limit_get(layout_mod->mod_data.base, field_type, &min, &max);
        elm_spinner_min_max_set(obj, 1 + min, 1 + max);
        elm_spinner_value_set(obj, 1 + tim.tm_mon);
     }
   else if (field_type == ELM_DATETIME_YEAR)
     {
        layout_mod->mod_data.field_limit_get(layout_mod->mod_data.base, field_type, &min, &max);
        elm_spinner_min_max_set(obj, 1900 + min, 1900 + max);
        elm_spinner_value_set(obj, 1900 + tim.tm_year);
     }
   else if (field_type == ELM_DATETIME_HOUR)
     {
        layout_mod->mod_data.field_limit_get(layout_mod->mod_data.base, field_type, &min, &max);
        elm_spinner_min_max_set(obj, min, max);
        elm_spinner_value_set(obj, tim.tm_hour);
     }
   else
     {
        layout_mod->mod_data.field_limit_get(layout_mod->mod_data.base, field_type, &min, &max);
        elm_spinner_min_max_set(obj, min, max);
        value = atoi(buf);
        elm_spinner_value_set(obj, value);
     }

}

EAPI Evas_Object *
field_create(Elm_Datetime_Module_Data *module_data, Elm_Datetime_Field_Type  field_type)
{
   Input_Spinner_Module_Data *layout_mod;
   Evas_Object *field_obj;
   layout_mod = (Input_Spinner_Module_Data *)module_data;
   if (!layout_mod) return NULL;

   if (field_type == ELM_DATETIME_AMPM)
     {
        field_obj = elm_button_add(layout_mod->mod_data.base);
        elm_object_style_set(field_obj, "spinner/datetime");
        evas_object_smart_callback_add(field_obj, "clicked", _ampm_clicked_cb, layout_mod);
     }
   else
     {
       field_obj = elm_spinner_add(layout_mod->mod_data.base);
       elm_object_style_set(field_obj, "datetime");
       elm_spinner_editable_set(field_obj, EINA_TRUE);
       elm_spinner_step_set(field_obj, 1);
       elm_spinner_wrap_set(field_obj, EINA_TRUE);
       elm_spinner_label_format_set(field_obj, "%02.0f");
       if (field_type == ELM_DATETIME_HOUR || field_type == ELM_DATETIME_MONTH)
         elm_spinner_interval_set(field_obj, 0.2);
       else
         elm_spinner_interval_set(field_obj, 0.1);
       evas_object_smart_callback_add(field_obj, "changed", _spinner_changed_cb, layout_mod);
       evas_object_smart_callback_add(field_obj, "entry,changed", _spinner_entry_changed_cb, layout_mod);
     }
   evas_object_data_set(field_obj, "_field_type", (void *)field_type);

   // ACCESS
   _access_set(field_obj, field_type);

   return field_obj;
}

EAPI Elm_Datetime_Module_Data *
obj_hook(Evas_Object *obj)
{
   Input_Spinner_Module_Data *layout_mod;
   layout_mod = ELM_NEW(Input_Spinner_Module_Data);
   if (!layout_mod) return NULL;

   return ((Elm_Datetime_Module_Data*)layout_mod);
}

EAPI void
obj_unhook(Elm_Datetime_Module_Data *module_data)
{
   Input_Spinner_Module_Data *layout_mod;

   layout_mod = (Input_Spinner_Module_Data *)module_data;
   if (!layout_mod) return;

   if (layout_mod)
     {
          free(layout_mod);
          layout_mod = NULL;
      }
}

EAPI void
obj_hide(Elm_Datetime_Module_Data *module_data)
{
  return;
}

// module api funcs needed
EAPI int
elm_modapi_init(void *m EINA_UNUSED)
{
   return 1; // succeed always
}

EAPI int
elm_modapi_shutdown(void *m EINA_UNUSED)
{
   return 1; // succeed always
}
