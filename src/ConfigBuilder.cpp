#include "ConfigBuilder.h"

#include <algorithm>
#include <cctype>

namespace Kafka1C {

    ConfigBuilder::ConfigBuilder(Loger* Loger, ErrorHandler* Error) {
        loger = Loger;
        error = Error;

        rebalance = new Rebalance(loger);
        event = new Event(loger);
        deliveryReport = new DeliveryReport(loger);

        loger->Info("Create config");
        conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    }

    ConfigBuilder::~ConfigBuilder() {
        delete_pointer(conf);
        delete_pointer(rebalance);
        delete_pointer(event);
        delete_pointer(deliveryReport);
    }

    bool ConfigBuilder::BuildProducerConfig() {
        loger->Info("Build config");

        loger->Debug("Set common props");
        if (loger->level == Loger::Levels::DEBUG)
            if (!SetProperty("debug", "all"))
                return false;

        if (!SetProperty("client.id", "rdkafka-1c"))
            return false;

        loger->Debug("Set user props");
        for (const auto& [key, value] : properties)
            if (!SetProperty(key, value))
                return false;

        loger->Debug("Set callbacks");
        if (!SetEventCb())
            return false;

        if (!SetDeliveryReportCb())
            return false;

        if (loger->level == Loger::Levels::DEBUG)
            LogConfigDump();

        loger->Debug("Clear config properties after build");
        properties.clear();

        return true;
    }

    bool ConfigBuilder::BuildConsumerConfig() {
        loger->Info("Build config");

        loger->Debug("Set common props");
        if (loger->level == Loger::Levels::DEBUG)
            if (!SetProperty("debug", "all"))
                return false;

        if (!SetProperty("client.id", "rdkafka-1c"))
            return false;

        loger->Debug("Set user props");
        for (const auto& [key, value] : properties)
            if (!SetProperty(key, value))
                return false;

        loger->Debug("Set callbacks");
        if (!SetEventCb())
            return false;

        if (!SetRebalanceCb())
            return false;

        if (loger->level == Loger::Levels::DEBUG)
            LogConfigDump();

        loger->Debug("Clear config properties after build");
        properties.clear();

        return true;
    }

    RdKafka::Conf* ConfigBuilder::GetConf() {
        return conf;
    }

    void ConfigBuilder::AddProperty(std::string Name, std::string Value) {
        properties[Name] = Value;
    }

    DeliveryReport* ConfigBuilder::GetDeliveryReport() {
        return deliveryReport;
    }

    /////////////////////////////////////////////////////////////////////////////
    // Support methods

    // Подстроки имён секретных свойств (регистр не важен): sasl.password, ssl.key.password,
    // ssl.keystore.password, ssl.key.pem, sasl.oauthbearer.client.secret, sasl.oauthbearer.config,
    // sasl.oauthbearer.assertion.private.key.pem/.passphrase и т.п.
    // librdkafka помечает секретными ещё sasl.username, ssl.ca.pem, ssl.key.location и
    // *.private.key.file - их не маскируем намеренно: это логин, публичный сертификат и пути к файлам,
    // они нужны для диагностики.
    // sasl.kerberos.kinit.cmd тоже не маскируем: librdkafka в режиме debug сама пишет в лог готовую
    // команду kinit, поэтому секреты в эту настройку вписывать нельзя (см. doc/build.md).
    static const char* const SensitiveMarkers[] = {
        "password", "secret", "passphrase", "key.pem", "oauthbearer.config", "jaas"
    };

    bool IsSensitiveProperty(const std::string& Name) {
        std::string name = Name;
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        for (const char* marker : SensitiveMarkers)
            if (name.find(marker) != std::string::npos)
                return true;

        return false;
    }

    std::string CoverPaswords(const std::string& Name, const std::string& Value) {
        if (Value.empty())
            return Value;

        // Закрытый ключ маскируется по содержимому, под каким бы именем он ни пришёл
        if (IsSensitiveProperty(Name) || Value.find("PRIVATE KEY") != std::string::npos)
            return "***";

        return Value;
    }

    void ConfigBuilder::LogConfigDump() {
        // conf->dump() не скрывает секреты и отдаёт список во владение вызывающему
        std::list<std::string>* dump = conf->dump();
        std::stringstream stream;

        stream << "Config dump:";

        for (auto it = dump->begin(); it != dump->end();) {
            std::string name = std::string(*it);
            it++;
            if (it == dump->end())
                break;
            std::string value = std::string(*it);
            it++;

            stream << std::endl << name << " = " << CoverPaswords(name, value);
        }
        delete dump;

        loger->Debug(stream.str());
    }

    bool ConfigBuilder::SetProperty(std::string Name, std::string Value) {
        loger->Debug("Set config property '" + Name + "' in value '" + CoverPaswords(Name, Value) + "'");

        std::string errorDescription;
        RdKafka::Conf::ConfResult result = conf->set(Name, Value, errorDescription);
        if (result != RdKafka::Conf::CONF_OK) {
            loger->Error("Failed to set config property: " + errorDescription);
            return false;
        }

        return true;
    }

    bool ConfigBuilder::SetEventCb() {
        if (!event)
            return true;

        loger->Debug("Set config property 'event_cb'");

        std::string errorDescription;
        RdKafka::Conf::ConfResult result = conf->set("event_cb", event, errorDescription);
        if (result != RdKafka::Conf::CONF_OK) {
            loger->Error("Failed to set config property 'event_cb': " + errorDescription);
            return false;
        }

        return true;
    }

    bool ConfigBuilder::SetDeliveryReportCb() {
        if (!deliveryReport)
            return true;

        loger->Debug("Set config property 'dr_cb'");

        std::string errorDescription;
        RdKafka::Conf::ConfResult result = conf->set("dr_cb", deliveryReport, errorDescription);
        if (result != RdKafka::Conf::CONF_OK) {
            loger->Error("Failed to set config property 'dr_cb': " + errorDescription);
            return false;
        }

        return true;
    }

    bool ConfigBuilder::SetRebalanceCb() {
        if (!rebalance)
            return true;

        loger->Debug("Set config property 'rebalance_cb'");

        std::string errorDescription;
        RdKafka::Conf::ConfResult result = conf->set("rebalance_cb", rebalance, errorDescription);
        if (result != RdKafka::Conf::CONF_OK) {
            loger->Error("Failed to set config property 'rebalance_cb': " + errorDescription);
            return false;
        }

        return true;
    }
} // namespace RdKafka1C