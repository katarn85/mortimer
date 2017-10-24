%define _unpackaged_files_terminate_build 0

Summary: 	The GNU core utilities: a set of tools commonly used in shell scripts

Name:		tizen-coreutils
Version:	6.9
Release:	12
License:	GPLv2+
Group:		System Environment/Base
Url:		http://www.gnu.org/software/coreutils/
Source0:	ftp://ftp.gnu.org/gnu/%{name}/coreutils-%{version}.tar.bz2
Source1:	mktemp-1.5.tar.gz
Source1001:	%{name}.manifest
Patch1:		coreutils-futimens.patch
Patch2:		coreutils-6.9-smack.patch

Patch1001:	mktemp-1.5-build.patch
BuildRequires:	autoconf >= 2.58
BuildRequires:	automake >= 1.10
BuildRequires:	gettext findutils

Provides:	fileutils sh-utils stat textutils mktemp
Provides:	coreutils
Obsoletes:	coreutils

%description
These are the GNU core utilities.  This package is the combination of
the old GNU fileutils, sh-utils, and textutils packages.

%prep
%setup -q -b 1 -n coreutils-%{version}
%patch1 -p1 -b .futimens
%patch2 -p1 -b .smack

%build
cp %{SOURCE1001} .
pushd ../mktemp-1.5
patch -p1 < %{PATCH1001}
%configure
make
popd

%configure
make

%install

pushd ../mktemp-1.5
make bindir=%{buildroot}/bin mandir=%{buildroot}%{_mandir} install
popd

%make_install

# man pages are not installed with make install
make mandir=%{buildroot}%{_mandir} install-man

# let be compatible with old fileutils, sh-utils and textutils packages :
mkdir -p %{buildroot}{/bin,%{_bindir},%{_sbindir},/sbin}
for f in cat chgrp chmod chown cp date dd df echo link ln ls mkdir mknod mv pwd rm rmdir sleep sync touch true uname unlink
do
    mv %{buildroot}{%{_bindir},/bin}/$f
done

# chroot was in /usr/sbin :
mv %{buildroot}{%{_bindir},%{_sbindir}}/chroot

# These come from util-linux and/or procps.
for i in hostname uptime kill ; do
    rm %{buildroot}{%{_bindir}/$i,%{_mandir}/man1/$i.1}
done

# Use hard links instead of symbolic links for LC_TIME files (bug #246729).
find %{buildroot}%{_datadir}/locale -type l | \
(while read link
 do
   target=$(readlink "$link")
   rm -f "$link"
   ln "$(dirname "$link")/$target" "$link"
 done)

mkdir -p $RPM_BUILD_ROOT%{_datadir}/license
for keyword in LICENSE COPYING COPYRIGHT;
do
	for file in `find %{_builddir} -name $keyword`;
	do
		cat $file >> $RPM_BUILD_ROOT%{_datadir}/license/%{name};
		echo "";
	done;
done

%clean
rm -rf $RPM_BUILD_ROOT

%docs_package

%files
%manifest %{name}.manifest
%doc COPYING
%{_datadir}/license/%{name}
/bin/cat
/bin/chgrp
/bin/chmod
/bin/chown
/bin/cp
/bin/date
/bin/dd
/bin/df
/bin/echo
/bin/ln
/bin/ls
/bin/mkdir
/bin/mknod
/bin/mktemp
/bin/mv
/bin/pwd
/bin/rm
/bin/rmdir
/bin/sleep
/bin/sync
/bin/touch
/bin/true
/bin/uname
%{_bindir}/basename
%{_bindir}/cksum
%{_bindir}/cut
%{_bindir}/dirname
%{_bindir}/du
%{_bindir}/env
%{_bindir}/expr
%{_bindir}/head
%{_bindir}/id
%{_bindir}/install
%{_bindir}/md5sum
%{_bindir}/nice
%{_bindir}/od
%{_bindir}/printenv
%{_bindir}/printf
%{_bindir}/readlink
%{_bindir}/seq
%{_bindir}/sort
%{_bindir}/stat
%{_bindir}/tac
%{_bindir}/tail
%{_bindir}/tee
%{_bindir}/test
%{_bindir}/tr
%{_bindir}/wc
%{_bindir}/who
%{_bindir}/whoami
%{_sbindir}/chroot
