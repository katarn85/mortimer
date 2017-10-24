Name:       gst-plugins-ext0.10
Version:    0.4.88
Summary:    GStreamer extra plugins (common)
Release:    9
Group:      libs
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(camsrcjpegenc)
#BuildRequires:  pkgconfig(drm-client)
#BuildRequires:  pkgconfig(drm-trusted)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:  pkgconfig(evas)
BuildRequires:  gst-plugins-base-devel 
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(libexif)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:	pkgconfig(libdri2)
BuildRequires:	pkgconfig(x11)
BuildRequires:	pkgconfig(xext)
BuildRequires:	pkgconfig(xv)
BuildRequires:	pkgconfig(xdamage)
BuildRequires:	pkgconfig(libdrm)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:	pkgconfig(dri2proto)
BuildRequires:	pkgconfig(xfixes)
BuildRequires:	pkgconfig(libtbm)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(native-buffer)
BuildRequires:  pkgconfig(libcrypto)
BuildRequires:  pkgconfig(kernel-headers)

%description
GStreamer extra plugins (common)

%prep
%setup -q


%build
export CFLAGS+=" -DCONTROL_PAGECACHE -DGST_EXT_TIME_ANALYSIS -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "

./autogen.sh --disable-static
%configure --disable-static \
 --enable-ext-hlsdemux2\
 --disable-ext-drmsrc\
 --disable-ext-xvimagesrc\
%if 0%{?tizen_build_binary_release_type_eng}
 --enable-pcmdump
%endif

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}
%make_install
mkdir -p %{buildroot}/usr/etc/
install -m 755 hlsdemux2/predefined_frame/* %{buildroot}/usr/etc/

%files
%manifest gst-plugins-ext0.10.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-0.10/*.so
/usr/share/license/%{name}
/usr/etc/blackframe_QVGA.264
/usr/etc/sec_audio_fixed_qvga.264
/usr/etc/sec_audio_fixed_qvga.jpg
