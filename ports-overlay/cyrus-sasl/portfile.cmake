# Заглушка порта cyrus-sasl.
# Cyrus SASL не собирается из vcpkg: librdkafka (feature "sasl") находит системную
# библиотеку через pkg-config (libsasl2.pc) и линкуется с ней динамически (RHEL/OL 9: libsasl2.so.3).
# Так же поступают официальные Linux-сборки librdkafka: механизмы SASL (в т.ч. GSSAPI)
# Cyrus загружает плагинами из /usr/lib64/sasl2, поэтому статическая сборка неудобна.
# На сборочной машине нужен cyrus-sasl-devel (RHEL/OL) или libsasl2-dev (Debian/Ubuntu/Astra),
# на серверах 1С - cyrus-sasl-lib (и cyrus-sasl-gssapi для Kerberos).

if(NOT EXISTS "/usr/include/sasl/sasl.h")
    message(FATAL_ERROR "Не найден /usr/include/sasl/sasl.h: установите cyrus-sasl-devel (RHEL/OL: dnf install cyrus-sasl-devel) или libsasl2-dev (Debian/Ubuntu/Astra: apt install libsasl2-dev)")
endif()

find_program(PKGCONFIG_BIN NAMES pkg-config pkgconf)
if(PKGCONFIG_BIN)
    execute_process(
        COMMAND "${PKGCONFIG_BIN}" --modversion libsasl2
        OUTPUT_VARIABLE SYSTEM_SASL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE SYSTEM_SASL_RESULT
    )
    if(NOT SYSTEM_SASL_RESULT EQUAL 0)
        message(FATAL_ERROR "pkg-config не видит libsasl2.pc: установите cyrus-sasl-devel (RHEL/OL) или libsasl2-dev (Debian/Ubuntu/Astra)")
    endif()
    message(STATUS "Используется системный Cyrus SASL ${SYSTEM_SASL_VERSION}")
endif()

set(VCPKG_POLICY_EMPTY_PACKAGE enabled)
