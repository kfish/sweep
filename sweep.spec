%define name sweep
%define version 0.1.0
%define release 1
%define prefix /usr

Summary: Sound wave editor

Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Applications/Sound
URL: http://sweep.sourceforge.net/

Source: http://download.sourceforge.net/pub/sweep/sweep-%{version}.tar.gz
Buildroot: %{_tmppath}/%{name}-%{version}-%{release}-root
Docdir: %{prefix}/doc
Prefix: %{prefix}
Requires: gtk+ >= 1.2.0

%description
Sweep is an editor for sound samples. It operates on files of various
formats such as .wav, .aiff and .au, and has multiple undo/redo levels
and filters. It supports audio filter plugins from the LADSPA project.


%package devel
Summary: Sweep plugin development kit
Group: Applications/Sound
%description devel
The sweep-devel package contains header files and documentation for writing
plugins for Sweep, a sound wave editor.

Install sweep-devel if you're going to create plugins for Sweep. You will
also need to install sweep.

%prep

%setup

%build
LINGUAS="fr hu it de" CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix}
gmake

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install-strip

%files

%defattr (0444, bin, bin, 0555)
%dir %{prefix}/lib/sweep

%defattr (0555, bin, bin)
%{prefix}/bin/sweep
%{prefix}/lib/sweep/libladspameta.so.1.0.0
%{prefix}/lib/sweep/libnormalise.so.1.0.0
%{prefix}/lib/sweep/libecho.so.1.0.0
%{prefix}/lib/sweep/libreverse.so.1.0.0
%{prefix}/lib/sweep/libbyenergy.so.1.0.0

%defattr (0555, bin, man)
%{prefix}/man/man1/sweep.1*
%doc ABOUT-NLS NEWS README ChangeLog
%doc README.Solaris README.ALSA
%doc doc/*.txt

%{prefix}/share/gnome/apps/Multimedia/sweep.desktop
%{prefix}/share/locale/*/*/*

%files devel
%doc doc/plugin_writers_guide.txt
%{prefix}/include/sweep/

%clean
rm -r $RPM_BUILD_ROOT

%changelog
* Sun Oct 08 2000 Conrad Parker <conrad@vergenet.net>
- updated for sweep version 0.1.0
- added devel package
- added packaging of plugins
- added documentation
