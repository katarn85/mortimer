if USE_GST_V4L
V4L_DIR=v4l
else
V4L_DIR=
endif

if USE_X
XIMAGE_DIR=ximage
else
XIMAGE_DIR=
endif

if USE_XVIDEO
XVIMAGE_DIR=xvimage
else
XVIMAGE_DIR=
endif

if USE_AUDIODATASPLITTER
AUDIODATASPLITTER_DIR=audiodatasplitter
else
AUDIODATASPLITTER_DIR=
endif

if USE_FRAMECOLLECTOR
FRAMECOLLECTOR_DIR=framecollector
else
FRAMECOLLECTOR_DIR=
endif

if USE_SPDIFSINK
SPDIFSINK_DIR=spdifsink
else
SPDIFSINK_DIR=
endif

if USE_MIXERSINK
MIXERSINK_DIR=mixersink
else
MIXERSINK_DIR=
endif

if USE_PLAYERMIXER
PLAYERMIXER_DIR=playermixer
else
PLAYERMIXER_DIR=
endif

SUBDIRS = \
  $(PLAYERMIXER_DIR) \
  $(XIMAGE_DIR) \
  $(XVIMAGE_DIR) \
  $(V4L_DIR) \
  $(FRAMECOLLECTOR_DIR) \
  $(AUDIODATASPLITTER_DIR) \
  $(SPDIFSINK_DIR) \
  $(MIXERSINK_DIR) 

DIST_SUBDIRS = \
  playermixer \
  v4l \
  ximage \
  xvimage \
  framecollector \
  audiodatasplitter \
  spdifsink \
  mixersink

#include $(top_srcdir)/common/parallel-subdirs.mak

