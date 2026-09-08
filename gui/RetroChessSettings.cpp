/*******************************************************************************
 * gui/RetroChessSettings.cpp                                                  *
 *                                                                             *
 * Copyright (C) 2026 RetroShare Team <retroshare.project@gmail.com>           *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Affero General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Affero General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Affero General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#include "RetroChessSettings.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#endif

#include "gui/settings/rsharesettings.h"

namespace {

class ChessBoardPreviewWidget : public QWidget
{
public:
	explicit ChessBoardPreviewWidget(QWidget *parent = nullptr) : QWidget(parent)
	{
		setFixedSize(228, 228);
		m_pieceBB = QIcon(":/piece/bB.svg").pixmap(60, 60);
		m_pieceBQ = QIcon(":/piece/bQ.svg").pixmap(60, 60);
		m_pieceBP = QIcon(":/piece/bP.svg").pixmap(60, 60);
		m_pieceWN = QIcon(":/piece/wN.svg").pixmap(60, 60);
		m_pieceWK = QIcon(":/piece/wK.svg").pixmap(60, 60);
		m_pieceWR = QIcon(":/piece/wR.svg").pixmap(60, 60);
	}

	void setTheme(const RetroChessBoardTheme &theme)
	{
		m_theme = theme;
		update();
	}

protected:
	void paintEvent(QPaintEvent *event) override
	{
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

		QFont rankFont = painter.font();
		rankFont.setPointSize(10);
		rankFont.setBold(true);

		const int squareSize = 76;
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col) {
				const bool isDark = ((row + col) % 2) != 0;
				const QRect sqRect(col * squareSize, row * squareSize, squareSize, squareSize);
				painter.fillRect(sqRect, isDark ? m_theme.dark : m_theme.light);

				// Rank numbers on column 0 (Ranks 8, 7, 6) in contrasting theme color
				if (col == 0) {
					const QString rankText = (row == 0) ? QStringLiteral("8") :
					                         (row == 1) ? QStringLiteral("7") :
					                                      QStringLiteral("6");
					painter.setFont(rankFont);
					painter.setPen(isDark ? m_theme.light : m_theme.dark);
					painter.drawText(sqRect.adjusted(4, 3, 0, 0), Qt::AlignLeft | Qt::AlignTop, rankText);
				}

				// Pieces
				QPixmap piecePixmap;
				if (row == 0) {
					if (col == 0) piecePixmap = m_pieceBB;
					else if (col == 1) piecePixmap = m_pieceBQ;
					else if (col == 2) piecePixmap = m_pieceBP;
				} else if (row == 2) {
					if (col == 0) piecePixmap = m_pieceWN;
					else if (col == 1) piecePixmap = m_pieceWK;
					else if (col == 2) piecePixmap = m_pieceWR;
				}

				if (!piecePixmap.isNull()) {
					const int px = sqRect.left() + (squareSize - piecePixmap.width()) / 2;
					const int py = sqRect.top() + (squareSize - piecePixmap.height()) / 2;
					painter.drawPixmap(px, py, piecePixmap);
				}
			}
		}
	}

private:
	RetroChessBoardTheme m_theme;
	QPixmap m_pieceBB;
	QPixmap m_pieceBQ;
	QPixmap m_pieceBP;
	QPixmap m_pieceWN;
	QPixmap m_pieceWK;
	QPixmap m_pieceWR;
};

} // namespace

QVector<RetroChessBoardTheme> RetroChessSettings::boardThemes()
{
	return {
		{"original", QObject::tr("Default"), QColor(211, 211, 158), QColor(120, 120, 90), QColor("#d3d3d3")},
		{"brown", QObject::tr("Brown"), QColor("#f0d9b5"), QColor("#b58863"), QColor("#baca44")},
		{"green", QObject::tr("Green"), QColor("#eeeed2"), QColor("#769656"), QColor("#baca44")},
		{"sky", QObject::tr("Sky"), QColor("#eef1f0"), QColor("#c4dbe5"), QColor("#a8cce4")},
		{"blue", QObject::tr("Blue"), QColor("#f1f5f7"), QColor("#5593ec"), QColor("#82b4f5")},
		{"darkblue", QObject::tr("Dark blue"), QColor("#eeeed2"), QColor("#4b7396"), QColor("#7fa5c4")},
		{"checkers", QObject::tr("Checkers"), QColor("#c4484c"), QColor("#303030"), QColor("#e87c80")},
		{"purple", QObject::tr("Purple"), QColor("#f0f1f0"), QColor("#8476ba"), QColor("#baca44")},
		{"red", QObject::tr("Red"), QColor("#f5dbc3"), QColor("#bb5746"), QColor("#baca44")}
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

bool RetroChessSettings::moveSoundEnabled()
{
	return Settings->valueFromGroup("RetroChess", "SoundMove", true).toBool();
}

bool RetroChessSettings::captureSoundEnabled()
{
	return Settings->valueFromGroup("RetroChess", "SoundCapture", true).toBool();
}

bool RetroChessSettings::gameResultSoundEnabled()
{
	return Settings->valueFromGroup("RetroChess", "SoundGameResult", true).toBool();
}

bool RetroChessSettings::invitationSoundEnabled()
{
	return Settings->valueFromGroup("RetroChess", "SoundInvitation", true).toBool();
}

void RetroChessSettings::setSoundOptions(
        bool move, bool capture, bool gameResult, bool invitation)
{
	Settings->setValueToGroup("RetroChess", "SoundMove", move);
	Settings->setValueToGroup("RetroChess", "SoundCapture", capture);
	Settings->setValueToGroup("RetroChess", "SoundGameResult", gameResult);
	Settings->setValueToGroup("RetroChess", "SoundInvitation", invitation);
	Settings->sync();
}

RetroChessSettingsDialog::RetroChessSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(tr("RetroChess Settings"));
	setMinimumSize(780, 480);

	QVBoxLayout *root = new QVBoxLayout(this);
	QHBoxLayout *content = new QHBoxLayout;
	QListWidget *navigation = new QListWidget(this);
	navigation->setFixedWidth(145);
	navigation->addItem(tr("Board colours"));
	navigation->addItem(tr("Sounds"));

	QStackedWidget *pages = new QStackedWidget(this);
	content->addWidget(navigation);
	content->addWidget(pages, 1);
	root->addLayout(content, 1);

	// --- Board Colours Page ---
	QWidget *boardPage = new QWidget(pages);
	QVBoxLayout *boardRoot = new QVBoxLayout(boardPage);
	QLabel *title = new QLabel(tr("Board colours"), boardPage);
	QFont titleFont = title->font();
	titleFont.setPointSize(titleFont.pointSize() + 3);
	titleFont.setBold(true);
	title->setFont(titleFont);
	boardRoot->addWidget(title);
	boardRoot->addWidget(new QLabel(
	        tr("Choose a board theme."), boardPage));

	QHBoxLayout *centerLayout = new QHBoxLayout;
	centerLayout->setSpacing(24);
	centerLayout->setAlignment(Qt::AlignTop);

	QGridLayout *themesLayout = new QGridLayout;
	themesLayout->setContentsMargins(0, 0, 0, 0);
	themesLayout->setHorizontalSpacing(8);
	themesLayout->setVerticalSpacing(8);
	themesLayout->setAlignment(Qt::AlignTop);

	QButtonGroup *group = new QButtonGroup(this);
	group->setExclusive(true);

	const QVector<RetroChessBoardTheme> themes = RetroChessSettings::boardThemes();
	const RetroChessBoardTheme currentTheme = RetroChessSettings::boardTheme();

	QVBoxLayout *previewLayout = new QVBoxLayout;
	previewLayout->setContentsMargins(0, 0, 0, 0);
	previewLayout->setSpacing(8);
	previewLayout->setAlignment(Qt::AlignTop);

	QLabel *previewTitle = new QLabel(currentTheme.name, boardPage);
	QFont previewTitleFont = previewTitle->font();
	previewTitleFont.setBold(true);
	previewTitle->setFont(previewTitleFont);
	previewLayout->addWidget(previewTitle);

	ChessBoardPreviewWidget *boardPreview = new ChessBoardPreviewWidget(boardPage);
	boardPreview->setTheme(currentTheme);
	previewLayout->addWidget(boardPreview);

	const QString buttonStyle = QStringLiteral(
	        "QToolButton {"
	        "  border: 1px solid #b5b5b5;"
	        "  border-radius: 3px;"
	        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #fbfbfb, stop:1 #e8e8e8);"
	        "  color: #222222;"
	        "  padding: 4px;"
	        "  font-size: 11px;"
	        "}"
	        "QToolButton:hover {"
	        "  border: 1px solid #999999;"
	        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e0e0e0);"
	        "}"
	        "QToolButton:checked {"
	        "  border: 1px solid #707070;"
	        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f4f4f4, stop:0.25 #e4e4e4, stop:1 #d2d2d2);"
	        "}");

	for (int i = 0; i < themes.size(); ++i) {
		const RetroChessBoardTheme &theme = themes.at(i);

		// 2x2 chess square preview (52x52 pixels, 26x26 per square)
		QPixmap preview(52, 52);
		preview.fill(Qt::transparent);
		QPainter p(&preview);
		p.fillRect(0, 0, 26, 26, theme.light);
		p.fillRect(26, 0, 26, 26, theme.dark);
		p.fillRect(0, 26, 26, 26, theme.dark);
		p.fillRect(26, 26, 26, 26, theme.light);
		p.end();

		QToolButton *button = new QToolButton(boardPage);
		button->setText(theme.name);
		button->setIcon(QIcon(preview));
		button->setIconSize(QSize(52, 52));
		button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		button->setCheckable(true);
		button->setFixedSize(104, 80);
		button->setStyleSheet(buttonStyle);
		button->setProperty("themeId", theme.id);

		if (theme.id == currentTheme.id) {
			button->setChecked(true);
		}

		group->addButton(button);
		themesLayout->addWidget(button, i / 3, i % 3);

		connect(button, &QToolButton::toggled, this, [theme, boardPreview, previewTitle](bool checked) {
			if (checked) {
				boardPreview->setTheme(theme);
				previewTitle->setText(theme.name);
			}
		});
	}

	centerLayout->addLayout(themesLayout);
	centerLayout->addLayout(previewLayout);
	centerLayout->addStretch();
	boardRoot->addLayout(centerLayout);
	boardRoot->addStretch();
	pages->addWidget(boardPage);

	// --- Sounds Page ---
	QWidget *soundsPage = new QWidget(pages);
	QVBoxLayout *soundsRoot = new QVBoxLayout(soundsPage);
	QLabel *soundsTitle = new QLabel(tr("Sounds"), soundsPage);
	soundsTitle->setFont(titleFont);
	soundsRoot->addWidget(soundsTitle);
	soundsRoot->addWidget(new QLabel(
	        tr("Choose which RetroChess events are allowed to play a sound. Preview buttons play a sample."), soundsPage));

	QMediaPlayer *player = new QMediaPlayer(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QAudioOutput *audioOutput = new QAudioOutput(player);
	player->setAudioOutput(audioOutput);
	audioOutput->setVolume(0.8f);
#else
	player->setVolume(80);
#endif

	auto playSound = [player](const QString &path) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		player->setSource(QUrl(path));
#else
		player->setMedia(QUrl(path));
#endif
		player->play();
	};

	QGridLayout *soundRows = new QGridLayout;
	soundRows->setColumnStretch(1, 1);
	auto addSound = [soundsPage, soundRows, playSound](
	        int row, const QString &name, const QString &purpose,
	        const QString &fileName, const QString &soundResource, bool checked) {
		QCheckBox *enabled = new QCheckBox(name, soundsPage);
		enabled->setChecked(checked);
		QLabel *description = new QLabel(purpose, soundsPage);
		QLabel *file = new QLabel(fileName, soundsPage);
		file->setTextInteractionFlags(Qt::TextSelectableByMouse);
		QPushButton *previewBtn = new QPushButton(QObject::tr("Preview"), soundsPage);
		previewBtn->setFixedWidth(64);
		QObject::connect(previewBtn, &QPushButton::clicked, [playSound, soundResource]() {
			playSound(soundResource);
		});
		soundRows->addWidget(enabled, row, 0);
		soundRows->addWidget(description, row, 1);
		soundRows->addWidget(file, row, 2);
		soundRows->addWidget(previewBtn, row, 3);
		return enabled;
	};

	QCheckBox *moveSound = addSound(0, tr("Piece move"),
	        tr("After a normal chess move"), "Move.mp3",
	        "qrc:/sound/Move.mp3",
	        RetroChessSettings::moveSoundEnabled());
	QCheckBox *captureSound = addSound(1, tr("Piece capture"),
	        tr("After a piece is captured"), "Capture.mp3",
	        "qrc:/sound/Capture.mp3",
	        RetroChessSettings::captureSoundEnabled());
	QCheckBox *resultSound = addSound(2, tr("Game result"),
	        tr("When a game ends with a win, draw, or defeat"),
	        "victory.mp3 / Draw.mp3 / Defeat.mp3",
	        "qrc:/sound/victory.mp3",
	        RetroChessSettings::gameResultSoundEnabled());
	QCheckBox *inviteSound = addSound(3, tr("Chess invitation"),
	        tr("When an invitation toaster is received"), "ping.mp3",
	        "qrc:/sound/ping.mp3",
	        RetroChessSettings::invitationSoundEnabled());

	soundsRoot->addLayout(soundRows);
	soundsRoot->addStretch();
	pages->addWidget(soundsPage);

	connect(navigation, &QListWidget::currentRowChanged,
	        pages, &QStackedWidget::setCurrentIndex);
	navigation->setCurrentRow(0);

	QDialogButtonBox *buttons = new QDialogButtonBox(
	        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
	root->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::accepted, this,
	        [this, group, moveSound, captureSound, resultSound, inviteSound]() {
		if (group->checkedButton()) {
			RetroChessSettings::setBoardThemeId(
			        group->checkedButton()->property("themeId").toString());
		}
		RetroChessSettings::setSoundOptions(
		        moveSound->isChecked(), captureSound->isChecked(),
		        resultSound->isChecked(), inviteSound->isChecked());
		accept();
	});
}
