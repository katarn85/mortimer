%define release_name Molly 
%define dist_version 2.3.0

Summary:	Tizen release files
Name:		tizen-release
Version:	2.3.0
Release:	3.4
License:	GPLv2
Group:		System/Base
URL:		http://www.tizen.com
Provides:	system-release = %{version}-%{release}
Provides:	tizen-release = %{version}-%{release}
Provides:	os-release = %{version}-%{release}
BuildArch:	noarch
Source0:    RPM-GPG-KEY-tizen02

%description
Tizen release files such as various /etc/ files that define the release.

%prep

%build

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT/etc
echo "Tizen release %{dist_version} (%{release_name})" > $RPM_BUILD_ROOT/etc/tizen-release

ln -s tizen-release $RPM_BUILD_ROOT/etc/system-release
ln -snf /lib/modules/3.10.30/version $RPM_BUILD_ROOT/etc/bsp_version

echo "NAME=\"Tizen\"" > $RPM_BUILD_ROOT/etc/os-release
echo "VERSION=\"%{dist_version}, %{release_name}\"" >> $RPM_BUILD_ROOT/etc/os-release
echo "ID=tizen" >> $RPM_BUILD_ROOT/etc/os-release
echo "PRETTY_NAME=\"Tizen %{release_name} (%{dist_version})\"" >> $RPM_BUILD_ROOT/etc/os-release
echo "VERSION_ID=\"%{dist_version}\"" >> $RPM_BUILD_ROOT/etc/os-release

mkdir -p $RPM_BUILD_ROOT/etc/pki/rpm-gpg
cp %{SOURCE0} $RPM_BUILD_ROOT/etc/pki/rpm-gpg
pushd $RPM_BUILD_ROOT/etc/pki/rpm-gpg
ln -sf RPM-GPG-KEY-tizen02 RPM-GPG-KEY-tizen-2-primary
popd

%clean
rm -rf $RPM_BUILD_ROOT

%files
%config %attr(0644,root,root) /etc/tizen-release
/etc/system-release
/etc/os-release
/etc/bsp_version
/etc/pki/rpm-gpg
