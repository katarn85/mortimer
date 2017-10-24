# This is a spec file for British English hunspell dictionary.
# In addition to 'pure British' dictionary (identified by en_GB), this package 
# creates different links for national dictionaries' variants - such as Australian, etc.)

# The word lists are based on the following source:
#   British English dictionary (http://en-gb.pyxidium.co.uk/dictionary/en_GB.zip)
#   Licence is LGPL

# This spec file is inspired by the the official Fedora Core git repo 
# (git://pkgs.fedoraproject.org/hunspell-en.git), but so much modified by 
# Michal Roj <m.roj@samsung.com> that virtually rewritten.

# The modifications introduced are as follows:
# - the install script creates both myspell and hunspell directories in /usr/share
# - (build) requirements are removed (e.g., no hunspell itself is needed to install dictionaries)

# How to update the dictionaries?
# Just download British English dictionary from http://en-gb.pyxidium.co.uk/dictionary/en_GB.zip 
# Then decompress it and exchange the current .dic and .aff files with something newer.
# ... and Voila

Name: hunspell-en
Summary: British English hunspell dictionary
Version: R1.20
Release: 3
Source0: %{name}-%{version}.tar.gz
Group: Applications/Text
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
License: LGPLv2+
BuildArch: noarch

%description
British English (European and Commonwealth) hunspell dictionary

%prep
%setup

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_datadir}/myspell/dicts
mkdir -p $RPM_BUILD_ROOT/%{_datadir}/hunspell
cp -p en_*.dic en_*.aff $RPM_BUILD_ROOT/%{_datadir}/myspell/dicts
cp -p en_*.dic en_*.aff $RPM_BUILD_ROOT/%{_datadir}/myspell/dicts
mkdir -p %{buildroot}/usr/share/license
cp LICENSE %{buildroot}/usr/share/license/%{name}

pushd $RPM_BUILD_ROOT/%{_datadir}/myspell/dicts
# Make links to 'usr/share/hunspell' from the 'myspell/dicts' directory.
ln -s ../myspell/dicts/en_GB.aff $RPM_BUILD_ROOT/%{_datadir}/hunspell/en_GB.aff
ln -s ../myspell/dicts/en_GB.dic $RPM_BUILD_ROOT/%{_datadir}/hunspell/en_GB.dic

# Now, create links for all GB-compatible English dictionaries (the idea taken from Fedora Core)
en_GB_aliases="en_AG en_AU en_BS en_BW en_BZ en_DK en_GH en_HK en_IE en_IN en_JM en_MW en_NA en_NG en_NZ en_SG en_TT en_ZA en-ZM en_ZW"
for lang in $en_GB_aliases; do
	ln -s en_GB.aff $lang.aff
	ln -s en_GB.dic $lang.dic
	ln -s ../myspell/dicts/$lang.aff $RPM_BUILD_ROOT/%{_datadir}/hunspell/$lang.aff
	ln -s ../myspell/dicts/$lang.dic $RPM_BUILD_ROOT/%{_datadir}/hunspell/$lang.dic
done
popd

%clean
rm -rf $RPM_BUILD_ROOT

%files -n hunspell-en
%manifest hunspell-en.manifest
%defattr(-,root,root,-)
%doc README_en_GB.txt
%{_datadir}/myspell/dicts/*
%{_datadir}/hunspell/*
/usr/share/license/%{name}

%changelog
* Tue Sep 4 2012 Michal Roj <m.roj@samsung.com> - second version
* Tue Aug 14 2012 Michal Roj <m.roj@samsung.com> - first version
