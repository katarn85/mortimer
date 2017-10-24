/*
 * uibcdefinition
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, ByungWook Jang <bw.jang@samsung.com>,
 * Manoj Kumar K <manojkumar.k@samsung.com>, Abhishek Bajaj <abhi.bajaj@samsung.com>, Nikhilesh Mittal <nikhilesh.m@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef __UIBC_DEFINITION__
#define __UIBC_DEFINITION__

#include <glib.h>

typedef enum {
  UIBC_OK     = 0,
  UIBC_EINVAL = -1
} WFDUIBCResult;

typedef enum {
  UIBC_GENERIC = 0,
  UIBC_HIDC
} WFDUIBCInputCategory;
/* Generic Input Type IDs for User inputs of the generic Category. (Ref. Table 4-5) */
typedef enum {
  UIBC_GENERIC_ID_TOUCH_DOWN = 0,
  UIBC_GENERIC_ID_TOUCH_UP,
  UIBC_GENERIC_ID_TOUCH_MOVE,
  UIBC_GENERIC_ID_KEY_DOWN,
  UIBC_GENERIC_ID_KEY_UP,
  UIBC_GENERIC_ID_ZOOM,
  UIBC_GENERIC_ID_SCROLL_VERTICAL,
  UIBC_GENERIC_ID_SCROLL_HORIZONTAL,
  UIBC_GENERIC_ID_ROTATE
}WFDUIBCGenInputTypeID;

typedef enum {
  UIBC_PATH_INFRARED = 0,
  UIBC_PATH_USB,
  UIBC_PATH_BT,
  UIBC_PATH_WI_FI,
  UIBC_PATH_ZIGBEE,
  UIBC_PATH_NO_SP
}WFDUIBCInputPathID;

/* Generic Input message Touch Event (Ref. Table 4-6, 4-7 & 4-8) */
typedef struct {
  guint8 pointer_ID;
  guint16 x_coordinate;
  guint16 y_coordinate;
} WFDUIBCTouchPoint;

typedef struct {
  guint8 num_pointers;
  WFDUIBCTouchPoint *pointers_list;
} WFDUIBCTouchEvent;

/* Generic Input Message Key Event (Ref. Table 4-9 & 4-10) */
typedef struct {
  /* ASCII value of key. */
  guint16 key_code_1;
  guint16 key_code_2;
} WFDUIBCKeyEvent;

/* Generic Input Message Zoom Event (Ref. Table 4-11) */
typedef struct {
  guint16 x_coordinate;
  guint16 y_coordinate;
  guint8 zoom_int_val;
  guint8 zoom_fraction_val;
} WFDUIBCZoomEvent;

/* Generic Input Message Scroll Event (Ref. Table 4-12 & 4-13) */
typedef struct {
  /* Scroll Unit Indication bits.
   * 0b00 unit is a pixel.
   * 0b01 unit is a mouse notch.
   * 0b10 & 0b11 reserved.*/
  guint8 unit_indicator;
  /* Scroll Direction Indicator.
   * 0b0 Scrolling Down/Right.
   * 0b1 Scrolling UP/Left.*/
  gboolean direction;
  /* 12 lower bits for number of scroll units. */
  guint16 num_units;
} WFDUIBCScrollEvent;

/* Generic Input Message Rotate Event (Ref. Table 4-14) */
typedef struct {
  /* positive means counter-clockwise */
  gint8 rotate_int_val;
  guint8 rotate_frac_val;
} WFDUIBCRotateEvent;

/* Generic Input Message Format. (Ref. Table 4-4)*/
typedef struct {
  WFDUIBCGenInputTypeID input_type_ID;
  guint32 length;
  union {
    WFDUIBCTouchEvent *touch_event;
    WFDUIBCKeyEvent *key_event;
    WFDUIBCZoomEvent *zoom_event;
    WFDUIBCScrollEvent *scroll_event;
    WFDUIBCRotateEvent *rotate_event;
  };
} WFDUIBCGenInputBody;

typedef struct {
  WFDUIBCGenInputTypeID input_type_ID;
  WFDUIBCInputPathID input_path_ID;
  guint32 length;
  guint8 type;
  guint8 code;
  guint8 value;
} WFDUIBCHidcInputBody;

/* UIBC message header. (Ref. Fig.4-9) */
typedef struct {
  gboolean is_timestamp;
  guint16 timestamp;
  guint8 input_cat;
  guint16 length;
  WFDUIBCGenInputBody *gen_input_body;
  WFDUIBCHidcInputBody *hidc_input_body;
} WFDUIBCMessage;

/* Val parameter = 1 indicates start sending events and 0 stop sending events */
typedef void (*wfd_uibc_state_cb)(int val, void *hidc_cap_list, guint hidc_cap_count, int neg_width, int neg_height, void *user_param);
typedef void (*wfd_uibc_control_cb)(int cmd, void *hidc_cap_list, guint hidc_cap_count, int neg_width, int neg_height, void **user_param);
typedef void (*wfd_uibc_send_event_cb)(WFDUIBCMessage *event, void *user_param);

#endif /*__UIBC_DEFINITION__*/
