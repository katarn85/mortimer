Summary: Utilities for managing accounts and shadow password files
Name: shadow-utils
Version: 4.1.4.2
Release: 8
URL: http://pkg-shadow.alioth.debian.org/
License: BSD and GPLv2+
Group: System/Base

Source0: http://pkg-shadow.alioth.debian.org/releases/shadow-%{version}.tar.gz
%if 0%{?tizen_build_binary_release_type_eng:1}
Source1: login-eng.defs
%else
Source1: login.defs
%endif
Source2: securetty
Source3: useradd.default
Source1001:     %{name}.manifest

Patch0: 008_login_log_failure_in_FTMP
Patch1: 008_su_get_PAM_username
Patch2: 008_su_no_sanitize_env
Patch3: 401_cppw_src.dpatch
Patch4: 402_cppw_selinux
Patch5: 428_grpck_add_prune_option
Patch6: 429_login_FAILLOG_ENAB
Patch7: 463_login_delay_obeys_to_PAM
Patch8: 483_su_fakelogin_wrong_arg0
Patch9: 501_commonio_group_shadow
Patch10: 506_relaxed_usernames
Patch11: 508_nologin_in_usr_sbin
Patch12: 523_su_arguments_are_concatenated
Patch13: 523_su_arguments_are_no_more_concatenated_by_default
Patch14: 542_useradd-O_option
Patch15: shadow-4.1.4.2-redhat.patch
Patch16: shadow-4.1.4.1-goodname.patch
Patch17: shadow-4.1.4.2-leak.patch
Patch18: shadow-4.1.4.2-fixes.patch
Patch19: shadow-4.1.4.2-rounds_prefix.patch

Requires: setup

%description
The shadow package includes the necessary programs for
converting UNIX password files to the shadow password format, plus
programs for managing user and group accounts. The pwconv command
converts passwords to the shadow password format. The pwunconv command
unconverts shadow passwords and generates an npasswd file (a standard
UNIX password file). The pwck command checks the integrity of password
and shadow files. The lastlog command prints out the last login times
for all users. The useradd, userdel, and usermod commands are used for
managing user accounts. The groupadd, groupdel, and groupmod commands
are used for managing group accounts.

%prep
%setup -q

%patch0 -p1 -b .008_login_log_failure_in_FTMP
%patch1 -p1 -b .008_su_get_PAM_username
%patch2 -p1 -b .008_su_no_sanitize_env
%patch3 -p1 -b .401_cppw_src.dpatch
%patch4 -p1 -b .402_cppw_selinux
%patch5 -p1 -b .428_grpck_add_prune_option
%patch6 -p1 -b .429_login_FAILLOG_ENAB
%patch7 -p1 -b .463_login_delay_obeys_to_PAM
%patch8 -p1 -b .483_su_fakelogin_wrong_arg0
%patch9 -p1 -b .501_commonio_group_shadow
%patch10 -p1 -b .506_relaxed_usernames
%patch11 -p1 -b .508_nologin_in_usr_sbin
%patch12 -p1 -b .523_su_arguments_are_concatenated
%patch13 -p1 -b .523_su_arguments_are_no_more_concatenated_by_default
%patch14 -p1 -b .542_useradd-O_option
%patch15 -p1 -b .redhat
%patch16 -p1 -b .goodname
%patch17 -p1 -b .leak
%patch18 -p1 -b .fixes
%patch19 -p1 -b .rounds_prefix

%build
cp %{SOURCE1001} .
%configure --without-libcrack --without-audit --mandir=/usr/share/man --without-libpam --without-selinux --enable-shadowgrp --disable-man --disable-account-tools-setuid --with-group-name-max-length=32 --disable-nls

make

%install
make install DESTDIR=%{buildroot}
install -d %{buildroot}/%{_sysconfdir}/default
install -c -m 444 %SOURCE1 %{buildroot}/%{_sysconfdir}/login.defs
install -c -m 444 %SOURCE2 %{buildroot}/%{_sysconfdir}/
install -c -m 644 %SOURCE3 %{buildroot}/%{_sysconfdir}/default/useradd
install -d %{buildroot}/sbin

chmod u+s %{buildroot}/%{_bindir}/su

install -d %{buildroot}/bin
mv %{buildroot}/%{_bindir}/su %{buildroot}/bin/
mv %{buildroot}/%{_bindir}/login %{buildroot}/bin/

# remove not needed files
rm %{buildroot}/%{_sbindir}/logoutd
rm %{buildroot}/%{_bindir}/groups
rm %{buildroot}/%{_sysconfdir}/login.access
rm %{buildroot}/%{_sysconfdir}/limits
rm %{buildroot}/%{_sysconfdir}/securetty
rm %{buildroot}/%{_bindir}/chfn
rm %{buildroot}/%{_bindir}/chsh
rm %{buildroot}/%{_bindir}/expiry
rm %{buildroot}/%{_sbindir}/chgpasswd
rm %{buildroot}/%{_sbindir}/grpck
rm %{buildroot}/%{_sbindir}/grpconv
rm %{buildroot}/%{_sbindir}/grpunconv
rm %{buildroot}/%{_sbindir}/pwck
rm %{buildroot}/%{_sbindir}/pwconv
rm %{buildroot}/%{_sbindir}/pwunconv
rm %{buildroot}/%{_sbindir}/vigr
rm %{buildroot}/%{_sbindir}/vipw

%remove_docs

mkdir -p $RPM_BUILD_ROOT%{_datadir}/license
for keyword in LICENSE COPYING COPYRIGHT;
do
	for file in `find %{_builddir} -name $keyword`;
	do
		cat $file >> $RPM_BUILD_ROOT%{_datadir}/license/%{name};
		echo "";
	done;
done

%files
%manifest %{name}.manifest
%{_datadir}/license/%{name}
%dir %{_sysconfdir}/default
%attr(0600,root,root)   %config(noreplace) %{_sysconfdir}/default/useradd
%attr(0644,root,root)   %config(noreplace) %{_sysconfdir}/login.defs
/bin/login
%{_bindir}/faillog
%{_bindir}/lastlog
%{_bindir}/sg
%{_sbindir}/chpasswd
%{_sbindir}/groupadd
%{_sbindir}/groupdel
%{_sbindir}/groupmems
%{_sbindir}/groupmod
%{_sbindir}/newusers
%{_sbindir}/nologin
%{_sbindir}/useradd
%{_sbindir}/userdel
%{_sbindir}/usermod
%if 0%{?tizen_build_binary_release_type_eng} == 1
/bin/su
%{_bindir}/chage
%{_bindir}/gpasswd
%{_bindir}/newgrp
%{_bindir}/passwd
%else
%exclude /bin/su
%exclude %{_bindir}/chage
%exclude %{_bindir}/gpasswd
%exclude %{_bindir}/newgrp
%exclude %{_bindir}/passwd
%endif
