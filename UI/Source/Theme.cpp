#pragma execution_character_set("utf-8")

#include "Theme.h"

QString CG::Ui::StyleSheet()
{
	return QStringLiteral(R"(
QWidget {
	background-color: #1c1c1c;
	color: #e0e0e0;
	font-family: "Segoe UI";
	font-size: 12px;
}
QMainWindow {
	background-color: #1c1c1c;
}
QMainWindow::separator {
	background: #000000;
	width: 2px;
	height: 2px;
}
QMenuBar {
	background-color: #2a2a2a;
	color: #e0e0e0;
	border-bottom: 1px solid #000000;
	padding: 2px 4px;
}
QMenuBar::item {
	padding: 4px 10px;
	background: transparent;
}
QMenuBar::item:selected {
	background: #4a4a4a;
}
QMenu {
	background-color: #2b2b2b;
	border: 1px solid #000000;
	padding: 4px;
}
QMenu::item {
	padding: 5px 28px 5px 12px;
}
QMenu::item:selected {
	background: #3d5a73;
}
QMenu::separator {
	height: 1px;
	background: #111111;
	margin: 4px 8px;
}
QToolBar {
	background-color: #2d2d2d;
	border: none;
	border-bottom: 1px solid #000000;
	spacing: 4px;
	padding: 4px 8px;
}
QToolBar::separator {
	background: #111111;
	width: 1px;
	margin: 4px 8px;
}
QToolButton, QToolBar QToolButton {
	background: #3a3a3a;
	color: #e0e0e0;
	border: 1px solid #1a1a1a;
	border-radius: 2px;
	padding: 4px 12px;
	min-height: 22px;
}
QToolButton:hover {
	background: #4a4a4a;
}
QToolButton:pressed {
	background: #2a2a2a;
}
QToolButton#CompileButton {
	background: #2d5a2d;
	border: 1px solid #1a3a1a;
}
QToolButton#CompileButton:hover {
	background: #3a7a3a;
}
QToolButton#PlayButton {
	background: #2a4a6a;
	border: 1px solid #1a2a3a;
}
QToolButton#PlayButton:hover {
	background: #3a6a8a;
}
QDockWidget {
	titlebar-close-icon: none;
	titlebar-normal-icon: none;
	color: #c8c8c8;
	font-weight: bold;
}
QDockWidget::title {
	background: #2a2a2a;
	border-bottom: 1px solid #000000;
	padding: 6px 10px;
	text-align: left;
}
QWidget#DockTitleBar {
	background: #2a2a2a;
	border-bottom: 1px solid #000000;
	min-height: 28px;
}
QLabel#DockTitle {
	color: #c8c8c8;
	font-weight: bold;
}
QDockWidget > QWidget {
	background: #242424;
	border: 1px solid #000000;
}
QTabBar::tab {
	background: #2a2a2a;
	color: #b0b0b0;
	border: 1px solid #000000;
	padding: 6px 16px;
	margin-right: 1px;
}
QTabBar::tab:selected {
	background: #3a3a3a;
	color: #ffffff;
	border-bottom: 2px solid #e6a800;
}
QTabBar::tab:hover {
	background: #333333;
	color: #ffffff;
}
QTreeWidget, QTreeView, QListView, QTextEdit, QPlainTextEdit, QScrollArea {
	background-color: #1a1a1a;
	border: none;
	color: #dcdcdc;
	selection-background-color: #3d5a73;
	selection-color: #ffffff;
	outline: none;
}
QTreeWidget::item, QTreeView::item {
	padding: 3px 4px;
	min-height: 20px;
}
QTreeWidget::item:hover {
	background: #323232;
}
QTreeWidget::item:selected {
	background: #3d5a73;
}
QHeaderView::section {
	background: #2a2a2a;
	color: #c8c8c8;
	border: none;
	padding: 4px;
}
QLineEdit, QComboBox, QSpinBox {
	background: #0f0f0f;
	color: #e0e0e0;
	border: 1px solid #111111;
	border-radius: 2px;
	padding: 4px 6px;
	min-height: 20px;
}
QLineEdit#PinEditor {
	padding: 0 4px;
	min-height: 16px;
	max-height: 18px;
	background: #141414;
	border: 1px solid #3a3a3a;
}
QCheckBox#PinEditor {
	background: transparent;
	spacing: 0;
}
QCheckBox#PinEditor::indicator {
	width: 13px;
	height: 13px;
	border: 1px solid #5a5a5a;
	background: #1a1a1a;
}
QCheckBox#PinEditor::indicator:checked {
	background: #4a90c8;
	border: 1px solid #6ab0e8;
}
QLineEdit:focus, QComboBox:focus {
	border: 1px solid #3d5a73;
}
QComboBox::drop-down {
	border: none;
	width: 18px;
}
QComboBox QAbstractItemView {
	background: #2b2b2b;
	selection-background-color: #3d5a73;
}
QPushButton {
	background: #3a3a3a;
	color: #e0e0e0;
	border: 1px solid #1a1a1a;
	border-radius: 2px;
	padding: 5px 14px;
}
QPushButton:hover {
	background: #4a4a4a;
}
QPushButton:pressed {
	background: #2a2a2a;
}
QScrollBar:vertical {
	background: #1a1a1a;
	width: 10px;
	margin: 0;
}
QScrollBar::handle:vertical {
	background: #4a4a4a;
	min-height: 24px;
	border-radius: 2px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
	background: none;
	height: 0;
}
QScrollBar:horizontal {
	background: #1a1a1a;
	height: 10px;
}
QScrollBar::handle:horizontal {
	background: #4a4a4a;
	min-width: 24px;
}
QStatusBar {
	background: #2a2a2a;
	color: #a0a0a0;
	border-top: 1px solid #000000;
}
QLabel#Breadcrumb {
	color: #c8c8c8;
	padding: 0 12px;
	font-size: 12px;
}
QLabel#PanelHint {
	color: #8a8a8a;
	padding: 8px;
}
QFrame#GraphTabBarFrame {
	background: #242424;
	border-bottom: 1px solid #000000;
}
QWidget#SectionHeader {
	background-color: #2a2a2a;
	border-bottom: 1px solid #000000;
	min-height: 26px;
}
QLabel#SectionTitle {
	color: #c8c8c8;
	font-weight: bold;
	padding-left: 8px;
}
QToolButton#PlusButton {
	background: #3a3a3a;
	color: #e8e8e8;
	font-size: 13px;
	font-weight: bold;
	border: 1px solid #1a1a1a;
	border-radius: 2px;
	padding: 0px;
	margin: 0px;
	min-width: 22px;
	max-width: 22px;
	min-height: 22px;
	max-height: 22px;
}
QToolButton#PlusButton:hover {
	background: #4a90d9;
	border: 1px solid #2a6aa8;
}
QToolButton#ClearButton {
	background: #3a3a3a;
	color: #e8e8e8;
	font-size: 11px;
	border: 1px solid #1a1a1a;
	border-radius: 2px;
	padding: 0px 8px;
	min-height: 22px;
	max-height: 22px;
}
QToolButton#ClearButton:hover {
	background: #4a90d9;
	border: 1px solid #2a6aa8;
}
QComboBox#VarTypeCombo, QComboBox#ParamTypeCombo {
	background: #1a1a1a;
	min-width: 72px;
	max-width: 110px;
	padding: 1px 4px;
}
QLineEdit#BlueprintNameEdit {
	background: #1a1a1a;
	padding: 2px 6px;
	min-height: 18px;
	border: 1px solid #111111;
}
QSplitter::handle {
	background: #000000;
}
)");
}
