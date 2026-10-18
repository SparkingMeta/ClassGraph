#pragma once
#pragma execution_character_set("utf-8")

#include <QColor>
#include <QString>

namespace CG::Ui{

	inline QColor WindowBg() { return QColor(28, 28, 28); }
	inline QColor PanelBg() { return QColor(36, 36, 36); }
	inline QColor HeaderBg() { return QColor(42, 42, 42); }
	inline QColor GraphBg() { return QColor(29, 29, 29); }
	inline QColor GridMinor() { return QColor(42, 42, 42); }
	inline QColor GridMajor() { return QColor(58, 58, 58); }
	inline QColor NodeBody() { return QColor(13, 14, 14, 250); }
	inline QColor NodeBorder() { return QColor(11, 14, 14); }
	inline QColor Selection() { return QColor(255, 198, 8); }
	inline QColor Text() { return QColor(224, 224, 224); }
	inline QColor TextMuted() { return QColor(154, 154, 154); }
	inline QColor ExecWhite() { return QColor(240, 240, 240); }
	inline QColor EntryTitle() { return QColor(150, 43, 42); }
	inline QColor FunctionTitle() { return QColor(44, 84, 104); }
	inline QColor PureTitle() { return QColor(40, 116, 52); }
	inline QColor ControllerTitle() { return QColor(240, 240, 240); }
	inline QColor OperatorTitle() { return QColor(255, 43, 0); }

	inline QColor TypeColor(const QString& type)
	{
		if (type == QStringLiteral("bool"))
			return QColor(204, 32, 32);
		if (type == QStringLiteral("int") || type == QStringLiteral("int64"))
			return QColor(0, 196, 196);
		if (type == QStringLiteral("float") || type == QStringLiteral("double"))
			return QColor(168, 220, 32);
		if (type == QStringLiteral("std::string"))
			return QColor(214, 32, 196);
		return QColor(32, 112, 220);
	}

	QString StyleSheet();
	QString PinEditorStyleSheet();

}
