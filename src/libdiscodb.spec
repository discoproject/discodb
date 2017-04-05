Name: libdiscodb
Version: 0.3.1
Release: 1%{?dist}.db
Summary: c library and headers for discodb

Group:	 bauman
License: 3-Clause BSD
URL:	 https://github.com/discoproject/discodb
Source0: ddb_delta.c
Source1: ddb_deltalist.c
Source2: ddb_membuffer.c
Source3: ddb_queue.c
Source4: ddb.c
Source5: ddb_cmph.c
Source6: ddb_cnf.c
Source7: ddb_cons.c
Source8: ddb_huffman.c
Source9: ddb_list.c
Source10: ddb_map.c
Source11: ddb_view.c
Source12: ddb_bits.h
Source13: ddb_cmph.h
Source14: ddb_delta.h
Source15: ddb_deltalist.h
Source16: ddb_hash.h
Source17: ddb_huffman.h
Source18: ddb_membuffer.h
Source19: ddb_profile.h
Source20: ddb_queue.h
Source21: ddb_types.h
Source22: ddb_internal.h
Source23: ddb_list.h
Source24: ddb_map.h
Source25: discodb.h
Source26: DISCODB_LICENSING.txt


BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
BuildRequires: gcc, cmph-devel
Requires: cmph
Provides: libdiscodb.so()(64bit)

%description
Provides a shared object which can be used to perform mongo-like queries against BSON data.
See website for examples.

%package devel
Summary: Development files for libbsoncompare
Requires: libdiscodb == %{version}-%{release}, cmph-devel
Group: Development/Libraries


%description devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.


%prep
cp -fp %{SOURCE12} ./
cp -fp %{SOURCE13} ./
cp -fp %{SOURCE14} ./
cp -fp %{SOURCE15} ./
cp -fp %{SOURCE16} ./
cp -fp %{SOURCE17} ./
cp -fp %{SOURCE18} ./
cp -fp %{SOURCE19} ./
cp -fp %{SOURCE20} ./
cp -fp %{SOURCE21} ./
cp -fp %{SOURCE22} ./
cp -fp %{SOURCE23} ./
cp -fp %{SOURCE24} ./
cp -fp %{SOURCE25} ./
cp -fp %{SOURCE26} ./

#%setup -q

%build
#rm -rf %{buildroot}
mkdir -p %{buildroot}
gcc %optflags -I$RPM_BUILD_DIR -lcmph -shared -o $RPM_BUILD_DIR/libdiscodb.so -fPIC %{SOURCE0} %{SOURCE1} %{SOURCE2} %{SOURCE3} %{SOURCE4} %{SOURCE5} %{SOURCE6} %{SOURCE7} %{SOURCE8} %{SOURCE9} %{SOURCE10} %{SOURCE11}

%install
mkdir -p $RPM_BUILD_ROOT/%{_usr}/%{_lib}
install -m 644 -p $RPM_BUILD_DIR/libdiscodb.so $RPM_BUILD_ROOT/%{_usr}/%{_lib}/libdiscodb.so

mkdir -p $RPM_BUILD_ROOT/%{_includedir}
install -m 644 -p $RPM_BUILD_DIR/ddb_bits.h $RPM_BUILD_ROOT/%{_includedir}/ddb_bits.h
install -m 644 -p $RPM_BUILD_DIR/ddb_cmph.h $RPM_BUILD_ROOT/%{_includedir}/ddb_cmph.h
install -m 644 -p $RPM_BUILD_DIR/ddb_delta.h $RPM_BUILD_ROOT/%{_includedir}/ddb_delta.h
install -m 644 -p $RPM_BUILD_DIR/ddb_deltalist.h $RPM_BUILD_ROOT/%{_includedir}/ddb_deltalist.h
install -m 644 -p $RPM_BUILD_DIR/ddb_hash.h $RPM_BUILD_ROOT/%{_includedir}/ddb_hash.h
install -m 644 -p $RPM_BUILD_DIR/ddb_huffman.h $RPM_BUILD_ROOT/%{_includedir}/ddb_huffman.h
install -m 644 -p $RPM_BUILD_DIR/ddb_membuffer.h $RPM_BUILD_ROOT/%{_includedir}/ddb_membuffer.h
install -m 644 -p $RPM_BUILD_DIR/ddb_profile.h $RPM_BUILD_ROOT/%{_includedir}/ddb_profile.h
install -m 644 -p $RPM_BUILD_DIR/ddb_queue.h $RPM_BUILD_ROOT/%{_includedir}/ddb_queue.h
install -m 644 -p $RPM_BUILD_DIR/ddb_types.h $RPM_BUILD_ROOT/%{_includedir}/ddb_types.h
install -m 644 -p $RPM_BUILD_DIR/ddb_internal.h $RPM_BUILD_ROOT/%{_includedir}/ddb_internal.h
install -m 644 -p $RPM_BUILD_DIR/ddb_list.h $RPM_BUILD_ROOT/%{_includedir}/ddb_list.h
install -m 644 -p $RPM_BUILD_DIR/ddb_map.h $RPM_BUILD_ROOT/%{_includedir}/ddb_map.h
install -m 644 -p $RPM_BUILD_DIR/discodb.h $RPM_BUILD_ROOT/%{_includedir}/discodb.h


mkdir -p $RPM_BUILD_ROOT/%{_docdir}/%{name}
install -m 644 -p $RPM_BUILD_DIR/DISCODB_LICENSING.txt $RPM_BUILD_ROOT/%{_docdir}/%{name}/LICENSING.txt

%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%{_usr}/%{_lib}/libdiscodb.so
%doc
%{_docdir}/%{name}/LICENSING.txt
%files devel
%{_includedir}/*.h

%changelog

