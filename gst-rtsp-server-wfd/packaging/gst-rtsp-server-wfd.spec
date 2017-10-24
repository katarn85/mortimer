Name:       gst-rtsp-server-wfd
Summary:    Multimedia Framework Wifi-Display Library
Version:    0.1.65
Release:    1
Group:      System/Libraries
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
Requires(post):  /sbin/ldconfig
Requires(postun):  /sbin/ldconfig
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(mm-session)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:	pkgconfig(xau)
BuildRequires:	pkgconfig(x11)
BuildRequires:	pkgconfig(xdmcp)
BuildRequires:	pkgconfig(xext)
BuildRequires:	pkgconfig(xfixes)
BuildRequires:	pkgconfig(libdrm)
BuildRequires:	pkgconfig(dri2proto)
BuildRequires:	pkgconfig(libdri2)
BuildRequires:	pkgconfig(utilX)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(mm-wfd-common)
BuildRequires:  pkgconfig(capi-appfw-application)
#BuildRequires:  sec-product-features

BuildRoot:  %{_tmppath}/%{name}-%{version}-build

%description

%package devel
Summary:    Multimedia Framework Wifi-Display RTSP server library (DEV)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel

%package factory
Summary:    Multimedia Framework Wifi-Display RTSP server Library (Factory)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description factory

%prep
%setup -q

%build

./autogen.sh

CFLAGS+=" -DMMFW_DEBUG_MODE -DGST_EXT_TIME_ANALYSIS -DAUDIO_FILTER_EFFECT -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS
LDFLAGS+="-Wl,--rpath=%{_prefix}/lib -Wl,--hash-style=both -Wl,--as-needed"; export LDFLAGS

# always enable sdk build. This option should go away
# postfix the '--define "sec_product_feature_mmfw_codec_qc 1"' to gbs compilation command to enable QC MSM specific changes. While building for target SCM will automatically take care of it
./configure \
 --enable-sdk \
 --disable-hdcp \
 --enable-qc-specific \
 --enable-wfd-extended-features \
 --prefix=%{_prefix} \
 --disable-static

# Call make instruction with smp support
#make %{?jobs:-j%jobs}
make

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}/%{_datadir}/license
cp -rf %{_builddir}/%{name}-%{version}/LICENSE.LGPLv2.1 %{buildroot}%{_datadir}/license/%{name}

%clean
rm -rf %{buildroot}

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%manifest gst-rtsp-server-wfd.manifest
%defattr(-,root,root,-)
%{_datadir}/license/%{name}
%{_libdir}/*.so*
%{_bindir}/*

%files devel
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_includedir}/mmf/rtsp-server.h
%{_includedir}/mmf/rtsp-server-common.h
%{_includedir}/mmf/rtsp-server-wfd.h
%{_includedir}/mmf/rtsp-server-extended.h
%{_includedir}/mmf/rtsp-auth.h
%{_includedir}/mmf/rtsp-media-mapping.h
%{_includedir}/mmf/rtsp-client.h 
%{_includedir}/mmf/rtsp-session-pool.h
%{_includedir}/mmf/rtsp-media.h
%{_includedir}/mmf/rtsp-media-factory.h
%{_includedir}/mmf/rtsp-session.h
%{_libdir}/pkgconfig/*
