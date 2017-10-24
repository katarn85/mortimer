const char *default_config =
"\
"
"\
"
"\
"
"\
"
"\
"
"\
"
"omx_dummy,\
"
"  parent-type=GstOmxDummy,\
"
"  type=GstOmxDummyOne,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.bellagio.dummy,\
"
"  rank=0;\
"
"\
"
"\
"
"omx_dummy_2,\
"
"  parent-type=GstOmxDummy,\
"
"  type=GstOmxDummyTwo,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.dummy2,\
"
"  rank=256;\
"
"\
"
"omx_mpeg4dec,\
"
"  type=GstOmxMpeg4Dec,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.video_decoder.mpeg4,\
"
"  sink=\"video/mpeg, mpegversion=(int)4, systemstream=false, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max]; video/x-divx, divxversion=(int)[4,5], width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max]; video/x-xvid, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max]; video/x-3ivx, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  src=\"video/x-raw-yuv, format=(fourcc){I420,YUY2,UYVY,NV12}, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  rank=256;\
"
"\
"
"omx_h264dec,\
"
"  type=GstOmxH264Dec,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.video_decoder.avc,\
"
"  sink=\"video/x-h264, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  src=\"video/x-raw-yuv, format=(fourcc){I420,YUY2,UYVY,NV12}, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  rank=256;\
"
"\
"
"omx_h265dec,\
"
"  type=GstOmxH265Dec,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.SDP.video_decoder.vdsp.hevc,\
"
"  sink=\"video/x-h265, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  src=\"video/x-raw-yuv, format=(fourcc){I420,YUY2,UYVY,NV12}, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  rank=256;\
"
"\
"
"omx_h263dec,\
"
"  type=GstOmxH263Dec,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.video_decoder.h263,\
"
"  sink=\"video/x-h263, variant=(string)itu, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  src=\"video/x-raw-yuv, format=(fourcc){I420,YUY2,UYVY,NV12}, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  rank=256;\
"
"\
"
"omx_wmvdec,\
"
"  type=GstOmxWmvDec,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.video_decoder.wmv,\
"
"  sink=\"video/x-wmv, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  src=\"video/x-raw-yuv, format=(fourcc){I420,YUY2,UYVY,NV12}, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  rank=256;\
"
"\
"
"omx_mpeg4enc,\
"
"  type=GstOmxMpeg4Enc,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.video_encoder.mpeg4,\
"
"  sink=\"video/x-raw-yuv, format=(fourcc){I420,YUY2,UYVY,NV12}, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  src=\"video/mpeg, mpegversion=(int)4, systemstream=false, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  rank=256;\
"
"\
"
"omx_h264enc,\
"
"  type=GstOmxH264Enc,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.video_encoder.avc,\
"
"  sink=\"video/x-raw-yuv, format=(fourcc){I420,YUY2,UYVY,NV12}, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  src=\"video/x-h264, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  rank=256;\
"
"\
"
"omx_h263enc,\
"
"  type=GstOmxH263Enc,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.video_encoder.h263,\
"
"  sink=\"video/x-raw-yuv, format=(fourcc){I420,YUY2,UYVY,NV12}, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  src=\"video/x-h263, variant=(string)itu, width=(int)[16,4096], height=(int)[16,4096], framerate=(fraction)[0,max];\",\
"
"  rank=256;\
"
"\
"
"omx_vorbisdec,\
"
"  type=GstOmxVorbisDec,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.audio_decoder.ogg.single,\
"
"  sink=\"application/ogg;\",\
"
"  src=\"audio/x-raw-int, endianess=(int)1234, width=(int)16, depth=(int)16, rate=(int)[8000, 96000], signed=(boolean)true, channels=(int)[1, 256];\",\
"
"  rank=128;\
"
"\
"
"omx_mp3dec,\
"
"  type=GstOmxMp3Dec,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.audio_decoder.mp3.mad,\
"
"  sink=\"audio/mpeg, mpegversion=(int)1, layer=(int)3, rate=(int)[8000, 48000], channels=(int)[1, 8], parsed=true;\",\
"
"  src=\"audio/x-raw-int, endianess=(int)1234, width=(int)16, depth=(int)16, rate=(int)[8000, 96000], signed=(boolean)true, channels=(int)[1, 2];\",\
"
"  rank=0;\
"
"omx_audiosink,\
"
"  type=GstOmxAudioSink,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.alsa.alsasink,\
"
"  sink=\"audio/x-raw-int, endianess=(int)1234, width=(int)16, depth=(int)16, rate=(int)[8000, 48000], signed=(boolean)true, channels=(int)[1, 8];\",\
"
"  rank=0;\
"
"omx_volume,\
"
"  type=GstOmxVolume,\
"
"  library-name=libomxil-bellagio.so.0,\
"
"  component-name=OMX.st.volume.component,\
"
"  sink=\"audio/x-raw-int, endianess=(int)1234, width=(int)16, depth=(int)16, rate=(int)[8000, 48000], signed=(boolean)true, channels=(int)[1, 8];\",\
"
"  src=\"audio/x-raw-int, endianess=(int)1234, width=(int)16, depth=(int)16, rate=(int)[8000, 48000], signed=(boolean)true, channels=(int)[1, 8];\",\
"
"  rank=0;\
"
;
