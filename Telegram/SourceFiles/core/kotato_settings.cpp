/*
This file is part of Kotatogram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/kotatogram/kotatogram-desktop/blob/dev/LEGAL
*/
#include "core/kotato_settings.h"

#include "mainwindow.h"
#include "mainwidget.h"
#include "window/window_controller.h"
#include "core/application.h"
#include "base/parse_helper.h"
#include "facades.h"
#include "ui/widgets/input_fields.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QTimer>

namespace KotatoSettings {
namespace {

constexpr auto kWriteJsonTimeout = crl::time(5000);

QString DefaultFilePath() {
	return cWorkingDir() + qsl("tdata/kotato-settings-default.json");
}

QString CustomFilePath() {
	return cWorkingDir() + qsl("tdata/kotato-settings-custom.json");
}

bool DefaultFileIsValid() {
	QFile file(DefaultFilePath());
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(
		base::parse::stripComments(file.readAll()),
		&error);
	file.close();

	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return false;
	}
	const auto settings = document.object();

	const auto version = settings.constFind(qsl("version"));
	if (version == settings.constEnd() || (*version).toInt() != AppKotatoVersion) {
		return false;
	}

	return true;
}

void WriteDefaultCustomFile() {
	const auto path = CustomFilePath();
	auto input = QFile(":/misc/default_kotato-settings-custom.json");
	auto output = QFile(path);
	if (input.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly)) {
		output.write(input.readAll());
	}
}

bool ReadOption(QJsonObject obj, QString key, std::function<void(QJsonValue)> callback) {
	const auto it = obj.constFind(key);
	if (it == obj.constEnd()) {
		return false;
	}
	callback(*it);
	return true;
}

bool ReadObjectOption(QJsonObject obj, QString key, std::function<void(QJsonObject)> callback) {
	auto readResult = false;
	auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
		if (v.isObject()) {
			callback(v.toObject());
			readResult = true;
		}
	});
	return (readValueResult && readResult);
}

bool ReadArrayOption(QJsonObject obj, QString key, std::function<void(QJsonArray)> callback) {
	auto readResult = false;
	auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
		if (v.isArray()) {
			callback(v.toArray());
			readResult = true;
		}
	});
	return (readValueResult && readResult);
}

bool ReadStringOption(QJsonObject obj, QString key, std::function<void(QString)> callback) {
	auto readResult = false;
	auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
		if (v.isString()) {
			callback(v.toString());
			readResult = true;
		}
	});
	return (readValueResult && readResult);
}

bool ReadIntOption(QJsonObject obj, QString key, std::function<void(int)> callback) {
	auto readResult = false;
	auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
		if (v.isDouble()) {
			callback(v.toInt());
			readResult = true;
		}
	});
	return (readValueResult && readResult);
}

bool ReadBoolOption(QJsonObject obj, QString key, std::function<void(bool)> callback) {
	auto readResult = false;
	auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
		if (v.isBool()) {
			callback(v.toBool());
			readResult = true;
		}
	});
	return (readValueResult && readResult);
}

std::unique_ptr<Manager> Data;

} // namespace

Manager::Manager() {
	_jsonWriteTimer.setSingleShot(true);
	connect(&_jsonWriteTimer, SIGNAL(timeout()), this, SLOT(writeTimeout()));
}

void Manager::fill() {
	if (!DefaultFileIsValid()) {
		writeDefaultFile();
	}
	if (!readCustomFile()) {
		WriteDefaultCustomFile();
	}
}

void Manager::write(bool force) {
	if (force && _jsonWriteTimer.isActive()) {
		_jsonWriteTimer.stop();
		writeTimeout();
	} else if (!force && !_jsonWriteTimer.isActive()) {
		_jsonWriteTimer.start(kWriteJsonTimeout);
	}
}

bool Manager::readCustomFile() {
	QFile file(CustomFilePath());
	if (!file.exists()) {
		return false;
	}
	if (!file.open(QIODevice::ReadOnly)) {
		return true;
	}
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(
		base::parse::stripComments(file.readAll()),
		&error);
	file.close();

	if (error.error != QJsonParseError::NoError) {
		return true;
	} else if (!document.isObject()) {
		return true;
	}
	const auto settings = document.object();

	if (settings.isEmpty()) {
		return true;
	}

	ReadObjectOption(settings, "fonts", [&](auto o) {
		ReadStringOption(o, "main", [&](auto v) {
			cSetMainFont(v);
		});

		ReadStringOption(o, "semibold", [&](auto v) {
			cSetSemiboldFont(v);
		});

		ReadBoolOption(o, "semibold_is_bold", [&](auto v) {
			cSetSemiboldFontIsBold(v);
		});

		ReadStringOption(o, "monospaced", [&](auto v) {
			cSetMonospaceFont(v);
		});

		ReadBoolOption(o, "use_system_font", [&](auto v) {
			cSetUseSystemFont(v);
		});
	});

	ReadIntOption(settings, "sticker_height", [&](auto v) {
		if (v >= 64 || v <= 256) {
			SetStickerHeight(v);
		}
	});

	auto isAdaptiveBubblesSet = ReadBoolOption(settings, "adaptive_bubbles", [&](auto v) {
		SetAdaptiveBubbles(v);
	});

	if (!isAdaptiveBubblesSet) {
		ReadBoolOption(settings, "adaptive_baloons", [&](auto v) {
			SetAdaptiveBubbles(v);
		});
	}

	ReadBoolOption(settings, "big_emoji_outline", [&](auto v) {
		SetBigEmojiOutline(v);
	});

	ReadBoolOption(settings, "always_show_scheduled", [&](auto v) {
		cSetAlwaysShowScheduled(v);
	});

	ReadBoolOption(settings, "show_chat_id", [&](auto v) {
		cSetShowChatId(v);
	});

	ReadOption(settings, "net_speed_boost", [&](auto v) {
		if (v.isString()) {

			const auto option = v.toString();
			if (option == "high") {
				SetNetworkBoost(3);
			} else if (option == "medium") {
				SetNetworkBoost(2);
			} else if (option == "low") {
				SetNetworkBoost(1);
			} else {
				SetNetworkBoost(0);
			}

		} else if (v.isNull()) {
			SetNetworkBoost(0);
		} else if (v.isDouble()) {
			SetNetworkBoost(v.toInt());
		}
	});

	ReadBoolOption(settings, "show_phone_in_drawer", [&](auto v) {
		cSetShowPhoneInDrawer(v);
	});

	ReadArrayOption(settings, "scales", [&](auto v) {
		ClearCustomScales();
		for (auto i = v.constBegin(), e = v.constEnd(); i != e; ++i) {
			if (!(*i).isDouble()) {
				continue;
			}

			AddCustomScale((*i).toInt());
		}
	});

	ReadIntOption(settings, "chat_list_lines", [&](auto v) {
		if (v >= 1 || v <= 2) {
			SetDialogListLines(v);
		}
	});

	ReadBoolOption(settings, "disable_up_edit", [&](auto v) {
		cSetDisableUpEdit(v);
	});

	ReadArrayOption(settings, "replaces", [&](auto v) {
		for (auto i = v.constBegin(), e = v.constEnd(); i != e; ++i) {
			if (!(*i).isArray()) {
				continue;
			}

			const auto a = (*i).toArray();

			if (a.size() != 2 || !a.at(0).isString() || !a.at(1).isString()) {
				continue;
			}
			const auto from = a.at(0).toString();
			const auto to = a.at(1).toString();

			AddCustomReplace(from, to);
			Ui::AddCustomReplacement(from, to);
		}
	});

	ReadBoolOption(settings, "confirm_before_calls", [&](auto v) {
		cSetConfirmBeforeCall(v);
	});
	return true;
}

void Manager::writeDefaultFile() {
	auto file = QFile(DefaultFilePath());
	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}
	const char *defaultHeader = R"HEADER(
// This is a list of default options for Kotatogram Desktop
// Please don't modify it, its content is not used in any way
// You can place your own options in the 'kotato-settings-custom.json' file

)HEADER";
	file.write(defaultHeader);

	auto settings = QJsonObject();
	settings.insert(qsl("version"), QString::number(AppKotatoVersion));

	auto settingsFonts = QJsonObject();
	settingsFonts.insert(qsl("main"), qsl("Open Sans"));
	settingsFonts.insert(qsl("semibold"), qsl("Open Sans Semibold"));
	settingsFonts.insert(qsl("semibold_is_bold"), false);
	settingsFonts.insert(qsl("monospaced"), qsl("Consolas"));
	settingsFonts.insert(qsl("use_system_font"), cUseSystemFont());

	settings.insert(qsl("fonts"), settingsFonts);

	settings.insert(qsl("sticker_height"), StickerHeight());
	settings.insert(qsl("adaptive_bubbles"), AdaptiveBubbles());
	settings.insert(qsl("big_emoji_outline"), BigEmojiOutline());
	settings.insert(qsl("always_show_scheduled"), cAlwaysShowScheduled());
	settings.insert(qsl("show_chat_id"), cShowChatId());
	settings.insert(qsl("net_speed_boost"), QJsonValue(QJsonValue::Null));
	settings.insert(qsl("show_phone_in_drawer"), cShowPhoneInDrawer());
	settings.insert(qsl("chat_list_lines"), DialogListLines());
	settings.insert(qsl("disable_up_edit"), cDisableUpEdit());
	settings.insert(qsl("confirm_before_calls"), cConfirmBeforeCall());

	auto settingsScales = QJsonArray();
	settings.insert(qsl("scales"), settingsScales);

	auto settingsReplaces = QJsonArray();
	settings.insert(qsl("replaces"), settingsReplaces);

	auto document = QJsonDocument();
	document.setObject(settings);
	file.write(document.toJson(QJsonDocument::Indented));
}

void Manager::writeCurrentSettings() {
	auto file = QFile(CustomFilePath());
	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}
	if (_jsonWriteTimer.isActive()) {
		writing();
	}
	const char *customHeader = R"HEADER(
// This file was automatically generated from current settings
// It's better to edit it with app closed, so there will be no rewrites
// You should restart app to see changes

)HEADER";
	file.write(customHeader);

	auto settings = QJsonObject();

	auto settingsFonts = QJsonObject();

	if (!cMainFont().isEmpty()) {
		settingsFonts.insert(qsl("main"), cMainFont());
	}

	if (!cSemiboldFont().isEmpty()) {
		settingsFonts.insert(qsl("semibold"), cSemiboldFont());
	}

	if (!cMonospaceFont().isEmpty()) {
		settingsFonts.insert(qsl("monospaced"), cMonospaceFont());
	}

	settingsFonts.insert(qsl("semibold_is_bold"), cSemiboldFontIsBold());
	settingsFonts.insert(qsl("use_system_font"), cUseSystemFont());

	settings.insert(qsl("fonts"), settingsFonts);

	settings.insert(qsl("sticker_height"), StickerHeight());
	settings.insert(qsl("adaptive_bubbles"), AdaptiveBubbles());
	settings.insert(qsl("big_emoji_outline"), BigEmojiOutline());
	settings.insert(qsl("always_show_scheduled"), cAlwaysShowScheduled());
	settings.insert(qsl("show_chat_id"), cShowChatId());
	settings.insert(qsl("net_speed_boost"), cNetSpeedBoost());
	settings.insert(qsl("show_phone_in_drawer"), cShowPhoneInDrawer());
	settings.insert(qsl("chat_list_lines"), DialogListLines());
	settings.insert(qsl("disable_up_edit"), cDisableUpEdit());
	settings.insert(qsl("confirm_before_calls"), cConfirmBeforeCall());

	auto settingsScales = QJsonArray();
	auto currentScales = cInterfaceScales();

	for (int i = 0; i < currentScales.size(); i++) {
		settingsScales << currentScales[i];
	}

	settings.insert(qsl("scales"), settingsScales);

	auto settingsReplaces = QJsonArray();
	auto currentReplaces = cCustomReplaces();

	for (auto i = currentReplaces.constBegin(), e = currentReplaces.constEnd(); i != e; ++i) {
		auto a = QJsonArray();
		a << i.key() << i.value();
		settingsReplaces << a;
	}

	settings.insert(qsl("replaces"), settingsReplaces);

	auto document = QJsonDocument();
	document.setObject(settings);
	file.write(document.toJson(QJsonDocument::Indented));
}

void Manager::writeTimeout() {
	writeCurrentSettings();
}

void Manager::writing() {
	_jsonWriteTimer.stop();
}

void Start() {
	if (Data) return;

	Data = std::make_unique<Manager>();
	Data->fill();
}

void Write() {
	if (!Data) return;

	Data->write();
}

void Finish() {
	if (!Data) return;

	Data->write(true);
}

} // namespace KotatoSettings
