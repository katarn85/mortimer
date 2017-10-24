/*
 * uibcmessages
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

#ifndef __UIBC_MESSAGES__
#define __UIBC_MESSAGES__

#include "wfdrtspuibcdefs.h"

gchar *
wfd_uibc_message_as_text (WFDUIBCMessage *msg);
/*
 * To print the elements of the message.
*/
WFDUIBCResult
wfd_uibc_message_dump (WFDUIBCMessage *msg);
/*
 * To parse the char string and extract information from the string.
*/
WFDUIBCResult
wfd_uibc_message_parse_buffer (const guint8 * data, guint size, WFDUIBCMessage * msg);

#endif /*__UIBC_MESSAGES__*/
