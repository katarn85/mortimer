Name:       gst-plugins-extension0.10
Summary:    GStreamer extra plugins
Version:    0.2.160
Release:    1
Group:      TO_BE/FILLED_IN
License:    TO_BE/FILLED_IN
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  prelink
BuildRequires:  gst-plugins-base-devel
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:  gst-openmax-devel
BuildRequires:  gst-plugins-drm-devel
%ifarch %{arm}
BuildRequires:  pkgconfig(mm-sound)
%endif

%description
Description: GStreamer extra plugins

%package -n gstreamer0.10-plugins-extension
Summary:    GStreamer extra plugins (common)
Group:      TO_BE/FILLED_IN

%description -n gstreamer0.10-plugins-extension
Description: GStreamer extra plugins (common)

%if 0
%ifarch %{arm}
%package -n gstreamer0.10-plugins-extension-soundalive
Summary:    GStreamer extra plugins (soundalive)
Group:      TO_BE/FILLED_IN

%description -n gstreamer0.10-plugins-extension-soundalive
Description: GStreamer extra plugins (soundalive)
%endif

%ifarch %{arm}
%package -n gstreamer0.10-plugins-extension-secrecord
Summary:    GStreamer extra plugins (secrecord)
Group:      TO_BE/FILLED_IN

%description -n gstreamer0.10-plugins-extension-secrecord
Description: GStreamer extra plugins (secrecord)
%endif
%endif

%ifarch %{arm}
%package -n gstreamer0.10-plugins-extension-wfdrtspsrc
Summary:    GStreamer extra plugins (wfdrtspsrc)
Group:      TO_BE/FILLED_IN

%description -n gstreamer0.10-plugins-extension-wfdrtspsrc
Description: GStreamer extra plugins (wfdrtspsrc)
%endif

%prep
%setup -q

%build
./autogen.sh
CFLAGS=" %{optflags}  -DGST_EXT_SOUNDALIVE_DISABLE_SA -DGST_EXT_XV_ENHANCEMENT -DGST_EXT_TIME_ANALYSIS -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS
%configure \
%ifarch %{arm}
    --disable-i386\
    --enable-wfd-extended-features\
    --enable-ext-wfdmanager\
    --enable-ext-wfdtsdemux\
    --disable-ext-soundalive\
    --disable-ext-secrecord\
    --enable-wfd-sink-uibc
%else
    --enable-i386\
    --disable-ext-wfdmanager\
    --disable-ext-wfdtsdemux\
    --disable-wfd-extended-features\
    --disable-ext-soundalive\
    --disable-ext-secrecord\
    --disable-wfd-sink-uibc 
%endif


make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
%ifarch %{arm}
#cp COPYING %{buildroot}/usr/share/license/gstreamer0.10-plugins-extension-soundalive
#cp COPYING %{buildroot}/usr/share/license/gstreamer0.10-plugins-extension-secrecord
cp COPYING %{buildroot}/usr/share/license/gstreamer0.10-plugins-extension-wfdrtspsrc
%endif
%make_install

%if 0
%ifarch %{arm}
%files -n gstreamer0.10-plugins-extension-soundalive
%manifest gstreamer0.10-plugins-extension-soundalive.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-0.10/libgstsoundalive.so
/usr/share/license/gstreamer0.10-plugins-extension-soundalive
%endif

%ifarch %{arm}
%files -n gstreamer0.10-plugins-extension-secrecord
%manifest gstreamer0.10-plugins-extension-secrecord.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-0.10/libgstsecrecord.so
/usr/share/license/gstreamer0.10-plugins-extension-secrecord
%endif
%endif

%ifarch %{arm}
%files -n gstreamer0.10-plugins-extension-wfdrtspsrc
%manifest gstreamer0.10-plugins-extension-wfdrtspsrc.manifest
%defattr(-,root,root,-)
%{_libdir}/libwfdrtspmanagerext.so*
%{_libdir}/gstreamer-0.10/libgstwfdtsdemux.so
%{_libdir}/gstreamer-0.10/libgstwfdmanager.so
/usr/share/license/gstreamer0.10-plugins-extension-wfdrtspsrc
%endif
