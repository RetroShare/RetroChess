#ifndef RETROCHESSSETTINGS_H
#define RETROCHESSSETTINGS_H

#include <QColor>
#include <QDialog>
#include <QString>
#include <QVector>

struct RetroChessBoardTheme
{
	QString id;
	QString name;
	QColor light;
	QColor dark;
	QColor lastMove;
};

class RetroChessSettings
{
public:
	static QVector<RetroChessBoardTheme> boardThemes();
	static RetroChessBoardTheme boardTheme();
	static QString boardThemeId();
	static void setBoardThemeId(const QString &id);
};

class RetroChessSettingsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit RetroChessSettingsDialog(QWidget *parent = nullptr);
};

#endif
