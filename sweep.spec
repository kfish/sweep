%define name sweep
%define version 0.0.9
%define release 1
%define prefix /usr

Summary: Sound wave editor

Name: %{name}
Version: %{version}
Release: %{release}
Group: Applications/Sound
Copyright: GPL

Url: http://sweep.sourceforge.net/

Source: http://download.sourceforge.net/pub/sweep/sweep-%{version}.tar.gz
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root

%description
Sweep is an editor for sound samples. It operates on files of various
formats such as .wav, .aiff and .au, and has multiple undo/redo levels
and filters. 

%prep

%setup

%build
LINGUAS="fr" CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix}
gmake

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install-strip

%files
%defattr(-,root,root)
%doc ABOUT-NLS NEWS README ChangeLog
%{prefix}/bin/sweep
%{prefix}/share/gnome/apps/Multimedia/sweep.desktop
%{prefix}/share/locale/*/*/*

%clean
rm -r $RPM_BUILD_ROOT
