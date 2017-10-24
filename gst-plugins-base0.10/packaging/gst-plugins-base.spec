Name:       gst-plugins-base
Summary:    GStreamer streaming media framework base plug-ins
Version:    0.20.41+tv
Release:    63
Group:      Applications/Multimedia
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
#Patch0:     Samsung-feature-bugs.patch
Requires(post): /sbin/ldconfig
Requires(post): /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(ogg)
BuildRequires:  pkgconfig(theora)
BuildRequires:  pkgconfig(vorbis)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(gstreamer-base-0.10)
BuildRequires:  pkgconfig(xv)
BuildRequires:  pkgconfig(xfixes)
BuildRequires:  pkgconfig(dri2proto)
BuildRequires:  pkgconfig(libdri2)
BuildRequires:  pkgconfig(libtbm)
%ifarch armv7l
#BuildRequires:  pkgconfig(libtbm-hawk-m)
%else
BuildRequires:  pkgconfig(libtbm-emulator)
%endif
#BuildRequires:  pkgconfig(libdrm_slp)
BuildRequires:  pkgconfig(xdamage)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  intltool
BuildRequires: kernel-headers
BuildRequires: pkgconfig(alsa)
BuildRequires: pkgconfig(audio-session-mgr)
BuildRequires: pkgconfig(gstreamer-interfaces-0.10)
BuildRequires: pkgconfig(gstreamer-app-0.10)
BuildRequires: pkgconfig(x11)
BuildRequires: pkgconfig(kernel-headers)
BuildRequires: pkgconfig(libavoc)
BuildRequires: pkgconfig(libresolution-util)
BuildRequires: 	pkgconfig(libsec-audio)
BuildRequires: 	pkgconfig(vconf)

%description
A well-groomed and well-maintained collection of GStreamer plug-ins and elements, 
spanning the range of possible types of elements one would want to write for GStreamer.



%package devel
Summary:    Development tools for GStreamer base plugins
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Separate sub-package for development based on gstreamer base plugins. 


%package tools
Summary:    Gstreamer base plugins utilities
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}

%description tools
Separate sub-package contaning helper applications of gstreamer base plugins.



%prep
%setup -q 

#%patch0 -p1

%build
%autogen --noconfigure

%define __debug_install_post   \
%{_rpmconfigdir}/find-debuginfo.sh %{?_find_debuginfo_opts} "%{_builddir}/%{?buildsubdir}"\
%{nil}

# Same with before
# -DGST_EXT_LINK_FIMCCONVERT
export CFLAGS+=" -Wall -g -fPIC\
 -DGST_EXT_XV_ENHANCEMENT\
 -DSAMSUNG_WFD_SPEC\
 -DGST_EXT_SS_TYPE\
 -DGST_EXT_LINK_FIMCCONVERT\
 -DGST_EXT_ENABLE_SMI\
 -DGST_EXT_TYPEFIND_ENHANCEMENT\
 -DGST_EXT_DECODEBIN2_QUEUESIZE\
 -DGST_EXT_DECODEBIN2_MODIFICATION\
 -DGST_EXT_URIDECODEBIN\
 -DGST_EXT_DECODEBIN\
 -DUSE_TBM_FOXP\
%ifnarch armv7l
 -DUSE_TBM_SDK\
%endif
 -DGST_EXT_MIME_TYPES\
 -DGST_EXT_CODE_ENHANCEMENT\
 -DGST_EXT_USE_SPLITTER\
 -DGST_EXT_UPSTREAM\
 -DGST_EXT_USE_SUBPCM_VOLUME\
 -DGST_EXT_VIDEO_XJPEG
 "

%configure --prefix=/usr\
 --disable-static\
 --disable-nls\
 --with-html-dir=/tmp/dump\
 --disable-examples\
 --disable-audiorate\
 --disable-gdp\
 --disable-cdparanoia\
 --disable-gnome_vfs\
 --disable-libvisual\
 --disable-freetypetest\
 --disable-rpath\
 --disable-valgrind\
 --disable-gcov\
 --disable-gtk-doc\
 --disable-debug\
 --enable-xvideo\
 --with-audioresample-format=int

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

#%ifarch armv7l
#install -c sys/xvimage/external_lib/* %{buildroot}%{_libdir}
#%endif

rm -rf %{buildroot}/tmp/dump

%post -p /sbin/ldconfig

/usr/bin/vconftool set -t int memory/Player/XvStateInfo 0 -g 29 -f -i -s system::vconf_multimedia
%postun -p /sbin/ldconfig


%files
%manifest gst-plugins-base.manifest
%defattr(-,root,root,-)
#%doc COPYING 
# libraries
%{_libdir}/libgstinterfaces-0.10.so.*
%{_libdir}/libgstaudio-0.10.so.*
%exclude %{_libdir}/libgstcdda-0.10.so.*
%{_libdir}/libgstfft-0.10.so.*
%{_libdir}/libgstriff-0.10.so.*
%{_libdir}/libgsttag-0.10.so.*
%{_libdir}/libgstnetbuffer-0.10.so.*
%{_libdir}/libgstrtp-0.10.so.*
%{_libdir}/libgstvideo-0.10.so.*
%{_libdir}/libgstpbutils-0.10.so.*
%{_libdir}/libgstrtsp-0.10.so.*
%{_libdir}/libgstsdp-0.10.so.*
%{_libdir}/libgstapp-0.10.so.*
# base plugins without external dependencies
%{_libdir}/gstreamer-0.10/libgstadder.so
%{_libdir}/gstreamer-0.10/libgstaudioconvert.so
%{_libdir}/gstreamer-0.10/libgstaudiotestsrc.so
%{_libdir}/gstreamer-0.10/libgstffmpegcolorspace.so
%{_libdir}/gstreamer-0.10/libgstdecodebin.so
%{_libdir}/gstreamer-0.10/libgstdecodebin2.so
%{_libdir}/gstreamer-0.10/libgstplaybin.so
%{_libdir}/gstreamer-0.10/libgsttypefindfunctions.so
%{_libdir}/gstreamer-0.10/libgstvideotestsrc.so
%{_libdir}/gstreamer-0.10/libgstsubparse.so
%{_libdir}/gstreamer-0.10/libgstvolume.so
%{_libdir}/gstreamer-0.10/libgstvideorate.so
%{_libdir}/gstreamer-0.10/libgstvideoscale.so
%{_libdir}/gstreamer-0.10/libgsttcp.so
%{_libdir}/gstreamer-0.10/libgstaudiospectrum.so
#%{_libdir}/gstreamer-0.10/libgstvideo4linux.so
%{_libdir}/gstreamer-0.10/libgstaudioresample.so
%{_libdir}/gstreamer-0.10/libgstapp.so
%{_libdir}/gstreamer-0.10/libgstxvimagesink.so
%{_libdir}/gstreamer-0.10/libgstframecollector.so
%{_libdir}/gstreamer-0.10/libgstaudiodatasplitter.so
%{_libdir}/gstreamer-0.10/libgstmixersink.so
%{_libdir}/gstreamer-0.10/libgstplayermixer.so*
%ifarch armv7l
%{_libdir}/gstreamer-0.10/libgstspdifsink.so
%endif
%exclude %{_libdir}/gstreamer-0.10/libgstencodebin.so
# base plugins with dependencies
%{_libdir}/gstreamer-0.10/libgstalsa.so
%{_libdir}/gstreamer-0.10/libgstogg.so
%{_libdir}/gstreamer-0.10/libgsttheora.so
%{_libdir}/gstreamer-0.10/libgstvorbis.so
%{_libdir}/gstreamer-0.10/libgstximagesink.so
%{_libdir}/gstreamer-0.10/libgstgio.so
%{_libdir}/gstreamer-0.10/libgstremotesink.so
# data
%{_datadir}/gst-plugins-base/license-translations.dict


%files devel
%dir %{_includedir}/gstreamer-0.10/gst/app
%{_includedir}/gstreamer-0.10/gst/tag/xmpwriter.h
%{_includedir}/gstreamer-0.10/gst/app/gstappbuffer.h
%{_includedir}/gstreamer-0.10/gst/app/gstappsink.h
%{_includedir}/gstreamer-0.10/gst/app/gstappsrc.h
%dir %{_includedir}/gstreamer-0.10/gst/audio
%{_includedir}/gstreamer-0.10/gst/audio/audio.h
%{_includedir}/gstreamer-0.10/gst/audio/audio-enumtypes.h
%{_includedir}/gstreamer-0.10/gst/audio/gstaudioclock.h
%{_includedir}/gstreamer-0.10/gst/audio/gstaudiodecoder.h
%{_includedir}/gstreamer-0.10/gst/audio/gstaudioencoder.h
%{_includedir}/gstreamer-0.10/gst/audio/gstaudiofilter.h
%{_includedir}/gstreamer-0.10/gst/audio/gstaudioiec61937.h
%{_includedir}/gstreamer-0.10/gst/audio/gstaudiosink.h
%{_includedir}/gstreamer-0.10/gst/audio/gstaudiosrc.h
%{_includedir}/gstreamer-0.10/gst/audio/gstbaseaudiosink.h
%{_includedir}/gstreamer-0.10/gst/audio/gstbaseaudiosrc.h
%{_includedir}/gstreamer-0.10/gst/audio/gstringbuffer.h
%{_includedir}/gstreamer-0.10/gst/audio/mixerutils.h
%{_includedir}/gstreamer-0.10/gst/audio/multichannel.h
%exclude %dir %{_includedir}/gstreamer-0.10/gst/cdda
%exclude %{_includedir}/gstreamer-0.10/gst/cdda/gstcddabasesrc.h
%dir %{_includedir}/gstreamer-0.10/gst/floatcast
%{_includedir}/gstreamer-0.10/gst/floatcast/floatcast.h
%dir %{_includedir}/gstreamer-0.10/gst/fft
%{_includedir}/gstreamer-0.10/gst/fft/gstfft*.h
%dir %{_includedir}/gstreamer-0.10/gst/interfaces
%{_includedir}/gstreamer-0.10/gst/interfaces/cameracontrol.h
%{_includedir}/gstreamer-0.10/gst/interfaces/cameracontrolchannel.h
%{_includedir}/gstreamer-0.10/gst/interfaces/colorbalance.h
%{_includedir}/gstreamer-0.10/gst/interfaces/colorbalancechannel.h
%{_includedir}/gstreamer-0.10/gst/interfaces/interfaces-enumtypes.h
%{_includedir}/gstreamer-0.10/gst/interfaces/mixer.h
%{_includedir}/gstreamer-0.10/gst/interfaces/mixeroptions.h
%{_includedir}/gstreamer-0.10/gst/interfaces/mixertrack.h
%{_includedir}/gstreamer-0.10/gst/interfaces/navigation.h
%{_includedir}/gstreamer-0.10/gst/interfaces/propertyprobe.h
%{_includedir}/gstreamer-0.10/gst/interfaces/tuner.h
%{_includedir}/gstreamer-0.10/gst/interfaces/tunerchannel.h
%{_includedir}/gstreamer-0.10/gst/interfaces/tunernorm.h
%{_includedir}/gstreamer-0.10/gst/interfaces/videoorientation.h
%{_includedir}/gstreamer-0.10/gst/interfaces/xoverlay.h
%{_includedir}/gstreamer-0.10/gst/interfaces/streamvolume.h
%dir %{_includedir}/gstreamer-0.10/gst/netbuffer
%{_includedir}/gstreamer-0.10/gst/netbuffer/gstnetbuffer.h
%dir %{_includedir}/gstreamer-0.10/gst/pbutils
%{_includedir}/gstreamer-0.10/gst/pbutils/codec-utils.h
%{_includedir}/gstreamer-0.10/gst/pbutils/descriptions.h
%{_includedir}/gstreamer-0.10/gst/pbutils/gstdiscoverer.h
%{_includedir}/gstreamer-0.10/gst/pbutils/gstpluginsbaseversion.h
%{_includedir}/gstreamer-0.10/gst/pbutils/install-plugins.h
%{_includedir}/gstreamer-0.10/gst/pbutils/missing-plugins.h
%{_includedir}/gstreamer-0.10/gst/pbutils/pbutils.h
%{_includedir}/gstreamer-0.10/gst/pbutils/pbutils-enumtypes.h
%{_includedir}/gstreamer-0.10/gst/pbutils/encoding-profile.h
%{_includedir}/gstreamer-0.10/gst/pbutils/encoding-target.h

%dir %{_includedir}/gstreamer-0.10/gst/riff
%{_includedir}/gstreamer-0.10/gst/riff/riff-ids.h
%{_includedir}/gstreamer-0.10/gst/riff/riff-media.h
%{_includedir}/gstreamer-0.10/gst/riff/riff-read.h
%dir %{_includedir}/gstreamer-0.10/gst/rtp
%{_includedir}/gstreamer-0.10/gst/rtp/gstbasertpaudiopayload.h
%{_includedir}/gstreamer-0.10/gst/rtp/gstbasertpdepayload.h
%{_includedir}/gstreamer-0.10/gst/rtp/gstbasertppayload.h
%{_includedir}/gstreamer-0.10/gst/rtp/gstrtcpbuffer.h
%{_includedir}/gstreamer-0.10/gst/rtp/gstrtpbuffer.h
%{_includedir}/gstreamer-0.10/gst/rtp/gstrtppayloads.h
%dir %{_includedir}/gstreamer-0.10/gst/rtsp
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtsp-enumtypes.h
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtspbase64.h
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtspconnection.h
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtspdefs.h
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtspextension.h
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtspmessage.h
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtsprange.h
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtsptransport.h
%{_includedir}/gstreamer-0.10/gst/rtsp/gstrtspurl.h
%dir %{_includedir}/gstreamer-0.10/gst/sdp/
%{_includedir}/gstreamer-0.10/gst/sdp/gstsdp.h
%{_includedir}/gstreamer-0.10/gst/sdp/gstsdpmessage.h
%dir %{_includedir}/gstreamer-0.10/gst/tag
%{_includedir}/gstreamer-0.10/gst/tag/tag.h
%{_includedir}/gstreamer-0.10/gst/tag/gsttagdemux.h
%{_includedir}/gstreamer-0.10/gst/tag/gsttagmux.h
%dir %{_includedir}/gstreamer-0.10/gst/video
%{_includedir}/gstreamer-0.10/gst/video/gstvideofilter.h
%{_includedir}/gstreamer-0.10/gst/video/gstvideosink.h
%{_includedir}/gstreamer-0.10/gst/video/video.h
%{_includedir}/gstreamer-0.10/gst/video/video-enumtypes.h
%{_includedir}/gstreamer-0.10/gst/video/video-overlay-composition.h
%dir %{_includedir}/gstreamer-0.10/sys/playermixer
%{_includedir}/gstreamer-0.10/sys/playermixer/player_mixer.h
%{_libdir}/libgstaudio-0.10.so
%{_libdir}/libgstinterfaces-0.10.so
%{_libdir}/libgstnetbuffer-0.10.so
%{_libdir}/libgstriff-0.10.so
%{_libdir}/libgstrtp-0.10.so
%{_libdir}/libgsttag-0.10.so
%{_libdir}/libgstvideo-0.10.so
%exclude %{_libdir}/libgstcdda-0.10.so
%{_libdir}/libgstpbutils-0.10.so
%{_libdir}/libgstrtsp-0.10.so
%{_libdir}/libgstsdp-0.10.so
%{_libdir}/libgstfft-0.10.so
%{_libdir}/libgstapp-0.10.so
# pkg-config files
%{_libdir}/pkgconfig/*.pc
%{_libdir}/gstreamer-0.10/libgstplayermixer.so*

%files tools
%manifest gst-plugins-base-tools.manifest
%defattr(-,root,root,-)
# helper programs
%{_bindir}/gst-discoverer-0.10
%exclude %{_bindir}/gst-visualise-0.10
%exclude %{_mandir}/man1/gst-visualise-0.10*

