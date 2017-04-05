%define name discodb
%define version 0.5
%define unmangled_version 0.5
%define release 1

Summary: An efficient, immutable, persistent mapping object.
Name: %{name}
Version: %{version}
Release: %{release}%{?dist}
Source0: %{name}-%{unmangled_version}.tar.gz
License: BSD
Group: Development/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix: %{_prefix}
Vendor: Nokia Research Center <UNKNOWN>
BuildRequires:libdiscodb-devel
Requires: libdiscodb


%description
An efficient, immutable, persistent mapping object.

See documentation at http://discodb.readthedocs.org

%prep
%setup -n %{name}-%{unmangled_version}

%build
env CFLAGS="$RPM_OPT_FLAGS" python setup.py build

%install
python setup.py install -O1 --root=$RPM_BUILD_ROOT --record=INSTALLED_FILES

%clean
rm -rf $RPM_BUILD_ROOT

%files -f INSTALLED_FILES
%defattr(-,root,root)
