# RPM внешней компоненты RdKafka1C для 1С:Предприятия (Linux x86_64, RHEL/OL 9+).
# Пакет собирается из готовых артефактов (libRdKafka1C.so и RdKafka1C.zip) скриптом
# packaging/rpm/build-rpm.sh, который передаёт версию и выпуск через --define "rdk_version X.Y.Z"
# и --define "rdk_release N" (выпуск повышается при пересборке той же версии).

%{!?rdk_version: %global rdk_version 1.3.1.1}
%{!?rdk_release: %global rdk_release 1}

# Бинарник уже собран в Release: не пересобираем debuginfo и не трогаем .so (strip изменил бы
# файл, и он перестал бы совпадать с libRdKafka1C.so внутри RdKafka1C.zip)
%global debug_package %{nil}
%global __strip /bin/true
%global _build_id_links none

%global rdk_dir /opt/rdkafka-1c

Name:           rdkafka-1c
Version:        %{rdk_version}
Release:        %{rdk_release}%{?dist}
Summary:        1C:Enterprise add-in for Apache Kafka (librdkafka)
# Компонента - ASL 2.0; вшиты OpenSSL (ASL 2.0), librdkafka, lz4, zstd (BSD), Boost (Boost), zlib (zlib)
License:        ASL 2.0 and BSD and Boost and zlib
URL:            https://github.com/romashkinandrey/rdkafka-1c
Source0:        libRdKafka1C.so
Source1:        RdKafka1C.zip
Source2:        LICENSE
Source3:        README.rpm.md
Source4:        README.kerberos.md
# Лицензии вшитых библиотек (из vcpkg), собирает build-rpm.sh
Source5:        third-party-licenses.tar.gz
ExclusiveArch:  x86_64

# Зависимости от библиотек (libsasl2.so.3, libstdc++ GLIBCXX_3.4.29, glibc 2.34) rpm находит сам
# по ELF-заголовкам. Пакет с libsasl2 указываем и явно: без него компонента не загрузится.
Requires:       cyrus-sasl-lib%{?_isa}
# Компонента переключает процесс на локаль ru_RU (setlocale) и через неё переводит строки из 1С
# (id сообщений, топики, параметры): без локали кириллица превращается в пустые строки.
# Локаль ru_RU однобайтовая (ISO-8859-5): её конвертер лежит в glibc-gconv-extra, без него то же самое
Requires:       (glibc-langpack-ru or glibc-all-langpacks)
Requires:       glibc-gconv-extra%{?_isa}

%description
RdKafka1C is a native (Native API) add-in for 1C:Enterprise that exchanges
messages with Apache Kafka through librdkafka.

The package installs to %{rdk_dir}:
- libRdKafka1C.so - the Linux library of the add-in;
- RdKafka1C.zip   - the add-in package for 1C (Linux and Windows builds,
  MANIFEST.XML), usable with AttachAddIn from a file.

OpenSSL, librdkafka, Boost, lz4, zstd and zlib are linked
statically; the only external library besides glibc/libstdc++ is the system
Cyrus SASL (libsasl2.so.3). The ru_RU locale (glibc-langpack-ru) and the ISO-8859-5
converter (glibc-gconv-extra) are required: the add-in converts strings
from 1C through this locale. Kerberos (SASL GSSAPI)
support needs the %{name}-kerberos package.

%package kerberos
Summary:        Kerberos (SASL GSSAPI) runtime dependencies for %{name}
BuildArch:      noarch
Requires:       %{name} = %{version}-%{release}
Requires:       cyrus-sasl-gssapi
Requires:       krb5-workstation

%description kerberos
Meta package that pulls in what RdKafka1C needs to authenticate to Kafka with
Kerberos (sasl.mechanisms=GSSAPI): the Cyrus SASL GSSAPI plugin and the MIT
Kerberos client tools (kinit, used by librdkafka to refresh tickets).
/etc/krb5.conf and a keytab must be configured separately; see
README.kerberos.md.

%prep
# Готовые артефакты: распаковывать нечего

%build
# Сборка выполнена заранее (CMake + vcpkg, Release)

%install
install -d -m 0755 %{buildroot}%{rdk_dir}
install -p -m 0755 %{SOURCE0} %{buildroot}%{rdk_dir}/libRdKafka1C.so
install -p -m 0644 %{SOURCE1} %{buildroot}%{rdk_dir}/RdKafka1C.zip
install -d -m 0755 %{buildroot}%{_docdir}/%{name} %{buildroot}%{_docdir}/%{name}-kerberos %{buildroot}%{_licensedir}/%{name}
install -p -m 0644 %{SOURCE3} %{buildroot}%{_docdir}/%{name}/README.md
install -p -m 0644 %{SOURCE4} %{buildroot}%{_docdir}/%{name}-kerberos/README.kerberos.md
install -p -m 0644 %{SOURCE2} %{buildroot}%{_licensedir}/%{name}/LICENSE
install -d -m 0755 %{buildroot}%{_licensedir}/%{name}/third-party
tar -xzf %{SOURCE5} -C %{buildroot}%{_licensedir}/%{name}/third-party
chmod 0644 %{buildroot}%{_licensedir}/%{name}/third-party/*

%files
%dir %{rdk_dir}
%{rdk_dir}/libRdKafka1C.so
%{rdk_dir}/RdKafka1C.zip
%dir %{_licensedir}/%{name}
%license %{_licensedir}/%{name}/LICENSE
%license %{_licensedir}/%{name}/third-party
%dir %{_docdir}/%{name}
%doc %{_docdir}/%{name}/README.md

%files kerberos
%dir %{_docdir}/%{name}-kerberos
%doc %{_docdir}/%{name}-kerberos/README.kerberos.md

%changelog
* Sat Sep 19 2026 Andrey Romashkin <romashk.andrey@gmail.com> - 1.3.1.1-1
- Fork build 1 of upstream rdkafka-1c 1.3.1 (see FORK.md)
- OpenSSL 3.5.8, librdkafka 2.15.1 (ssl, sasl/GSSAPI, zlib, zstd, lz4)
- Secrets are masked in the add-in debug log
- Symbols of statically linked libraries are no longer exported
- Initial RPM packaging: rdkafka-1c and rdkafka-1c-kerberos
- Requires the ru_RU locale (glibc-langpack-ru) and glibc-gconv-extra (ISO-8859-5)
