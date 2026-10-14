#pragma once
#pragma execution_character_set("utf-8")

#include "VDto.h"

#include <QColor>
#include <QString>

using namespace CG::Router;

namespace CG::Ui{

	inline QColor WindowBg() { return QColor(28, 28, 28); }
	inline QColor PanelBg() { return QColor(36, 36, 36); }
	inline QColor HeaderBg() { return QColor(42, 42, 42); }
	inline QColor GraphBg() { return QColor(16, 16, 16); }
	inline QColor GridMinor() { return QColor(24, 24, 24); }
	inline QColor GridMajor() { return QColor(40, 40, 40); }
	inline QColor NodeBody() { return QColor(18, 18, 18, 245); }
	inline QColor NodeBorder() { return QColor(8, 8, 8); }
	inline QColor Selection() { return QColor(255, 198, 8); }
	inline QColor Text() { return QColor(224, 224, 224); }
	inline QColor TextMuted() { return QColor(154, 154, 154); }
	inline QColor ExecWhite() { return QColor(240, 240, 240); }

	inline QColor PinColor(const QString& type_name, DtoPinKind kind)
	{
		if (kind == DtoPinKind::Exec || type_name.isEmpty())
			return ExecWhite();
		if (type_name == QLatin1String("bool")) return QColor(220, 48, 48);
		if (type_name == QLatin1String("int") || type_name == QLatin1String("int64"))
			return QColor(0, 196, 196);
		if (type_name == QLatin1String("float") || type_name == QLatin1String("double"))
			return QColor(88, 196, 88);
		if (type_name == QLatin1String("std::string")) return QColor(220, 64, 196);
		return QColor(220, 196, 64);
	}

	inline QColor NodeHeader(const QString& caller_name, int caller_kind, bool is_pure)
	{
		if (caller_name.isEmpty())
			return QColor(70, 70, 70);
		if (caller_name == QLatin1String("Entry") || caller_name == QLatin1String("Exit"))
			return QColor(158, 36, 36);
		if (caller_kind == 2) // CK_Controller
			return QColor(96, 96, 96);
		if (is_pure)
			return QColor(16, 128, 128);
		if (caller_kind == 3) // CK_Operator
			return QColor(48, 112, 128);
		return QColor(56, 96, 148);
	}

	QString StyleSheet();

}
