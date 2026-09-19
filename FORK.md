# Форк rdkafka-1c: что изменено и зачем

Основа — [sv-sand/rdkafka-1c](https://github.com/sv-sand/rdkafka-1c) версии 1.3.1. Форк:
[romashkinandrey/rdkafka-1c](https://github.com/romashkinandrey/rdkafka-1c).

API компоненты не менялся: обновлены уязвимые зависимости (в библиотеках для Linux и Windows), закрыты
утечки секретов, добавлены сборка под Oracle Linux 9 и RPM-пакеты.

## Нумерация версий

Версия форка — это версия апстрима плюс номер сборки: `1.3.1.1`, следующая — `1.3.1.2` и т. д.
Если форк переедет на новую версию апстрима, например 1.4.0, нумерация начнётся с `1.4.0.1`.

Где записан номер версии:

- `src/AddInNative.h` (`COMPONENT_VERSION`), отсюда он попадает в `.so` и `.dll`;
- `package/INFO.XML` и `INFO.XML` внутри `package/RdKafka1C.zip` и макета `Template.bin`;
- `cf/Configuration.xml`, `README.md`;
- в тесте `test/AddInNativeTest.cpp`;
- в `packaging/rpm/rdkafka-1c.spec` (версия по умолчанию и changelog).

`packaging/rpm/build-rpm.sh` сверяет версию из кода с `INFO.XML` в zip, поэтому при смене версии
zip и макет нужно пересобрать.

| Сборка | Что вошло |
|---|---|
| 1.3.1.1 | Linux-библиотека: зависимости, маскирование секретов, RPM; правка BSL — для всех ОС; Windows DLL оставалась от апстрима |
| 1.3.1.2 | Windows DLL пересобрана из кода форка: OpenSSL 3.5.8, librdkafka 2.15.1 с zlib и zstd, маскирование секретов; код компоненты не менялся |

## Ветки

| Ветка | Что в ней |
|---|---|
| `main` | апстрим без изменений, чтобы было с чем сравнивать и откуда подтягивать обновления |
| `security/deps-secrets-2026-09` | все изменения форка (описаны ниже) |

## 1. Уязвимые библиотеки внутри `.so` и `.dll`

**Проблема.** OpenSSL 3.5.0 (от 08.04.2025) и librdkafka 2.10.0 вшиты в `libRdKafka1C.so` и `RdKafka1C.dll` статически.
Обновление пакетов ОС их не затрагивает, а сканер уязвимостей на хосте их не видит.

**Что сделано:**

- **OpenSSL 3.5.0 → 3.5.8** (ветка LTS, выпуск от 25.08.2026). Закрыты уязвимости 3.5.0, в том числе
  уровня High: CVE-2025-15467 (разбор CMS AuthEnvelopedData) и CVE-2026-45447 (use-after-free в
  PKCS7_verify).
  - В реестре vcpkg ветка 3.5 заканчивается на 3.5.4, поэтому версия задана overlay-портом
    `ports-overlay/openssl`. Как обновлять его, описано в [ports-overlay/README.md](ports-overlay/README.md).
  - Ветку 3.6 не взяли: её поддержка заканчивается 01.11.2026, а у 3.5 — 08.04.2030.
- **librdkafka 2.10.0 → 2.15.1.** В 2.10.0 есть регрессия: кэш метаданных консюмера обновляется
  без блокировок, и при потере связи с брокером портится память (librdkafka #5066, исправлено в 2.10.1).
  CVE у неё нет, поэтому сканеры её не показывают.
- **Baseline vcpkg `f45c0ed` → `66c2e79`** (Boost 1.92, gtest 1.18). Удалён неиспользуемый реестр `artifact`.
- **В librdkafka включены `zlib`, `zstd` и `sasl`** (последний — только для Linux).
  - Старая сборка не читала сообщения, которые другие продюсеры сжимали gzip или zstd:
    на стенде она прочитала 0 сообщений из 10.
  - Механизм SASL GSSAPI (Kerberos) не был включён в сборку для Linux (в Windows DLL он работает через SSPI).
  - Cyrus SASL берётся из системы через overlay-заглушку `ports-overlay/cyrus-sasl`, как в официальных
    сборках librdkafka. Поэтому библиотека зависит от `libsasl2.so.3` из пакета `cyrus-sasl-lib`
    и работает только на RHEL-подобных системах версии 9 и новее.
- **Флаг `-Wl,--exclude-libs,ALL`** (Linux). Поставлявшаяся `.so` экспортировала 14 011 символов,
  из них около 9 000 — символы OpenSSL. У новой 323 экспорта, OpenSSL среди них нет.
  - Если в процесс 1С была загружена системная libcrypto (например, для Kerberos), вызовы OpenSSL
    из компоненты связывались с системной копией вместо вшитой.
  - На стенде: у старой сборки было 3 281 такое связывание с системными libssl/libcrypto, у новой — 0.
- **Сборка Linux в Release** (`-DCMAKE_BUILD_TYPE=Release` в `build.sh`, `build-tests.sh`,
  `generate.sh`). Генератор Unix Makefiles игнорирует `--config Release`, поэтому раньше собиралась
  отладочная библиотека с отладочными вариантами зависимостей, в том числе с отладочной OpenSSL.

## 2. Секреты в журналах

- **BSL, `КафкаКоннектор.УстановитьПараметр`.** Значение параметра, в том числе `sasl.password`,
  подставлялось в текст исключения. Оттуда пароль мог попасть в журнал регистрации, на экран
  пользователя и в технологический журнал.
  - Теперь в тексте остаются только имя параметра и описание ошибки.
  - В версии 1.3.1 компонента возвращает `Отказ` только для значения не строкового типа, а обработка
    передаёт строки, поэтому на практике эта ветка не срабатывает. Правка сделана на будущее.
- **C++, debug-лог компоненты.** Раньше маскировался только `sasl.password`. `ssl.key.password`,
  `ssl.keystore.password`, `ssl.key.pem`, `sasl.oauthbearer.config` и другие секреты писались
  открытым текстом.
  - Теперь значения скрываются по подстроке в имени свойства: `password`, `secret`, `passphrase`,
    `key.pem`, `oauthbearer.config`, `jaas`.
  - Независимо от имени скрывается любое значение, в котором есть `PRIVATE KEY`.
  - На стенде: в debug-логе старой сборки было 12 совпадений с секретами, у новой — 0.
  - Добавлены тесты `test/ConfigBuilderTest.cpp`.
- **`LogConfigDump`.** Список `conf->dump()` с копиями секретов не освобождался, а при нечётном числе
  элементов цикл выходил за конец списка. Исправлено.
- **Что намеренно не маскируется.** `sasl.username`, `ssl.ca.pem` и пути к файлам ключей нужны для
  диагностики. Команду из `sasl.kerberos.kinit.cmd` librdkafka сама пишет в debug-лог, поэтому
  вписывать секреты в эту настройку нельзя.

## 3. Поставка

- **`package/RdKafka1C.zip` и макет обработки `КафкаКоннектор`**
  (`cf/DataProcessors/КафкаКоннектор/Templates/RdKafka1C/Ext/Template.bin`). В обоих заменены
  библиотеки для Linux и Windows. Обработка `КафкаКоннектор` подключает компоненту из макета,
  поэтому обновлять нужно и его.
  - `RdKafka1C.dll` пересобрана в сборке 1.3.1.2 (Windows 11, MSVC 14.44, триплет `x64-windows-static-md`):
    OpenSSL 3.5.8 и librdkafka 2.15.1 вместо 3.5.0 и 2.10.0, добавлены zlib и zstd, вошли исправления
    форка на C++ (маскирование секретов, `LogConfigDump`). Системные DLL в зависимостях те же, экспорт —
    те же 533 символа по числу и те же функции Native API, символов OpenSSL и librdkafka нет.
    Kerberos на Windows librdkafka реализует через SSPI.
- **RPM для RHEL и Oracle Linux 9+** (`package/rpm/`, исходники в `packaging/rpm/`). Компонента
  ставится в `/opt/rdkafka-1c/`, README и лицензии — в `/usr/share/doc` и `/usr/share/licenses`.

  | Пакет | Что ставит | Зависимости |
  |---|---|---|
  | `rdkafka-1c` (x86_64) | `libRdKafka1C.so` и `RdKafka1C.zip`, лицензии вшитых библиотек | `cyrus-sasl-lib`, локаль `ru_RU` (`glibc-langpack-ru` или `glibc-all-langpacks`), `glibc-gconv-extra` |
  | `rdkafka-1c-kerberos` (noarch) | метапакет для Kerberos, только README | `rdkafka-1c` той же версии, `cyrus-sasl-gssapi`, `krb5-workstation` |

  Локаль и конвертер ISO-8859-5 обязательны. Компонента переключает процесс на `ru_RU` и через неё
  перекодирует строки из 1С. Без них кириллические идентификаторы сообщений и имена топиков
  становятся пустыми: сообщения уходят, но `СтатусСообщения` возвращает `NOT_PERSISTED`.
- **Документация.** Сборка на Oracle Linux 9 и под Windows, требования к серверу 1С и сборка RPM
  описаны в [doc/build.md](doc/build.md). Подключение и Kerberos — в
  [packaging/rpm/README.rpm.md](packaging/rpm/README.rpm.md) и
  [packaging/rpm/README.kerberos.md](packaging/rpm/README.kerberos.md).

## Как проверено (сборка 1.3.1.2)

Стенд — виртуальные машины Oracle Linux 9.8 (GCC 11.5), Oracle Linux 10.2 и Windows 11 24H2
(MSVC 14.44), Apache Kafka 4.3.1 с собственными CA и KDC.

- [x] gtest: 48 из 48 на Linux и на Windows (45 тестов апстрима и 3 новых).
- [x] Linux: отправка и чтение сообщений через компоненту по PLAINTEXT, SSL (по имени и по IP),
  SASL_SSL/SCRAM-SHA-512, SASL GSSAPI (с kinit по keytab и без kinit, через `KRB5_CLIENT_KTNAME`).
- [x] Windows: отправка и чтение по PLAINTEXT, SSL и SASL_SSL/SCRAM-SHA-512.
- [x] Негативные сценарии: чужой CA, недоступный брокер (отправка и консюмер) — на Linux и Windows,
  чужой keytab — на Linux. Компонента возвращает ошибку, процесс не падает.
- [x] Пароль SCRAM не попадает в debug-лог компоненты на Linux и Windows.
- [x] Windows: чтение сообщений, сжатых gzip, zstd и lz4, и отправка со сжатием gzip и zstd
  (DLL апстрима gzip и zstd не читала: 0 из 10); в debug-логе нет секретов (у DLL апстрима — 12).
- [x] RPM ставятся с зависимостями на чистые Oracle Linux 9.8 и 10.2, после установки весь набор
  проверок проходит.
- [x] Linux, на сборке 1.3.1.1 (код тот же): чтение сообщений, сжатых gzip, zstd и lz4, отправка
  со сжатием gzip и zstd; в debug-логе нет секретов; вызовы OpenSSL не связываются с системными
  libssl/libcrypto.
- [ ] Работа внутри платформы 1С: стенда с 1С не было.
- [ ] Kerberos на Windows (SSPI): нужна учётная запись Windows с билетом Kerberos — домен Active Directory
  или привязка к MIT KDC через `ksetup`; на стенде не проверялось.

## Связанный форк

[romashkinandrey/Simple-Kafka_Adapter](https://github.com/romashkinandrey/Simple-Kafka_Adapter),
ветка `security/schema-registry-url`.

Там закрыты чтение локальных файлов и SSRF через адрес Schema Registry:

- libcurl разрешены только протоколы http и https;
- адрес проверяется перед запросом;
- `subject` и `version` URL-кодируются.

---

Скуф Технолоджи, 19.09.2026
