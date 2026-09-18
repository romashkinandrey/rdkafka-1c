#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "ConfigBuilder.h"

namespace Kafka1C {

	// Секретные свойства определяются по имени, пути к файлам и публичные сертификаты - нет
	TEST(ConfigBuilderTest, IsSensitiveProperty)
	{
		for (const char* name : {
			"sasl.password",
			"SASL.PASSWORD",
			"ssl.key.password",
			"ssl.keystore.password",
			"ssl.key.pem",
			"sasl.oauthbearer.client.secret",
			"sasl.oauthbearer.config",
			"sasl.oauthbearer.assertion.private.key.pem",
			"sasl.oauthbearer.assertion.private.key.passphrase",
			"sasl.jaas.config" })
			EXPECT_TRUE(IsSensitiveProperty(name)) << name;

		for (const char* name : {
			"bootstrap.servers",
			"security.protocol",
			"sasl.mechanisms",
			"sasl.username",
			"sasl.kerberos.keytab",
			"sasl.kerberos.principal",
			"sasl.kerberos.kinit.cmd",
			"ssl.key.location",
			"ssl.ca.location",
			"ssl.ca.pem",
			"ssl.certificate.pem" })
			EXPECT_FALSE(IsSensitiveProperty(name)) << name;
	}

	// Закрытый ключ скрывается по содержимому под любым именем, публичные значения остаются как есть
	TEST(ConfigBuilderTest, CoverPaswords)
	{
		EXPECT_EQ("***", CoverPaswords("sasl.password", "S3cr3t"));
		EXPECT_EQ("***", CoverPaswords("ssl.certificate.pem", "-----BEGIN PRIVATE KEY-----\nMII...\n-----END PRIVATE KEY-----"));
		EXPECT_EQ("***", CoverPaswords("ssl.ca.pem", "-----BEGIN ENCRYPTED PRIVATE KEY-----"));
		EXPECT_EQ("-----BEGIN CERTIFICATE-----", CoverPaswords("ssl.ca.pem", "-----BEGIN CERTIFICATE-----"));
		EXPECT_EQ("localhost:9092", CoverPaswords("bootstrap.servers", "localhost:9092"));
		EXPECT_EQ("", CoverPaswords("sasl.password", ""));
	}

	// Ни установка свойств, ни дамп конфигурации не должны выводить секреты в debug-лог
	TEST(ConfigBuilderTest, SecretsAreMaskedInDebugLog)
	{
		std::string logFile = (std::filesystem::temp_directory_path() / "rdkafka-1c-config-builder-test.log").string();

		{
			Loger loger;
			loger.level = Loger::Levels::DEBUG;
			ASSERT_TRUE(loger.Init(logFile));

			ErrorHandler error(&loger);
			ConfigBuilder builder(&loger, &error);

			builder.AddProperty("bootstrap.servers", "localhost:9092");
			builder.AddProperty("security.protocol", "sasl_ssl");
			builder.AddProperty("sasl.mechanisms", "PLAIN");
			builder.AddProperty("sasl.username", "user-visible");
			builder.AddProperty("sasl.password", "S3cr3t-1");
			builder.AddProperty("ssl.key.password", "S3cr3t-2");
			builder.AddProperty("ssl.keystore.password", "S3cr3t-3");
			builder.AddProperty("ssl.key.pem", "-----BEGIN PRIVATE KEY-----S3cr3t-4-----END PRIVATE KEY-----");
			builder.AddProperty("sasl.oauthbearer.config", "principal=S3cr3t-5");

			ASSERT_TRUE(builder.BuildProducerConfig());
		}

		std::ifstream file(logFile);
		std::stringstream content;
		content << file.rdbuf();
		file.close();
		std::filesystem::remove(logFile);

		std::string log = content.str();
		EXPECT_EQ(std::string::npos, log.find("S3cr3t")) << log;
		EXPECT_NE(std::string::npos, log.find("Config dump:"));
		EXPECT_NE(std::string::npos, log.find("sasl.password = ***"));
		EXPECT_NE(std::string::npos, log.find("localhost:9092"));
		EXPECT_NE(std::string::npos, log.find("user-visible"));
	}

} // namespace Kafka1C
