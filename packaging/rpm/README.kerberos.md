# rdkafka-1c-kerberos — Kerberos (SASL GSSAPI) для RdKafka1C

Метапакет ставит то, что нужно компоненте для аутентификации в Kafka через Kerberos:

- `cyrus-sasl-gssapi` — модуль GSSAPI для Cyrus SASL;
- `krb5-workstation` — клиент MIT Kerberos (команда `kinit`, которой librdkafka обновляет билеты).

Настроить отдельно:

1. `/etc/krb5.conf` — realm и KDC.
2. keytab клиента, доступный на чтение пользователю службы 1С (например, `usr1cv8`, права 0400).
3. Параметры компоненты (`УстановитьПараметр`):

   ```
   security.protocol          = sasl_plaintext   (или sasl_ssl)
   sasl.mechanisms            = GSSAPI
   sasl.kerberos.service.name = kafka
   sasl.kerberos.principal    = <клиент>@<REALM>
   sasl.kerberos.keytab       = /путь/к/client.keytab
   ```

   Без `kinit`: `sasl.kerberos.min.time.before.relogin = 0` и переменная окружения
   `KRB5_CLIENT_KTNAME=/путь/к/client.keytab` у службы 1С — билет берётся из keytab автоматически.
   В этом режиме `sasl.kerberos.principal` и `sasl.kerberos.keytab` не используются: принципал берётся
   из keytab, поэтому в нём должен быть только клиентский принципал. Переменную службе 1С можно задать
   drop-in файлом systemd, например `systemctl edit srv1cv8-8.3.xx.xxxx@default` →
   `[Service]` / `Environment=KRB5_CLIENT_KTNAME=/путь/к/client.keytab`, затем перезапуск службы.

Имя брокера в `bootstrap.servers` и `advertised.listeners` должно совпадать с именем хоста
в принципале брокера (`kafka/<fqdn>@REALM`).

В `sasl.kerberos.kinit.cmd` не вписывайте пароли и другие секреты: при уровне логирования `debug`
librdkafka пишет в лог компоненты готовую команду kinit.
