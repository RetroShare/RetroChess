#include "RetroChessSettings.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

#include "gui/settings/rsharesettings.h"

QVector<RetroChessBoardTheme> RetroChessSettings::boardThemes()
{
	return {
		{"original", QObject::tr("Original"), QColor(211, 211, 158), QColor(120, 120, 90), QColor("#d3d3d3")},
		{"green", QObject::tr("Green"), QColor("#eeeed2"), QColor("#769656"), QColor("#58a995")},
		{"brown", QObject::tr("Brown"), QColor("#f0d9b5"), QColor("#b58863"), QColor("#b3aa32")},
		{"blue", QObject::tr("Blue"), QColor("#dee3e6"), QColor("#8ca2ad"), QColor("#98b66f")},
		{"checkers", QObject::tr("Checkers"), QColor("#d25058"), QColor("#292929"), QColor("#d3d3d3")}
	};
}

QString RetroChessSettings::boardThemeId()
{
	return Settings->valueFromGroup("RetroChess", "BoardTheme", "original").toString();
}

RetroChessBoardTheme RetroChessSettings::boardTheme()
{
	const QString selected = boardThemeId();
	for (const RetroChessBoardTheme &theme : boardThemes())
		if (theme.id == selected) return theme;
	return boardThemes().first();
}

void RetroChessSettings::setBoardThemeId(const QString &id)
{
	Settings->setValueToGroup("RetroChess", "BoardTheme", id);
	Settings->sync();
}

RetroChessSettingsDialog::RetroChessSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(tr("RetroChess Settings"));
	setMinimumWidth(470);
	QVBoxLayout *root = new QVBoxLayout(this);
	QLabel *title = new QLabel(tr("Board colours"), this);
	QFont titleFont = title->font();
	titleFont.setPointSize(titleFont.pointSize() + 3);
	titleFont.setBold(true);
	title->setFont(titleFont);
	root->addWidget(title);
	root->addWidget(new QLabel(tr("Choose the colour theme used for new and active chess boards."), this));

	QGridLayout *themesLayout = new QGridLayout;
	QButtonGroup *group = new QButtonGroup(this);
	group->setExclusive(true);
	const QVector<RetroChessBoardTheme> themes = RetroChessSettings::boardThemes();
	for (int i = 0; i < themes.size(); ++i) {
		const RetroChessBoardTheme &theme = themes.at(i);
		QPixmap preview(92, 64);
		QPainter painter(&preview);
		for (int row = 0; row < 4; ++row)
			for (int col = 0; col < 4; ++col)
				painter.fillRect(col * 23, row * 16, 23, 16,
				                 ((row + col) % 2) ? theme.dark : theme.light);

		QToolButton *button = new QToolButton(this);
		button->setText(theme.name);
		button->setIcon(QIcon(preview));
		button->setIconSize(preview.size());
		button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		button->setCheckable(true);
		button->setMinimumSize(110, 94);
		button->setProperty("themeId", theme.id);
		button->setChecked(theme.id == RetroChessSettings::boardThemeId());
		group->addButton(button);
		themesLayout->addWidget(button, i / 3, i % 3);
	}
	root->addLayout(themesLayout);

	QDialogButtonBox *buttons = new QDialogButtonBox(
	        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
	root->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::accepted, this, [this, group]() {
		if (group->checkedButton())
			RetroChessSettings::setBoardThemeId(
			        group->checkedButton()->property("themeId").toString());
		accept();
	});
}
