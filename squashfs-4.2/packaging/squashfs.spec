Name:		squashfs
Version:	4.2
Release:	1
Summary:	Tools for squashfs, a compressed read-only filesystem for Linux
Group:		System/Tools
Source0:	%{name}-%{version}.tar.gz
Source100:	%{name}.manifest
Patch0:		squashfs-4.2-makefile_config.patch
License:	GPL
URL:		http://squashfs.sourceforge.net/
BuildRequires:	eglibc-devel
BuildRequires:	lzo-devel
BuildRequires:	zlib-devel
Requires:	lzo

%description
Squashfs is a compressed read-only filesystem for Linux.
Squashfs is intended for general read-only filesystem use,
for archival use (i.e. in cases where a .tar.gz file may be used),
and in constrained block device/memory systems (e.g. embedded systems) where low overhead is needed.
The filesystem is currently stable, and has been tested on PowerPC, i586, Sparc and ARM architectures.

%prep
%setup -q
%patch0 -p1
cp %{SOURCE100} .

%build
cd squashfs-tools
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
cd squashfs-tools
mkdir -p %{buildroot}%{_bindir}
install mksquashfs %{buildroot}%{_bindir}/mksquashfs
%if 0%{?tizen_build_binary_release_type_eng}
install unsquashfs %{buildroot}%{_bindir}/unsquashfs
%endif

mkdir -p %{buildroot}/usr/share/license
cp -f ../COPYING %{buildroot}/usr/share/license/%{name}

%files
%defattr(-,root,root)
%attr(755,root,root) %{_bindir}/mksquashfs
%if 0%{?tizen_build_binary_release_type_eng}
%attr(755,root,root) %{_bindir}/unsquashfs
%endif
%doc CHANGES COPYING README
%manifest %{name}.manifest
/usr/share/license/%{name}

%changelog
* Wed Mar 02 2011 Silvan Calarco <silvan.calarco@...> 4.2-1mamba
- update to 4.2

* Mon Feb 08 2010 Silvan Calarco <silvan.calarco@...> 4.0-2mamba
- added patch for lzma support and rebuilt using lzma sdk

* Tue Apr 07 2009 Silvan Calarco <silvan.calarco@...> 4.0-1mamba
- update to 4.0

* Sun Sep 14 2008 Silvan Calarco <silvan.calarco@...> 3.4-1mamba
- update to 3.4

* Fri Jul 04 2008 Silvan Calarco <silvan.calarco@...> 3.3-2mamba
- applied patch to fix hang problem while creating livegames dvd

* Tue Jan 15 2008 Silvan Calarco <silvan.calarco@...> 3.3-1mamba
- update to 3.3

* Fri Jun 30 2006 Silvan Calarco <silvan.calarco@...> 3.0-1qilnx
- package created by autospec

