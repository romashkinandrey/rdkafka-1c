# Overlay-порты vcpkg

Подключаются через `overlay-ports` в `vcpkg-configuration.json` и имеют приоритет над портами реестра.

## openssl — 3.5.8 (LTS)

Основа — последний порт ветки 3.5 в реестре vcpkg (OpenSSL 3.5.4, git-tree
`737382595bac2a92c7f8f54f120b53d2637af521` в `versions/o-/openssl.json`). Изменены только версия
(`vcpkg.json`) и SHA512 архива (`portfile.cmake`). Порт из текущего baseline (3.6.x) не подходит:
он собирает цель `build_inst_sw`, которой в Makefile OpenSSL 3.5 нет.

Зачем: в реестре vcpkg ветка 3.5 заканчивается на 3.5.4, дальше идут 3.6.x, поддержка которых
заканчивается 01.11.2026. Ветка 3.5 — LTS (поддержка до 08.04.2030). Версия 3.5.8 (25.08.2026)
закрывает все уязвимости, затрагивающие 3.5.0, в том числе CVE-2025-15467 и CVE-2026-45447.

Обновление до следующего выпуска 3.5.x:
1. поменять `"version"` в `openssl/vcpkg.json`;
2. `curl -fL https://github.com/openssl/openssl/archive/openssl-<версия>.tar.gz | shasum -a 512`
   и вписать хэш в `SHA512` в `openssl/portfile.cmake`;
3. в распакованном архиве проверить, что патчи порта применяются (`patch -p1 --dry-run`), и что после
   `perl Configure linux-x86_64 no-shared` существует цель сборки (`make -n build_sw`).

## cyrus-sasl — заглушка (системная libsasl2)

Нужна для `librdkafka[sasl]` — механизма SASL GSSAPI (Kerberos) на Linux. Порт ничего не собирает:
librdkafka находит системную Cyrus SASL через pkg-config и линкуется с libsasl2 динамически,
всё остальное (OpenSSL, librdkafka, zlib, zstd, lz4, Boost) по-прежнему вшито статически.

- Сборочная машина: `cyrus-sasl-devel` (RHEL/OL: `dnf`) или `libsasl2-dev` (Debian/Ubuntu/Astra: `apt`).
- Библиотека получает зависимость от той libsasl2, с которой собрана: на RHEL/OL 9 — `libsasl2.so.3`,
  на Debian/Ubuntu — `libsasl2.so.2`. Сборка с OL9 загрузится только на RHEL-подобных системах
  (RHEL/OL/Rocky/Alma 9 и новее), для Debian-семейства библиотеку нужно собирать там же.
- Серверы 1С (RHEL/OL 9): пакет `cyrus-sasl-lib` обязателен (без него компонента не загрузится);
  для Kerberos дополнительно `cyrus-sasl-gssapi` и `krb5-workstation`, `/etc/krb5.conf`, keytab.
