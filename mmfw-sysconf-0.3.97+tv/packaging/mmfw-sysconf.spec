Name:       mmfw-sysconf
Summary:    Multimedia Framework system configuration package
Version:    0.3.97+tv
Release:    5
VCS:        framework/multimedia/mmfw-sysconf#mmfw-sysconf_0.1.110-0-144-g25720f9f7daad9a897662eb6059d71d958cc3f06
Group:      TO_BE/FILLED_IN
License:    Apache-2.0
Source0:    mmfw-sysconf-%{version}.tar.gz

%description
Multimedia Framework system configuration package

%define chip_hawkm 1
%ifarch %{arm}

%if %{?chip_hawkm}
%package hawkm
Summary: Multimedia Framework system configuration package for hawkm
Group: TO_BE/FILLED_IN
License:    Apache-2.0

%description hawkm
Multimedia Framework system configuration package for hawkm

%else
%package cleansdk-e4x12
Summary: Multimedia Framework system configuration package for cleansdk-e4x12
Group: TO_BE/FILLED_IN
License:    Apache-2.0

%description cleansdk-e4x12
Multimedia Framework system configuration package for cleansdk-e4x12

%package e3250
Summary: Multimedia Framework system configuration package for e3250
Group: TO_BE/FILLED_IN
License:    Apache-2.0

%description e3250
Multimedia Framework system configuration package for e3250
%endif

%else
%package simulator
Summary: Multimedia Framework system configuration package for simulator
Group: TO_BE/FILLED_IN
License:    Apache-2.0

%description simulator
Multimedia Framework system configuration package for simulator
%endif

%prep
%setup -q -n %{name}-%{version}

%build

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/etc
mkdir -p %{buildroot}/usr
mkdir -p %{buildroot}/usr/share/license
%ifarch %{arm}
%if %{?chip_hawkm}
cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-hawkm
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-hawkm
%else
%if 0%{?tizen_profile_wearable}
cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-e3250
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-e3250
%else
cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-cleansdk-e4x12
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-cleansdk-e4x12
%endif
%endif
%else
cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/mmfw-sysconf-simulator
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/mmfw-sysconf-simulator
%endif


%ifarch %{arm}
%if %{?chip_hawkm}
mkdir -p %{buildroot}/mmfw-sysconf-hawkm
cp -arf mmfw-sysconf-hawkm/* %{buildroot}/mmfw-sysconf-hawkm
%else
mkdir -p %{buildroot}/mmfw-sysconf-cleansdk-e4x12
cp -arf mmfw-sysconf-cleansdk-e4x12/* %{buildroot}/mmfw-sysconf-cleansdk-e4x12
mkdir -p %{buildroot}/mmfw-sysconf-e3250
cp -arf mmfw-sysconf-e3250/* %{buildroot}/mmfw-sysconf-e3250
%endif
%else
mkdir -p %{buildroot}/mmfw-sysconf-simulator
cp -arf mmfw-sysconf-simulator/* %{buildroot}/mmfw-sysconf-simulator
%endif

%ifarch %{arm}

%if %{?chip_hawkm}
%post hawkm
cp -arf /mmfw-sysconf-hawkm/* /
rm -rf /mmfw-sysconf-hawkm

%else
	 
%post cleansdk-e4x12
cp -arf /mmfw-sysconf-cleansdk-e4x12/* /
rm -rf /mmfw-sysconf-cleansdk-e4x12
%post e3250
cp -arf /mmfw-sysconf-e3250/* /
rm -rf /mmfw-sysconf-e3250
%endif
%else
%post simulator
cp -arf /mmfw-sysconf-simulator/* /
rm -rf /mmfw-sysconf-simulator
%endif

%ifarch %{arm}

%if %{?chip_hawkm}
%files hawkm
%manifest mmfw-sysconf-hawkm.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-hawkm/etc/asound.conf
/mmfw-sysconf-hawkm/etc/pulse/*
/mmfw-sysconf-hawkm/usr/etc/*.ini
/mmfw-sysconf-hawkm/usr/etc/gst-openmax.conf
/mmfw-sysconf-hawkm/usr/etc/gst-tz-openmax.conf
/mmfw-sysconf-hawkm/usr/etc/gstreamer-registry-copy.sh
/mmfw-sysconf-hawkm/usr/etc/gstreamer-registry.sh
/mmfw-sysconf-hawkm/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-hawkm/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-hawkm/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/usr/share/license/*
%else

%files cleansdk-e4x12
%manifest mmfw-sysconf-cleansdk-e4x12.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-cleansdk-e4x12/etc/asound.conf
/mmfw-sysconf-cleansdk-e4x12/etc/pulse/*
/mmfw-sysconf-cleansdk-e4x12/usr/etc/*.ini
/mmfw-sysconf-cleansdk-e4x12/usr/etc/gst-openmax.conf
/mmfw-sysconf-cleansdk-e4x12/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-cleansdk-e4x12/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-cleansdk-e4x12/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/usr/share/license/*

%files e3250
%manifest mmfw-sysconf-e3250.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-e3250/etc/asound.conf
/mmfw-sysconf-e3250/etc/pulse/*
/mmfw-sysconf-e3250/etc/profile.d/*
/mmfw-sysconf-e3250/usr/etc/*.ini
/mmfw-sysconf-e3250/usr/etc/*.txt
/mmfw-sysconf-e3250/usr/etc/gst-openmax.conf
/mmfw-sysconf-e3250/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-e3250/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-e3250/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-e3250/opt/system/*
/usr/share/license/*
%endif
%else
%files simulator
%manifest mmfw-sysconf-simulator.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-simulator/etc/asound.conf
/mmfw-sysconf-simulator/etc/pulse/*
/mmfw-sysconf-simulator/usr/etc/*.ini
/mmfw-sysconf-simulator/usr/etc/gst-openmax.conf
/mmfw-sysconf-simulator/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-simulator/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-simulator/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/usr/share/license/mmfw-sysconf-simulator
%endif

