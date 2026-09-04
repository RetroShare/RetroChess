/*******************************************************************************
 * gui/chess.cpp                                                               *
 *                                                                             *
 * Copyright (C) 2020 RetroShare Team <retroshare.project@gmail.com>           *
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

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QTableWidget>
#include <QMediaPlayer>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#endif
#include <QMessageBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QUrl>
#include <QVBoxLayout>
#include <QTimer>

#include "chess.h"
#include "ui_chess.h"
#include "RetroChessSettings.h"
#include "ChessDebugWidget.h"

#include <QPointer>

#include "gui/common/AvatarDefs.h"
#include "../services/p3RetroChess.h"

// NEW: Constructor for Distant GXS Identity
RetroChessWindow::RetroChessWindow(const RsGxsId &gxsId, int player, QWidget *parent) :
    QWidget(parent),
    m_ui(new Ui::RetroChessWindow),
    mGxsId(gxsId),
    mIsGxs(true),
    m_suppressLeave(false),
    m_resultPopupShown(false),
    m_rematchRequested(false),
    m_capturedBlackLabel(nullptr),
    m_capturedWhiteLabel(nullptr),
    m_moveTable(nullptr),
	m_historyFirstButton(nullptr),
	m_historyPreviousButton(nullptr),
	m_historyNextButton(nullptr),
	m_historyLatestButton(nullptr),
	m_viewedHistoryPly(0),
    m_moveSound(nullptr),
    m_captureSound(nullptr),
    m_victorySound(nullptr),
    m_debugWidget(nullptr),
    m_fullmoveNumber(1),
    m_desynchronized(false)
{
    QString player_str; 
    if (player == 1) {
        player_str = " (1)";
    } else if (player == 2) {
        player_str = " (2)";
    }

    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    mPeerId = gxsId.toStdString(); // Use string representation for internal tracking
	mOwnGxsId = rsRetroChess->ownGxsIdForPeer(gxsId);

    m_ui->m_player1_result->hide();
    m_ui->m_player2_result->hide();
    m_ui->m_status_bar->hide();

    m_flag_finished = 0;	// set as unfinish
	m_checkedKingTile = -1;
	m_enPassantPawnTile = -1;
	m_pendingPromotionChoice = 0;
	m_kingMoved[0] = m_kingMoved[1] = false;
	m_rookMoved[0][0] = m_rookMoved[0][1] = false;
	m_rookMoved[1][0] = m_rookMoved[1][1] = false;
	m_halfmoveClock = 0;
	click1 = nullptr;

    //tile = { { NULL } };
    count=0;
    turn=1;	// white first
    max=0;
    texp = new int[60];

    if (player) { // local player as black
        // Note: For GXS we track identities rather than PeerIds
        player_str = " (1)";
        m_localplayer_turn = 0;
        
        // Use non-blocking lookup with fallback for unknown identities
        RsIdentityDetails d1, d2;
        if (rsIdentity->getIdDetails(mOwnGxsId, d1)) {
            p1name = d1.mNickname;
        } else {
            p1name = mOwnGxsId.isNull() ? "Local GXS identity" : mOwnGxsId.toStdString().substr(0, 8) + "...";
        }
        if (rsIdentity->getIdDetails(gxsId, d2)) {
            p2name = d2.mNickname;
        } else {
            p2name = gxsId.toStdString().substr(0, 8) + "...";
        }
    } else { // local player as white
        player_str = " (2)";
        m_localplayer_turn = 1;

        RsIdentityDetails d1, d2;
        if (rsIdentity->getIdDetails(gxsId, d1)) {
            p1name = d1.mNickname;
        } else {
            p1name = gxsId.toStdString().substr(0, 8) + "...";
        }
        if (rsIdentity->getIdDetails(mOwnGxsId, d2)) {
            p2name = d2.mNickname;
        } else {
            p2name = mOwnGxsId.isNull() ? "Local GXS identity" : mOwnGxsId.toStdString().substr(0, 8) + "...";
        }
    }

	const std::string &localName = m_localplayer_turn == 0 ? p1name : p2name;
	const std::string &opponentName = m_localplayer_turn == 0 ? p2name : p1name;
	QString title = QString::fromUtf8(localName.c_str()) + " Playing Chess against "
	        + QString::fromUtf8(opponentName.c_str()) + player_str;

    setWindowTitle(title);
    initAccessories();
    playerTurnNotice();
    initChessBoard();
}

RetroChessWindow::RetroChessWindow(std::string peerid, int player, QWidget *parent) :
	QWidget(parent),
	m_ui( new Ui::RetroChessWindow() ),
	mPeerId(peerid),
	mIsGxs(false),
	m_suppressLeave(false),
	m_resultPopupShown(false),
	m_rematchRequested(false),
	m_capturedBlackLabel(nullptr),
	m_capturedWhiteLabel(nullptr),
	m_moveTable(nullptr),
	m_historyFirstButton(nullptr),
	m_historyPreviousButton(nullptr),
	m_historyNextButton(nullptr),
	m_historyLatestButton(nullptr),
	m_viewedHistoryPly(0),
	m_moveSound(nullptr),
	m_captureSound(nullptr),
	m_victorySound(nullptr),
	m_debugWidget(nullptr),
	m_fullmoveNumber(1),
	m_desynchronized(false)
	//ui(new Ui::RetroChessWindow)
{
	m_ui->setupUi( this );
	setAttribute(Qt::WA_DeleteOnClose);
    m_ui->m_player1_result->hide();
    m_ui->m_player2_result->hide();
    m_ui->m_status_bar->hide();

    m_flag_finished = 0;	// set as unfinish
	m_checkedKingTile = -1;
	m_enPassantPawnTile = -1;
	m_pendingPromotionChoice = 0;
	m_kingMoved[0] = m_kingMoved[1] = false;
	m_rookMoved[0][0] = m_rookMoved[0][1] = false;
	m_rookMoved[1][0] = m_rookMoved[1][1] = false;
	m_halfmoveClock = 0;
	click1 = nullptr;

	//tile = { { NULL } };
	count=0;
    turn=1;	// white first
	max=0;
	texp = new int[60];

	QString player_str;
    if (player )	// local player as black
	{
		p1id = rsPeers->getOwnId();
		p2id = RsPeerId(peerid);
		player_str = " (1)";

        m_localplayer_turn = 0;
	}
    else	// local player as white
	{
		p1id = RsPeerId(peerid);
		p2id = rsPeers->getOwnId();
		player_str = " (2)";

        m_localplayer_turn = 1;
    }

	p1name = rsPeers->getPeerName(p1id);
	p2name = rsPeers->getPeerName(p2id);

	const std::string &localName = m_localplayer_turn == 0 ? p1name : p2name;
	const std::string &opponentName = m_localplayer_turn == 0 ? p2name : p1name;
	QString title = QString::fromStdString(localName);
	title += " Playing Chess against ";
	title += QString::fromStdString(opponentName);
	title+=player_str;


	this->setWindowTitle(title);

	this->initAccessories();
	this->initChessBoard();

    this->playerTurnNotice();
}

RetroChessWindow::~RetroChessWindow()
{
	delete[] texp;
	delete m_ui;
}

static constexpr int TILE_SIZE = 64;
static constexpr int BORDER_SIZE = 20;
static constexpr int BOARD_FULL_SIZE = BORDER_SIZE * 2 + TILE_SIZE * 8; // 552
static constexpr int BOARD_INNER_SIZE = BOARD_FULL_SIZE - BORDER_SIZE; // 532

class Border
{
public:
	Border();
	static void outline(QWidget *baseWidget, int xPos, int yPos, int Pos)
	{
		QLabel *outLabel = new QLabel(baseWidget);
		outLabel->setProperty("retroChessBoardBorder", true);

		if(!Pos)
			outLabel->setGeometry(xPos,yPos,BOARD_FULL_SIZE,BORDER_SIZE);        //Horizontal Borders

		else
			outLabel->setGeometry(xPos,yPos,BORDER_SIZE,BOARD_FULL_SIZE-2*BORDER_SIZE);        //Vertical Borders

		const QColor borderColor = RetroChessSettings::boardTheme().dark.lighter(135);
		outLabel->setStyleSheet(QString("QLabel { background-color: %1; color: black; }")
		                            .arg(borderColor.name()));
	}
};

void RetroChessWindow::initAccessories()
{
	// display player's name
	m_ui->m_player1_name->setText( p1name.c_str() );
	m_ui->m_player2_name->setText( p2name.c_str() );
	m_ui->m_move_record->hide();
	m_ui->moveHistoryLayout->removeWidget(m_ui->m_move_record);
	m_moveTable = new QTableWidget(0, 3, m_ui->moveHistoryFrame);
	m_moveTable->setHorizontalHeaderLabels(QStringList() << "#" << tr("White") << tr("Black"));
	m_moveTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_moveTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_moveTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	m_moveTable->verticalHeader()->hide();
	m_moveTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_moveTable->setSelectionMode(QAbstractItemView::SingleSelection);
	m_moveTable->setSelectionBehavior(QAbstractItemView::SelectItems);
	m_moveTable->setShowGrid(false);
	m_moveTable->setAlternatingRowColors(true);
	m_moveTable->setStyleSheet(
	        "QTableWidget { border: 0; background: transparent; alternate-background-color: #eeeeee; }"
	        "QHeaderView::section { background: #e3e3e3; border: 0; padding: 4px; color: #555; }");
	m_ui->moveHistoryLayout->addWidget(m_moveTable, 1);

	QHBoxLayout *historyControls = new QHBoxLayout;
	historyControls->setContentsMargins(2, 0, 2, 0);
	historyControls->setSpacing(2);
	m_historyFirstButton = new QPushButton(QIcon(":/images/skip-prev-solid.png"), QString(), m_ui->moveHistoryFrame);
	m_historyPreviousButton = new QPushButton(QIcon(":/images/nav-arrow-left-solid.png"), QString(), m_ui->moveHistoryFrame);
	m_historyNextButton = new QPushButton(QIcon(":/images/nav-arrow-right-solid.png"), QString(), m_ui->moveHistoryFrame);
	m_historyLatestButton = new QPushButton(QIcon(":/images/skip-next-solid.png"), QString(), m_ui->moveHistoryFrame);
	for (QPushButton *button : {m_historyFirstButton, m_historyPreviousButton,
	                            m_historyNextButton, m_historyLatestButton}) {
		button->setFlat(true);
		button->setMinimumWidth(34);
		button->setIconSize(QSize(20, 20));
		historyControls->addWidget(button);
	}
	m_historyFirstButton->setToolTip(tr("Initial position"));
	m_historyPreviousButton->setToolTip(tr("Previous move"));
	m_historyNextButton->setToolTip(tr("Next move"));
	m_historyLatestButton->setToolTip(tr("Latest position"));
	m_ui->moveHistoryLayout->addLayout(historyControls);
	connect(m_historyFirstButton, &QPushButton::clicked, this, [this]() { showHistoryPly(0); });
	connect(m_historyPreviousButton, &QPushButton::clicked, this, [this]() {
		showHistoryPly(m_viewedHistoryPly - 1);
	});
	connect(m_historyNextButton, &QPushButton::clicked, this, [this]() {
		showHistoryPly(m_viewedHistoryPly + 1);
	});
	connect(m_historyLatestButton, &QPushButton::clicked, this, [this]() { showLivePosition(); });
	connect(m_moveTable, &QTableWidget::cellClicked, this, [this](int row, int column) {
		if (column == 0) return;
		const int ply = row * 2 + column;
		if (ply < m_boardHistory.size()) showHistoryPly(ply);
	});
	updateHistoryControls();

	// Compact captured-piece strips, arranged like online chess boards:
	// Black's lost pieces above the moves and White's below them.
	m_capturedBlackLabel = new QLabel(m_ui->moveHistoryFrame);
	m_capturedWhiteLabel = new QLabel(m_ui->moveHistoryFrame);
	for (QLabel *label : {m_capturedBlackLabel, m_capturedWhiteLabel}) {
		label->setFixedHeight(29);
		label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		label->setWordWrap(false);
		label->setTextFormat(Qt::RichText);
		label->setStyleSheet("QLabel { color: #666; font-size: 12px; padding: 1px 4px; }");
	}
	m_capturedBlackLabel->setToolTip(tr("Captured black pieces"));
	m_capturedWhiteLabel->setToolTip(tr("Captured white pieces"));
	m_ui->moveHistoryLayout->insertWidget(0, m_capturedBlackLabel);
	m_ui->moveHistoryLayout->addWidget(m_capturedWhiteLabel);

	// Keep turn information beside the moves. The upper label belongs to the
	// opponent and the lower label belongs to the local player, regardless of
	// which colour each player has in this game.
	QLabel *opponentStatus = m_localplayer_turn == 0
	        ? m_ui->m_player2_result : m_ui->m_player1_result;
	QLabel *localStatus = m_localplayer_turn == 0
	        ? m_ui->m_player1_result : m_ui->m_player2_result;
	m_ui->gridLayout_2->removeWidget(m_ui->m_player1_result);
	m_ui->gridLayout_3->removeWidget(m_ui->m_player2_result);
	opponentStatus->setParent(m_ui->moveHistoryFrame);
	localStatus->setParent(m_ui->moveHistoryFrame);
	for (QLabel *label : {opponentStatus, localStatus}) {
		label->setMinimumHeight(25);
		label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		label->setContentsMargins(4, 0, 4, 0);
	}
	m_ui->moveHistoryLayout->insertWidget(0, opponentStatus);
	m_ui->moveHistoryLayout->addWidget(localStatus);

	QHBoxLayout *gameControls = new QHBoxLayout;
	gameControls->setContentsMargins(2, 2, 2, 2);
	gameControls->setSpacing(2);
	QPushButton *abortButton = new QPushButton(tr("Abort"), m_ui->moveHistoryFrame);
	QPushButton *drawButton = new QPushButton(QString::fromUtf8("½ ") + tr("Draw"), m_ui->moveHistoryFrame);
	QPushButton *resignButton = new QPushButton(tr("Resign"), m_ui->moveHistoryFrame);
	QPushButton *debugButton = new QPushButton(
	        QIcon(":/images/bug.png"), QString(), m_ui->moveHistoryFrame);
	debugButton->setToolTip(tr("Debug"));
	debugButton->setAccessibleName(tr("Debug"));
	debugButton->setIconSize(QSize(18, 18));
	for (QPushButton *button : {abortButton, drawButton, resignButton, debugButton}) {
		button->setMinimumWidth(0);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		button->setStyleSheet("QPushButton { padding: 4px 2px; font-size: 10px; }");
		gameControls->addWidget(button);
	}
	abortButton->setToolTip(tr("Abort game"));
	drawButton->setToolTip(tr("Offer a draw or claim a rule-based draw"));
	resignButton->setToolTip(tr("Resign the game"));
	m_ui->moveHistoryLayout->addLayout(gameControls);

	connect(abortButton, &QPushButton::clicked, this, [this]() {
		if (m_flag_finished || QMessageBox::question(
		        this, tr("Abort game"), tr("Abort this game?")) != QMessageBox::Yes)
			return;
		sendGameAction("abort");
		applyGameAction("abort", false);
	});
	connect(resignButton, &QPushButton::clicked, this, [this]() {
		if (m_flag_finished || QMessageBox::question(
		        this, tr("Resign"), tr("Are you sure you want to resign?")) != QMessageBox::Yes)
			return;
		sendGameAction("resign");
		applyGameAction("resign", false);
	});
	connect(drawButton, &QPushButton::clicked, this, [this]() {
		if (m_flag_finished) return;
		if (turn == m_localplayer_turn && canClaimThreefoldRepetition()) {
			sendGameAction("draw_repetition");
			applyGameAction("draw_repetition", false);
			return;
		}
		if (turn == m_localplayer_turn && canClaimFiftyMoveRule()) {
			sendGameAction("draw_fifty_move");
			applyGameAction("draw_fifty_move", false);
			return;
		}
		sendGameAction("draw_offer");
		m_ui->m_status_bar->setText(tr("Draw offer sent"));
		m_ui->m_status_bar->show();
	});
	connect(debugButton, &QPushButton::clicked, this, &RetroChessWindow::showChessDebugWindow);
	m_debugWidget = new ChessDebugWidget(
	        windowTitle(), [this]() { return currentFen(); }, this);

	m_moveSound = new QMediaPlayer(this);
	m_captureSound = new QMediaPlayer(this);
	m_victorySound = new QMediaPlayer(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	m_moveSound->setAudioOutput(new QAudioOutput(m_moveSound));
	m_captureSound->setAudioOutput(new QAudioOutput(m_captureSound));
	m_victorySound->setAudioOutput(new QAudioOutput(m_victorySound));
	m_moveSound->setSource(QUrl("qrc:/sound/Move.mp3"));
	m_captureSound->setSource(QUrl("qrc:/sound/Capture.mp3"));
	m_victorySound->setSource(QUrl("qrc:/sound/victory.mp3"));
	m_moveSound->audioOutput()->setVolume(0.7f);
	m_captureSound->audioOutput()->setVolume(0.7f);
	m_victorySound->audioOutput()->setVolume(0.8f);
#else
	m_moveSound->setMedia(QUrl("qrc:/sound/Move.mp3"));
	m_captureSound->setMedia(QUrl("qrc:/sound/Capture.mp3"));
	m_victorySound->setMedia(QUrl("qrc:/sound/victory.mp3"));
	m_moveSound->setVolume(70);
	m_captureSound->setVolume(70);
	m_victorySound->setVolume(80);
#endif

	// Keep game notifications out of the sidebar layout. This compact overlay
	// behaves like a status bar and never changes the window's size hint.
	m_ui->gridLayout_4->removeWidget(m_ui->m_status_bar);
	m_ui->m_status_bar->setParent(this);
	m_ui->m_status_bar->setFixedHeight(24);
	m_ui->m_status_bar->setAlignment(Qt::AlignCenter);
	m_ui->m_status_bar->setStyleSheet(
	        "QLabel { background: #fff3cd; color: #7a1f1f; border-top: 1px solid #d6b656; padding: 2px; }");
	m_ui->m_status_bar->setGeometry(0, height() - 24, width(), 24);
	m_ui->m_status_bar->raise();

	if (!mIsGxs) {
		// Direct games need separate lookup paths: getAvatarFromSslId() is for
		// remote peers, while our own node avatar comes from getOwnAvatar().
		QPixmap p1avatar;
		QPixmap p2avatar;
		const RsPeerId ownId = rsPeers->getOwnId();
		auto loadPeerAvatar = [&ownId](const RsPeerId &id, QPixmap &avatar) {
			if (id != ownId) {
				AvatarDefs::getAvatarFromSslId(id, avatar);
				return;
			}

			unsigned char *avatarData = nullptr;
			int avatarSize = 0;
			rsChats->getOwnNodeAvatarData(avatarData, avatarSize);
			if (avatarData)
				free(avatarData);
			if (avatarSize > 0)
				AvatarDefs::getOwnAvatar(avatar);
			else
				// Generate the familiar per-peer coloured fallback instead of
				// RetroShare's static blue missing-avatar image.
				AvatarDefs::getAvatarFromSslId(ownId, avatar);
		};
		loadPeerAvatar(p1id, p1avatar);
		loadPeerAvatar(p2id, p2avatar);

		const QSize avatarSize(128, 128);
		auto setPeerAvatar = [&avatarSize](QLabel *label, const QPixmap &avatar) {
			label->setFixedSize(avatarSize);
			label->setAlignment(Qt::AlignCenter);
			label->setScaledContents(false);
			if (!avatar.isNull())
				label->setPixmap(avatar.scaled(
				        avatarSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		};
		setPeerAvatar(m_ui->m_player1_avatar, p1avatar);
		setPeerAvatar(m_ui->m_player2_avatar, p2avatar);
	} else {
		// GXS mode: retrieve avatars via the GXS identity service.
		// Determine which slot is "us" and which is the remote peer,
		// mirroring the same player/role logic used in the constructor.
		QPixmap p1avatar, p2avatar;
		// p1 is always the identity shown in the player-1 slot (set in constructor)
		// p2 is the identity shown in the player-2 slot
		// The remote peer is mGxsId; our own is myGxsId.
		// Which slot each maps to depends on the player role (set by the constructor).
		RsGxsId slot1Id, slot2Id;
		if (m_localplayer_turn == 0) {
			// We are black (player 1 slot = us, player 2 slot = remote)
			slot1Id = mOwnGxsId;
			slot2Id = mGxsId;
		} else {
			// We are white (player 1 slot = remote, player 2 slot = us)
			slot1Id = mGxsId;
			slot2Id = mOwnGxsId;
		}

		if (!slot1Id.isNull())
			AvatarDefs::getAvatarFromGxsId(slot1Id, p1avatar);
		if (!slot2Id.isNull())
			AvatarDefs::getAvatarFromGxsId(slot2Id, p2avatar);

		const QSize avatarSize(128, 128);
		auto setGxsAvatar = [&avatarSize](QLabel *label, const QPixmap &avatar) {
			label->setFixedSize(avatarSize);
			label->setAlignment(Qt::AlignCenter);
			label->setScaledContents(false);
			label->setPixmap(avatar.scaled(
			        avatarSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		};

		setGxsAvatar(m_ui->m_player1_avatar, p1avatar);
		setGxsAvatar(m_ui->m_player2_avatar, p2avatar);
	}

	// Re-apply the final player-panel order after every accessory has been
	// reparented. For Black, the Black/local frame belongs below the board and
	// the White/opponent frame above it.
	if (m_localplayer_turn == 0) {
		m_ui->gridLayout_4->removeWidget(m_ui->frame);
		m_ui->gridLayout_4->removeWidget(m_ui->frame_2);
		m_ui->gridLayout_4->addWidget(m_ui->frame_2, 0, 0);
		m_ui->gridLayout_4->addWidget(m_ui->frame, 5, 0);
		m_ui->gridLayout_4->invalidate();
		m_ui->gridLayout_4->activate();
	}

	//m_ui->m_move_record->setStyleSheet("QLabel {background-color: white;}");
}

void RetroChessWindow::closeEvent(QCloseEvent *event)
{
    // send leave message
    if (!m_suppressLeave && mIsGxs) {
        rsRetroChess->player_leave_gxs(this->mGxsId);
    } else if (!m_suppressLeave) {
        rsRetroChess->player_leave(mPeerId);
    }

    emit gameClosed(QString::fromStdString(mPeerId));

    QWidget::closeEvent(event);
}

void RetroChessWindow::disOrange()
{
	int i;

	for(i=0; i<max; i++)
		tile[texp[i]/8][texp[i]%8]->tileDisplay();

}

void RetroChessWindow::validate_tile(int row, int col, int c)
{
	Q_UNUSED(c)
	showLivePosition();
	Tile *clickedtile = tile[col][row];
	clickedtile->validate(++count);
}

void RetroChessWindow::initChessBoard()
{
	//QWidget *baseWidget, Tile *tile[8][8]
	QWidget *baseWidget = m_ui->m_chess_board;

	int i,j,k = 0;
	const bool flipped = m_localplayer_turn == 0;

	//borderDisplay (border size: 552 * 552)
	{
		Border::outline( baseWidget,	0,		0,		0);
		Border::outline( baseWidget,	0,		BOARD_INNER_SIZE,	0);
		Border::outline( baseWidget,	0,		BORDER_SIZE,		1);
		Border::outline( baseWidget,	BOARD_INNER_SIZE,	BORDER_SIZE,		1);
	}

	//Create 64 tiles (allocating memories to the objects of Tile class)
	for(i = 0; i < 8; i++)
	{
		for(j=0; j<8; j++)
		{
			tile[i][j] = new Tile(baseWidget);
			tile[i][j]->setChessWindow( this );
			tile[i][j]->tileColor=(i+j)%2;
			tile[i][j]->piece=0;
			tile[i][j]->row=i;
			tile[i][j]->col=j;
			tile[i][j]->tileNum=k++;
			tile[i][j]->tileDisplay();
			const int displayRow = flipped ? 7 - i : i;
			const int displayCol = flipped ? 7 - j : j;
			tile[i][j]->setGeometry(
			        BORDER_SIZE + displayCol * TILE_SIZE,
			        BORDER_SIZE + displayRow * TILE_SIZE,
			        TILE_SIZE, TILE_SIZE);
			tile[i][j]->resize( TILE_SIZE, TILE_SIZE );
		}
	}

	// Board coordinates use the existing 20-pixel border, so they do not
	// increase the board or window dimensions.
	for (i = 0; i < 8; ++i) {
		const int boardIndex = flipped ? 7 - i : i;
		QLabel *rankLabel = new QLabel(QString::number(8 - boardIndex), baseWidget);
		rankLabel->setGeometry(0, BORDER_SIZE + i * TILE_SIZE, BORDER_SIZE, TILE_SIZE);
		rankLabel->setAlignment(Qt::AlignCenter);
		rankLabel->setStyleSheet(
		        "QLabel { color: #353525; background: transparent; font-weight: bold; }");
		rankLabel->raise();

		QLabel *fileLabel = new QLabel(QString(QChar('a' + boardIndex)), baseWidget);
		fileLabel->setGeometry(BORDER_SIZE + i * TILE_SIZE, BOARD_INNER_SIZE, TILE_SIZE, BORDER_SIZE);
		fileLabel->setAlignment(Qt::AlignCenter);
		fileLabel->setStyleSheet(
		        "QLabel { color: #353525; background: transparent; font-weight: bold; }");
		fileLabel->raise();
	}

	//white pawns
	for(j=0; j<8; j++)
	{
		tile[1][j]->piece=1;
		tile[1][j]->pieceColor=0;
		tile[1][j]->display('P');
	}

	//black pawns
	for(j=0; j<8; j++)
	{
		tile[6][j]->piece=1;
		tile[6][j]->pieceColor=1;
		tile[6][j]->display('P');
	}

	//white and black remaining elements
	for(j=0; j<8; j++)
	{
		tile[0][j]->piece=1;
		tile[0][j]->pieceColor=0;
		tile[7][j]->piece=1;
		tile[7][j]->pieceColor=1;
	}

	{
		tile[0][0]->display('R');
		tile[0][1]->display('H');
		tile[0][2]->display('B');
		tile[0][3]->display('Q');
		tile[0][4]->display('K');
		tile[0][5]->display('B');
		tile[0][6]->display('H');
		tile[0][7]->display('R');
	}


	{
		tile[7][0]->display('R');
		tile[7][1]->display('H');
		tile[7][2]->display('B');
		tile[7][3]->display('Q');
		tile[7][4]->display('K');
		tile[7][5]->display('B');
		tile[7][6]->display('H');
		tile[7][7]->display('R');
	}

	wR=7;
	wC=4;

	bR=0;
	bC=4;
	recordCurrentPosition();
	recordBoardSnapshot();
}


int RetroChessWindow::chooser(Tile *tile_p)
{
	switch(tile_p->pieceName)
	{
	case 'P':
		flag=validatePawn(tile_p);
		break;

	case 'R':
		flag=validateRook(tile_p);
		break;

	case 'H':
		flag=validateHorse(tile_p);
		break;

	case 'K':
		flag=validateKing(tile_p);
		break;

	case 'Q':
		flag=validateQueen(tile_p);
		break;

	case 'B':
		flag=validateBishop(tile_p);
		break;

	}

	// Piece validators above generate geometric moves. Remove moves which
	// would leave this player's king in check (including unsafe king moves).
	int legalCount = 0;
	for (int i = 0; i < max; ++i)
	{
		const int toRow = texp[i] / 8;
		const int toCol = texp[i] % 8;
		if (isLegalMove(tile_p->row, tile_p->col, toRow, toCol, tile_p->pieceColor))
			texp[legalCount++] = texp[i];
	}
	max = legalCount;
	flag = max > 0;

	orange();

	return flag;
}

//PAWN
int RetroChessWindow::validatePawn(Tile *tile_p)
{
	int row,col;

	row=tile_p->row;
	col=tile_p->col;
	retVal=0;

	//White Pawn
	if(tile_p->pieceColor)
	{
		if(row-1>=0 && !tile[row-1][col]->piece)
		{
			/*int tnum = tile[row-1][col]->tileNum;
			std::cout << "tile: " << texp[max] << std::endl;
			int a = texp[max];
			texp[max] = tnum;
			max++;*/
			texp[max++]=tile[row-1][col]->tileNum;
			retVal=1;
		}

		if(row==6 && !tile[5][col]->piece && !tile[4][col]->piece)
		{
			texp[max++]=tile[row-2][col]->tileNum;
			retVal=1;
		}

		if(row-1>=0 && col-1>=0)
		{
			if(tile[row-1][col-1]->pieceColor!=tile_p->pieceColor && tile[row-1][col-1]->piece)
			{
				texp[max++]=tile[row-1][col-1]->tileNum;
				retVal=1;
			}
			else if (isEnPassantMove(row, col, row - 1, col - 1, tile_p->pieceColor))
			{
				texp[max++]=tile[row-1][col-1]->tileNum;
				retVal=1;
			}
		}

		if(row-1>=0 && col+1<=7)
		{
			if(tile[row-1][col+1]->pieceColor!=tile_p->pieceColor && tile[row-1][col+1]->piece)
			{
				texp[max++]=tile[row-1][col+1]->tileNum;
				retVal=1;
			}
			else if (isEnPassantMove(row, col, row - 1, col + 1, tile_p->pieceColor))
			{
				texp[max++]=tile[row-1][col+1]->tileNum;
				retVal=1;
			}
		}
	}
	else
	{
		if(row+1<=7 && !tile[row+1][col]->piece)
		{
			texp[max++]=tile[row+1][col]->tileNum;
			retVal=1;
		}

		if(row==1 && !tile[2][col]->piece && !tile[3][col]->piece)
		{
			texp[max++]=tile[row+2][col]->tileNum;
			retVal=1;
		}

		if(row+1<=7 && col-1>=0)
		{
			if(tile[row+1][col-1]->pieceColor!=tile_p->pieceColor && tile[row+1][col-1]->piece)
			{
				texp[max++]=tile[row+1][col-1]->tileNum;
				retVal=1;
			}
			else if (isEnPassantMove(row, col, row + 1, col - 1, tile_p->pieceColor))
			{
				texp[max++]=tile[row+1][col-1]->tileNum;
				retVal=1;
			}
		}

		if(row+1<=7 && col+1<=7)
		{
			if(tile[row+1][col+1]->pieceColor!=tile_p->pieceColor && tile[row+1][col+1]->piece)
			{
				texp[max++]=tile[row+1][col+1]->tileNum;
				retVal=1;
			}
			else if (isEnPassantMove(row, col, row + 1, col + 1, tile_p->pieceColor))
			{
				texp[max++]=tile[row+1][col+1]->tileNum;
				retVal=1;
			}
		}
	}

	return retVal;
}


//ROOK
int RetroChessWindow::validateRook(Tile *tile_p)
{
	int r,c;

	retVal=0;

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}


	return retVal;
}


//HORSE
int RetroChessWindow::validateHorse(Tile *tile_p)
{
	int r,c;
	retVal=0;

	r=tile_p->row;
	c=tile_p->col;

	if(r-2>=0 && c-1>=0)
	{
		if(tile[r-2][c-1]->pieceColor!=tile_p->pieceColor || !tile[r-2][c-1]->piece)
		{
			texp[max++]=tile[r-2][c-1]->tileNum;
			retVal=1;
		}
	}

	if(r-2>=0 && c+1<=7)
	{
		if(tile[r-2][c+1]->pieceColor!=tile_p->pieceColor || !tile[r-2][c+1]->piece)
		{
			texp[max++]=tile[r-2][c+1]->tileNum;
			retVal=1;
		}
	}

	if(r-1>=0 && c-2>=0)
	{
		if(tile[r-1][c-2]->pieceColor!=tile_p->pieceColor || !tile[r-1][c-2]->piece)
		{
			texp[max++]=tile[r-1][c-2]->tileNum;
			retVal=1;
		}
	}

	if(r-1>=0 && c+2<=7)
	{
		if(tile[r-1][c+2]->pieceColor!=tile_p->pieceColor || !tile[r-1][c+2]->piece)
		{
			texp[max++]=tile[r-1][c+2]->tileNum;
			retVal=1;
		}
	}

	if(r+2<=7 && c+1<=7)
	{
		if(tile[r+2][c+1]->pieceColor!=tile_p->pieceColor || !tile[r+2][c+1]->piece)
		{
			texp[max++]=tile[r+2][c+1]->tileNum;
			retVal=1;
		}
	}

	if(r+2<=7 && c-1>=0)
	{
		if(tile[r+2][c-1]->pieceColor!=tile_p->pieceColor || !tile[r+2][c-1]->piece)
		{
			texp[max++]=tile[r+2][c-1]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7 && c-2>=0)
	{
		if(tile[r+1][c-2]->pieceColor!=tile_p->pieceColor || !tile[r+1][c-2]->piece)
		{
			texp[max++]=tile[r+1][c-2]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7 && c+2<=7)
	{
		if(tile[r+1][c+2]->pieceColor!=tile_p->pieceColor || !tile[r+1][c+2]->piece)
		{
			texp[max++]=tile[r+1][c+2]->tileNum;
			retVal=1;
		}
	}

	return retVal;
}


//KING
int RetroChessWindow::validateKing(Tile *tile_p)
{
	int r,c;
	retVal=0;

	r=tile_p->row;
	c=tile_p->col;

	if(r-1>=0)
	{
		if(!tile[r-1][c]->piece || tile[r-1][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r-1][c]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7)
	{
		if(!tile[r+1][c]->piece || tile[r+1][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r+1][c]->tileNum;
			retVal=1;
		}
	}

	if(c-1>=0)
	{
		if(!tile[r][c-1]->piece || tile[r][c-1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c-1]->tileNum;
			retVal=1;
		}
	}

	if(c+1<=7)
	{
		if(!tile[r][c+1]->piece || tile[r][c+1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c+1]->tileNum;
			retVal=1;
		}
	}

	if(r-1>=0 && c-1>=0)
	{
		if(!tile[r-1][c-1]->piece || tile[r-1][c-1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r-1][c-1]->tileNum;
			retVal=1;
		}
	}

	if(r-1>=0 && c+1<=7)
	{
		if(!tile[r-1][c+1]->piece || tile[r-1][c+1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r-1][c+1]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7 && c-1>=0)
	{
		if(!tile[r+1][c-1]->piece || tile[r+1][c-1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r+1][c-1]->tileNum;
			retVal=1;
		}
	}

	if(r+1<=7 && c+1<=7)
	{
		if(!tile[r+1][c+1]->piece || tile[r+1][c+1]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r+1][c+1]->tileNum;
			retVal=1;
		}
	}

	if (canCastle(tile_p->pieceColor, false))
	{
		texp[max++] = tile[r][2]->tileNum;
		retVal = 1;
	}
	if (canCastle(tile_p->pieceColor, true))
	{
		texp[max++] = tile[r][6]->tileNum;
		retVal = 1;
	}

	return retVal;
}


//QUEEN
int RetroChessWindow::validateQueen(Tile *tile_p)
{
	int r,c;

	retVal=0;

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0 && c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0 && c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7 && c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7 && c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}


	return retVal;
}

//BISHOP
int RetroChessWindow::validateBishop(Tile *tile_p)
{
	int r,c;
	retVal=0;

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0 && c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r-->0 && c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7 && c++<7)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	r=tile_p->row;
	c=tile_p->col;
	while(r++<7 && c-->0)
	{
		if(!tile[r][c]->piece)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
		}

		else if(tile[r][c]->pieceColor==tile_p->pieceColor)
			break;

		else if(tile[r][c]->pieceColor!=tile_p->pieceColor)
		{
			texp[max++]=tile[r][c]->tileNum;
			retVal=1;
			break;
		}
	}

	return retVal;
}

// seems like "check" method is check "King"'s status in current situation. (alive or done)
// for help player to make decition to keep "King" alive.
int RetroChessWindow::check(Tile *tile_p)
{
	Q_UNUSED(tile_p)
	return isKingInCheck(turn) ? 1 : 0;
}

bool RetroChessWindow::isSquareAttacked(int row, int col, int attackingColor) const
{
	auto clearPath = [this](int fromRow, int fromCol, int toRow, int toCol) {
		const int rowStep = (toRow > fromRow) - (toRow < fromRow);
		const int colStep = (toCol > fromCol) - (toCol < fromCol);
		for (int r = fromRow + rowStep, c = fromCol + colStep;
		     r != toRow || c != toCol; r += rowStep, c += colStep)
			if (tile[r][c]->piece) return false;
		return true;
	};

	for (int r = 0; r < 8; ++r)
		for (int c = 0; c < 8; ++c)
		{
			const Tile *source = tile[r][c];
			if (!source->piece || source->pieceColor != attackingColor) continue;
			const int dr = row - r;
			const int dc = col - c;
			const int adr = qAbs(dr);
			const int adc = qAbs(dc);
			switch (source->pieceName)
			{
			case 'P':
				if (dr == (attackingColor ? -1 : 1) && adc == 1) return true;
				break;
			case 'H':
				if ((adr == 2 && adc == 1) || (adr == 1 && adc == 2)) return true;
				break;
			case 'K':
				if (qMax(adr, adc) == 1) return true;
				break;
			case 'B':
				if (adr == adc && adr && clearPath(r, c, row, col)) return true;
				break;
			case 'R':
				if (((dr == 0) != (dc == 0)) && clearPath(r, c, row, col)) return true;
				break;
			case 'Q':
				if (((adr == adc && adr) || ((dr == 0) != (dc == 0)))
				        && clearPath(r, c, row, col)) return true;
				break;
			}
		}
	return false;
}

bool RetroChessWindow::isKingInCheck(int color) const
{
	for (int r = 0; r < 8; ++r)
		for (int c = 0; c < 8; ++c)
			if (tile[r][c]->piece && tile[r][c]->pieceColor == color
			        && tile[r][c]->pieceName == 'K')
				return isSquareAttacked(r, c, 1 - color);
	return true; // A malformed position without a king cannot continue.
}

bool RetroChessWindow::isEnPassantMove(
		int fromRow, int fromCol, int toRow, int toCol, int color) const
{
	if (m_enPassantPawnTile < 0 || toRow < 0 || toRow > 7 || toCol < 0 || toCol > 7)
		return false;
	const int direction = color ? -1 : 1;
	if (toRow - fromRow != direction || qAbs(toCol - fromCol) != 1
	        || tile[toRow][toCol]->piece)
		return false;
	const Tile *vulnerablePawn = tile[fromRow][toCol];
	return vulnerablePawn->tileNum == m_enPassantPawnTile
	        && vulnerablePawn->piece && vulnerablePawn->pieceName == 'P'
	        && vulnerablePawn->pieceColor != color;
}

void RetroChessWindow::updateEnPassantTarget(int fromTile, int toTile, char movedPiece)
{
	const int fromRow = fromTile / 8;
	const int toRow = toTile / 8;
	m_enPassantPawnTile = movedPiece == 'P' && qAbs(toRow - fromRow) == 2
	        ? toTile : -1;
}

bool RetroChessWindow::canCastle(int color, bool kingSide) const
{
	const int row = color ? 7 : 0;
	const int side = kingSide ? 1 : 0;
	const int rookCol = kingSide ? 7 : 0;
	const int step = kingSide ? 1 : -1;
	const int throughCol = 4 + step;
	const int destinationCol = 4 + 2 * step;

	if (m_kingMoved[color] || m_rookMoved[color][side]) return false;
	const Tile *king = tile[row][4];
	const Tile *rook = tile[row][rookCol];
	if (!king->piece || king->pieceName != 'K' || king->pieceColor != color
	        || !rook->piece || rook->pieceName != 'R' || rook->pieceColor != color)
		return false;

	for (int col = 4 + step; col != rookCol; col += step)
		if (tile[row][col]->piece) return false;

	const int opponent = 1 - color;
	return !isSquareAttacked(row, 4, opponent)
	        && !isSquareAttacked(row, throughCol, opponent)
	        && !isSquareAttacked(row, destinationCol, opponent);
}

bool RetroChessWindow::isCastlingMove(
		int fromRow, int fromCol, int toRow, int toCol, int color) const
{
	return fromRow == (color ? 7 : 0) && toRow == fromRow && fromCol == 4
	        && (toCol == 2 || toCol == 6) && canCastle(color, toCol == 6);
}

void RetroChessWindow::performCastlingRookMove(int row, bool kingSide)
{
	Tile *rookFrom = tile[row][kingSide ? 7 : 0];
	Tile *rookTo = tile[row][kingSide ? 5 : 3];
	rookTo->piece = 1;
	rookTo->pieceColor = rookFrom->pieceColor;
	rookTo->pieceName = 'R';
	rookFrom->piece = 0;
	rookFrom->display('R');
	rookFrom->tileDisplay();
	rookTo->display('R');
	rookTo->tileDisplay();
}

void RetroChessWindow::updateCastlingRights(
		int fromTile, int toTile, char movedPiece,
		char capturedPiece, int capturedColor)
{
	const int fromRow = fromTile / 8;
	const int fromCol = fromTile % 8;
	const int movedColor = tile[toTile / 8][toTile % 8]->pieceColor;
	if (movedPiece == 'K')
		m_kingMoved[movedColor] = true;
	else if (movedPiece == 'R') {
		if (fromRow == 0 && (fromCol == 0 || fromCol == 7))
			m_rookMoved[0][fromCol == 7 ? 1 : 0] = true;
		else if (fromRow == 7 && (fromCol == 0 || fromCol == 7))
			m_rookMoved[1][fromCol == 7 ? 1 : 0] = true;
	}

	if (capturedPiece == 'R') {
		const int toRow = toTile / 8;
		const int toCol = toTile % 8;
		if (toRow == (capturedColor ? 7 : 0) && (toCol == 0 || toCol == 7))
			m_rookMoved[capturedColor][toCol == 7 ? 1 : 0] = true;
	}
}

char RetroChessWindow::promotionChoiceForPawn(int color)
{
	if (color != m_localplayer_turn) {
		const char choice = m_pendingPromotionChoice;
		m_pendingPromotionChoice = 0;
		return choice == 'Q' || choice == 'R' || choice == 'B' || choice == 'H'
		        ? choice : 'Q';
	}

	// Deliberately parentless: exec() is application-modal anyway, and a
	// stack-allocated child would be double-destroyed if this window got
	// deleted while the nested event loop below is running.
	QDialog dialog;
	dialog.setWindowTitle(tr("Pawn promotion"));
	QVBoxLayout layout(&dialog);
	layout.addWidget(new QLabel(tr("Promote pawn to:"), &dialog));

	QComboBox choices(&dialog);
	choices.setIconSize(QSize(36, 36));
	choices.setMinimumWidth(220);
	const QChar colorCode = color ? 'w' : 'b';
	auto addChoice = [&choices, colorCode](const QString &name, QChar resourceCode, char gameCode) {
		choices.addItem(
		        QIcon(QString(":/piece/%1%2.svg").arg(colorCode).arg(resourceCode)),
		        name, QString(QChar(gameCode)));
	};
	addChoice(tr("Queen"), 'Q', 'Q');
	addChoice(tr("Rook"), 'R', 'R');
	addChoice(tr("Bishop"), 'B', 'B');
	addChoice(tr("Knight"), 'N', 'H');
	layout.addWidget(&choices);

	QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout.addWidget(&buttons);

	char choice = 'Q';
	QPointer<RetroChessWindow> self(this);
	if (dialog.exec() == QDialog::Accepted)
		choice = choices.currentData().toString().at(0).toLatin1();
	// Network slots processed by the nested loop may have destroyed this
	// window. The promotion choice now travels inside the move action, so
	// nothing must be sent from here.
	if (!self)
		return 'Q';
	return choice;
}

bool RetroChessWindow::isPseudoLegalMove(
		int fromRow, int fromCol, int toRow, int toCol, int color) const
{
	if (fromRow < 0 || fromRow > 7 || fromCol < 0 || fromCol > 7
	        || toRow < 0 || toRow > 7 || toCol < 0 || toCol > 7
	        || (fromRow == toRow && fromCol == toCol)) return false;
	const Tile *source = tile[fromRow][fromCol];
	const Tile *target = tile[toRow][toCol];
	if (!source->piece || source->pieceColor != color
	        || (target->piece && target->pieceColor == color)
	        || (target->piece && target->pieceName == 'K')) return false;

	const int dr = toRow - fromRow;
	const int dc = toCol - fromCol;
	const int adr = qAbs(dr);
	const int adc = qAbs(dc);
	auto clearPath = [this](int fr, int fc, int tr, int tc) {
		const int rs = (tr > fr) - (tr < fr);
		const int cs = (tc > fc) - (tc < fc);
		for (int r = fr + rs, c = fc + cs; r != tr || c != tc; r += rs, c += cs)
			if (tile[r][c]->piece) return false;
		return true;
	};

	switch (source->pieceName)
	{
	case 'P': {
		const int direction = color ? -1 : 1;
		const int startRow = color ? 6 : 1;
		if (dc == 0 && !target->piece && dr == direction) return true;
		if (dc == 0 && !target->piece && fromRow == startRow && dr == 2 * direction
		        && !tile[fromRow + direction][fromCol]->piece) return true;
		return adc == 1 && dr == direction
		        && (target->piece || isEnPassantMove(fromRow, fromCol, toRow, toCol, color));
	}
	case 'H': return (adr == 2 && adc == 1) || (adr == 1 && adc == 2);
	case 'K': return qMax(adr, adc) == 1
	        || isCastlingMove(fromRow, fromCol, toRow, toCol, color);
	case 'B': return adr == adc && adr && clearPath(fromRow, fromCol, toRow, toCol);
	case 'R': return ((dr == 0) != (dc == 0)) && clearPath(fromRow, fromCol, toRow, toCol);
	case 'Q': return ((adr == adc && adr) || ((dr == 0) != (dc == 0)))
	                 && clearPath(fromRow, fromCol, toRow, toCol);
	default: return false;
	}
}

bool RetroChessWindow::isLegalMove(
		int fromRow, int fromCol, int toRow, int toCol, int color)
{
	if (!isPseudoLegalMove(fromRow, fromCol, toRow, toCol, color)) return false;
	Tile *source = tile[fromRow][fromCol];
	Tile *target = tile[toRow][toCol];
	const int targetPiece = target->piece;
	const int targetColor = target->pieceColor;
	const char targetName = target->pieceName;
	const int sourcePiece = source->piece;
	const int sourceColor = source->pieceColor;
	const char sourceName = source->pieceName;
	Tile *enPassantPawn = nullptr;
	int enPassantPiece = 0;
	int enPassantColor = 0;
	char enPassantName = 0;
	if (sourceName == 'P' && isEnPassantMove(fromRow, fromCol, toRow, toCol, color)) {
		enPassantPawn = tile[fromRow][toCol];
		enPassantPiece = enPassantPawn->piece;
		enPassantColor = enPassantPawn->pieceColor;
		enPassantName = enPassantPawn->pieceName;
		enPassantPawn->piece = 0;
	}

	source->piece = 0;
	target->piece = 1;
	target->pieceColor = sourceColor;
	target->pieceName = sourceName;
	const bool legal = !isKingInCheck(color);
	source->piece = sourcePiece;
	source->pieceColor = sourceColor;
	source->pieceName = sourceName;
	target->piece = targetPiece;
	target->pieceColor = targetColor;
	target->pieceName = targetName;
	if (enPassantPawn) {
		enPassantPawn->piece = enPassantPiece;
		enPassantPawn->pieceColor = enPassantColor;
		enPassantPawn->pieceName = enPassantName;
	}
	return legal;
}

bool RetroChessWindow::hasAnyLegalMove(int color)
{
	for (int fromRow = 0; fromRow < 8; ++fromRow)
		for (int fromCol = 0; fromCol < 8; ++fromCol)
			if (tile[fromRow][fromCol]->piece && tile[fromRow][fromCol]->pieceColor == color)
				for (int toRow = 0; toRow < 8; ++toRow)
					for (int toCol = 0; toCol < 8; ++toCol)
						if (isLegalMove(fromRow, fromCol, toRow, toCol, color)) return true;
	return false;
}

bool RetroChessWindow::isDeadPosition() const
{
	int bishops = 0;
	int knights = 0;
	int bishopSquareColor = -1;

	for (int row = 0; row < 8; ++row) {
		for (int col = 0; col < 8; ++col) {
			const Tile *square = tile[row][col];
			if (!square->piece || square->pieceName == 'K') continue;

			switch (square->pieceName) {
			case 'B': {
				++bishops;
				const int color = (row + col) % 2;
				if (bishopSquareColor < 0)
					bishopSquareColor = color;
				else if (bishopSquareColor != color)
					return false;
				break;
			}
			case 'H':
				++knights;
				break;
			default:
				// A pawn, rook or queen always leaves mating material.
				return false;
			}
		}
	}

	// K vs K, K+B vs K, and K+N vs K.
	if (bishops + knights <= 1) return true;

	// With no knights and every bishop confined to the same square color,
	// neither side can ever mate (including positions with promoted bishops).
	return knights == 0 && bishops > 0;
}

QString RetroChessWindow::currentPositionKey()
{
	QString key;
	key.reserve(80);
	for (int row = 0; row < 8; ++row) {
		for (int col = 0; col < 8; ++col) {
			const Tile *square = tile[row][col];
			if (!square->piece) {
				key += '.';
				continue;
			}
			QChar piece(square->pieceName);
			key += square->pieceColor ? piece.toUpper() : piece.toLower();
		}
	}

	key += turn ? " w " : " b ";
	key += !m_kingMoved[1] && !m_rookMoved[1][1] ? 'K' : '-';
	key += !m_kingMoved[1] && !m_rookMoved[1][0] ? 'Q' : '-';
	key += !m_kingMoved[0] && !m_rookMoved[0][1] ? 'k' : '-';
	key += !m_kingMoved[0] && !m_rookMoved[0][0] ? 'q' : '-';
	key += ' ';

	// En-passant changes the position only when it gives the side to move an
	// actual legal move; otherwise the available moves are identical.
	int enPassantDestination = -1;
	if (m_enPassantPawnTile >= 0) {
		const int pawnRow = m_enPassantPawnTile / 8;
		const int pawnCol = m_enPassantPawnTile % 8;
		const int destinationRow = pawnRow + (turn ? -1 : 1);
		for (int offset : {-1, 1}) {
			const int fromCol = pawnCol + offset;
			if (fromCol < 0 || fromCol > 7 || destinationRow < 0 || destinationRow > 7)
				continue;
			const Tile *candidate = tile[pawnRow][fromCol];
			if (candidate->piece && candidate->pieceName == 'P'
			        && candidate->pieceColor == turn
			        && isLegalMove(pawnRow, fromCol, destinationRow, pawnCol, turn)) {
				enPassantDestination = destinationRow * 8 + pawnCol;
				break;
			}
		}
	}
	key += enPassantDestination < 0 ? "-" : QString::number(enPassantDestination);
	return key;
}

void RetroChessWindow::recordCurrentPosition()
{
	const QString key = currentPositionKey();
	m_positionOccurrences.insert(key, m_positionOccurrences.value(key) + 1);
}

bool RetroChessWindow::canClaimThreefoldRepetition()
{
	return m_positionOccurrences.value(currentPositionKey()) >= 3;
}

void RetroChessWindow::updateHalfmoveClock(char movedPiece, bool capture)
{
	m_halfmoveClock = movedPiece == 'P' || capture ? 0 : m_halfmoveClock + 1;
}

bool RetroChessWindow::canClaimFiftyMoveRule() const
{
	return m_halfmoveClock >= 100;
}

void RetroChessWindow::highlightCheckedKing(int color)
{
	for (int row = 0; row < 8; ++row)
		for (int col = 0; col < 8; ++col)
		{
			Tile *king = tile[row][col];
			if (!king->piece || king->pieceColor != color || king->pieceName != 'K')
				continue;

			const int kingTile = king->tileNum;
			if (m_checkedKingTile >= 0 && m_checkedKingTile != kingTile)
				tile[m_checkedKingTile / 8][m_checkedKingTile % 8]->tileDisplay();
			m_checkedKingTile = kingTile;

			const RetroChessBoardTheme theme = RetroChessSettings::boardTheme();
			const QColor squareColor = king->tileColor ? theme.dark : theme.light;
			king->setStyleSheet(QString(
			        "QLabel { background: qradialgradient("
			        "cx:0.5, cy:0.5, radius:0.68, fx:0.5, fy:0.5, "
			        "stop:0 rgba(255, 30, 30, 245), "
			        "stop:0.42 rgba(238, 45, 45, 220), "
			        "stop:0.72 rgba(220, 65, 65, 105), "
			        "stop:1 %1); }").arg(squareColor.name()));
			return;
		}
}

void RetroChessWindow::clearKingCheckHighlight()
{
	if (m_checkedKingTile < 0) return;
	tile[m_checkedKingTile / 8][m_checkedKingTile % 8]->tileDisplay();
	m_checkedKingTile = -1;
	// If the restored king square is part of the latest move, preserve that
	// theme-specific last-move highlight.
	drawLastMove();
}

void RetroChessWindow::orange()
{
	const RetroChessBoardTheme theme = RetroChessSettings::boardTheme();
	for (int i = 0; i < max; ++i)
	{
		Tile *destination = tile[texp[i] / 8][texp[i] % 8];
		const QColor square = destination->tileColor ? theme.dark : theme.light;
		QColor marker("#3f8148");
		marker.setAlpha(180);

		QString gradient;
		if (destination->piece)
		{
			// A broad, soft outer ring keeps the captured piece fully visible.
			QColor captureMarker("#7aaa62");
			captureMarker.setAlpha(120);
			gradient = QString(
			        "qradialgradient(cx:0.5, cy:0.5, radius:0.80, fx:0.5, fy:0.5, "
			        "stop:0 %1, stop:0.62 %1, stop:0.64 %2, "
			        "stop:0.95 %2, stop:0.97 %1, stop:1 %1)")
			        .arg(square.name(), captureMarker.name(QColor::HexArgb));
		}
		else
		{
			// Empty legal squares use a small translucent destination dot.
			gradient = QString(
			        "qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, "
			        "stop:0 %1, stop:0.36 %1, stop:0.38 %2, stop:1 %2)")
			        .arg(marker.name(QColor::HexArgb), square.name());
		}
		destination->setStyleSheet(QString("QLabel { background: %1; }").arg(gradient));
	}
}

void RetroChessWindow::recordLastMove(int tile_num)
{
    this->m_last_move_que.push_back(tile_num);
}

void RetroChessWindow::refreshBoardTheme()
{
	const QColor borderColor = RetroChessSettings::boardTheme().dark.lighter(135);
	const QString borderStyle = QString(
	        "QLabel { background-color: %1; color: black; }")
	        .arg(borderColor.name());
	const QList<QLabel*> labels = findChildren<QLabel*>();
	for (QLabel *label : labels)
		if (label->property("retroChessBoardBorder").toBool())
			label->setStyleSheet(borderStyle);

	for (int row = 0; row < 8; ++row)
		for (int column = 0; column < 8; ++column)
			if (tile[row][column])
				tile[row][column]->tileDisplay();

	drawLastMove();
}

void RetroChessWindow::closeForRematch()
{
    m_suppressLeave = true;
    close();
}

void RetroChessWindow::showGameResultDialog(bool localWon, bool draw, const QString &reason)
{
    if (m_resultPopupShown)
        return;
    m_resultPopupShown = true;
	if (!draw && m_victorySound) {
		m_victorySound->stop();
		m_victorySound->setPosition(0);
		m_victorySound->play();
	}

    QDialog *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Game over"));
    dialog->setModal(true);
    dialog->setMinimumWidth(390);
    dialog->setStyleSheet(
        "QDialog { background: #282725; color: white; }"
        "QLabel#resultTitle { font-size: 25px; font-weight: bold; color: white; background: transparent; }"
        "QLabel#resultText { font-size: 15px; color: #d6d6d6; background: transparent; }"
        "QPushButton { min-height: 38px; padding: 4px 22px; font-size: 15px; "
        "background: #3a3937; color: white; border: 1px solid #555; border-radius: 5px; }"
        "QPushButton:hover { background: #4b7f2b; }"
        "QPushButton:disabled { color: #888; background: #333; }");

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);

    QLabel *title = new QLabel(draw ? tr("Draw") : (localWon ? tr("You won!") : tr("You lost")), dialog);
    title->setObjectName("resultTitle");
    title->setAlignment(Qt::AlignCenter);
    QLabel *message = new QLabel(!reason.isEmpty() ? reason : (draw ? tr("The game ended in a draw.")
                                      : (localWon ? tr("Congratulations — the game is over.")
                                                  : tr("The game is over. Ready for another one?"))), dialog);
    message->setObjectName("resultText");
    message->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    layout->addWidget(message);

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *lobbyButton = new QPushButton(tr("Chess lobby"), dialog);
    QPushButton *rematchButton = new QPushButton(tr("Rematch"), dialog);
    buttons->addWidget(lobbyButton);
    buttons->addWidget(rematchButton);
    layout->addLayout(buttons);

    connect(lobbyButton, &QPushButton::clicked, this, [this, dialog]() {
        dialog->close();
        close();
    });
    connect(rematchButton, &QPushButton::clicked, this, [this, dialog]() {
        dialog->close();
        if (mIsGxs)
            emit rematchRequested(mGxsId, m_localplayer_turn);
        else
            emit rematchRequestedPeer(QString::fromStdString(mPeerId), m_localplayer_turn);
    });
    dialog->show();
}

void RetroChessWindow::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (m_ui && m_ui->m_status_bar) {
		m_ui->m_status_bar->setGeometry(0, height() - 24, width(), 24);
		m_ui->m_status_bar->raise();
	}
}

void RetroChessWindow::recordBoardSnapshot(int fromTile, int toTile)
{
	QString position;
	position.reserve(64);
	for (int row = 0; row < 8; ++row)
		for (int col = 0; col < 8; ++col) {
			const Tile *square = tile[row][col];
			if (!square->piece) position += '.';
			else {
				QChar piece(square->pieceName);
				position += square->pieceColor ? piece.toUpper() : piece.toLower();
			}
		}
	m_boardHistory.push_back(position);
	m_boardHistoryMoves.push_back(qMakePair(fromTile, toTile));
	m_viewedHistoryPly = m_boardHistory.size() - 1;
	updateHistoryControls();
}

void RetroChessWindow::showHistoryPly(int ply)
{
	if (m_boardHistory.isEmpty()) return;
	ply = qBound(0, ply, m_boardHistory.size() - 1);
	m_viewedHistoryPly = ply;
	const QString &position = m_boardHistory.at(ply);
	for (int index = 0; index < 64; ++index) {
		const QChar encoded = position.at(index);
		Tile *square = tile[index / 8][index % 8];
		square->tileDisplay();
		square->displayPosition(encoded != '.', encoded.toUpper().toLatin1(), encoded.isUpper());
	}

	const QPair<int, int> move = m_boardHistoryMoves.at(ply);
	const QString highlightStyle = QString("QLabel { background-color: %1; }")
	        .arg(RetroChessSettings::boardTheme().lastMove.name());
	if (move.first >= 0) tile[move.first / 8][move.first % 8]->setStyleSheet(highlightStyle);
	if (move.second >= 0) tile[move.second / 8][move.second % 8]->setStyleSheet(highlightStyle);

	if (ply == 0) m_moveTable->clearSelection();
	else {
		const int row = (ply - 1) / 2;
		const int column = (ply - 1) % 2 + 1;
		if (QTableWidgetItem *item = m_moveTable->item(row, column)) {
			m_moveTable->setCurrentItem(item);
			m_moveTable->scrollToItem(item);
		}
	}
	updateHistoryControls();
}

void RetroChessWindow::showLivePosition()
{
	if (!m_boardHistory.isEmpty()) showHistoryPly(m_boardHistory.size() - 1);
}

void RetroChessWindow::updateHistoryControls()
{
	const int latest = m_boardHistory.size() - 1;
	const bool canGoBack = latest >= 0 && m_viewedHistoryPly > 0;
	const bool canGoForward = latest >= 0 && m_viewedHistoryPly < latest;
	if (m_historyFirstButton) m_historyFirstButton->setEnabled(canGoBack);
	if (m_historyPreviousButton) m_historyPreviousButton->setEnabled(canGoBack);
	if (m_historyNextButton) m_historyNextButton->setEnabled(canGoForward);
	if (m_historyLatestButton) m_historyLatestButton->setEnabled(canGoForward);
}

void RetroChessWindow::recordMove(
		int fromTile, int toTile, char pieceName, bool capture, char promotion)
{
	auto squareName = [](int tileNumber) {
		const QChar file('a' + tileNumber % 8);
		const QChar rank('8' - tileNumber / 8);
		return QString(file) + QString(rank);
	};

	QString notation;
	if (pieceName == 'K' && qAbs(toTile % 8 - fromTile % 8) == 2) {
		notation = toTile % 8 == 6 ? "O-O" : "O-O-O";
	} else {
		if (pieceName != 'P') notation += QChar(pieceName == 'H' ? 'N' : pieceName);
		notation += squareName(fromTile);
		notation += capture ? "x" : "-";
		notation += squareName(toTile);
		if (promotion)
			notation += QString("=%1").arg(QChar(promotion == 'H' ? 'N' : promotion));
	}
	m_move_history.push_back(notation);

	const int row = (m_move_history.size() - 1) / 2;
	if (m_moveTable->rowCount() <= row) {
		m_moveTable->insertRow(row);
		m_moveTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
		m_moveTable->setRowHeight(row, 23);
	}
	const int column = (m_move_history.size() % 2) ? 1 : 2;
	m_moveTable->setItem(row, column, new QTableWidgetItem(notation));
	m_moveTable->scrollToBottom();
}

void RetroChessWindow::recordCapturedPiece(char pieceName, int pieceColor)
{
	if (pieceColor == 0) {
		m_capturedBlack.append(QChar(pieceName));
	} else {
		m_capturedWhite.append(QChar(pieceName));
	}

	auto compactPieces = [](const QString &pieces, const QString &color) {
		QString result;
		const QString order = "QRRBBHHPPPPPPPPK";
		QString rendered;
		for (const QChar piece : order) {
			if (rendered.contains(piece) || !pieces.contains(piece))
				continue;
			rendered += piece;
			QChar pieceCode;
			switch (piece.toLatin1()) {
			case 'K': pieceCode = 'K'; break;
			case 'Q': pieceCode = 'Q'; break;
			case 'R': pieceCode = 'R'; break;
			case 'B': pieceCode = 'B'; break;
			case 'H': pieceCode = 'N'; break;
			default: pieceCode = 'P'; break;
			}
			const int amount = pieces.count(piece);
			const QChar colorCode = color == "black" ? 'b' : 'w';
			result += QString("<img src=\":/piece/%1%2.svg\" width=\"20\" height=\"20\">")
			        .arg(colorCode).arg(pieceCode);
			if (amount > 1)
				result += QString("×%1").arg(amount);
			result += "&nbsp;";
		}
		return result;
	};
	m_capturedBlackLabel->setText(compactPieces(m_capturedBlack, "black"));
	m_capturedWhiteLabel->setText(compactPieces(m_capturedWhite, "white"));
}

void RetroChessWindow::playMoveSound(bool capture)
{
	QMediaPlayer *player = capture ? m_captureSound : m_moveSound;
	if (!player)
		return;
	player->stop();
	player->setPosition(0);
	player->play();
}

static QString chessSquareName(int tileNumber)
{
	if (tileNumber < 0 || tileNumber >= 64) return QStringLiteral("??");
	return QString(QChar('a' + tileNumber % 8)) + QString::number(8 - tileNumber / 8);
}

QString RetroChessWindow::currentFen() const
{
	QString placement;
	for (int row = 0; row < 8; ++row) {
		int empty = 0;
		for (int col = 0; col < 8; ++col) {
			const Tile *square = tile[row][col];
			if (!square->piece) {
				++empty;
				continue;
			}
			if (empty) {
				placement += QString::number(empty);
				empty = 0;
			}
			QChar piece(square->pieceName == 'H' ? 'N' : square->pieceName);
			placement += square->pieceColor ? piece.toUpper() : piece.toLower();
		}
		if (empty) placement += QString::number(empty);
		if (row != 7) placement += '/';
	}

	QString castling;
	auto hasPiece = [this](int row, int col, char name, int color) {
		const Tile *square = tile[row][col];
		return square->piece && square->pieceName == name && square->pieceColor == color;
	};
	if (!m_kingMoved[1] && !m_rookMoved[1][1]
	        && hasPiece(7, 4, 'K', 1) && hasPiece(7, 7, 'R', 1)) castling += 'K';
	if (!m_kingMoved[1] && !m_rookMoved[1][0]
	        && hasPiece(7, 4, 'K', 1) && hasPiece(7, 0, 'R', 1)) castling += 'Q';
	if (!m_kingMoved[0] && !m_rookMoved[0][1]
	        && hasPiece(0, 4, 'K', 0) && hasPiece(0, 7, 'R', 0)) castling += 'k';
	if (!m_kingMoved[0] && !m_rookMoved[0][0]
	        && hasPiece(0, 4, 'K', 0) && hasPiece(0, 0, 'R', 0)) castling += 'q';
	if (castling.isEmpty()) castling = "-";

	QString enPassant = "-";
	if (m_enPassantPawnTile >= 0) {
		const Tile *pawn = tile[m_enPassantPawnTile / 8][m_enPassantPawnTile % 8];
		if (pawn->piece && pawn->pieceName == 'P') {
			const int targetRow = pawn->row + (pawn->pieceColor ? 1 : -1);
			enPassant = QString(QChar('a' + pawn->col)) + QString::number(8 - targetRow);
		}
	}
	return QString("%1 %2 %3 %4 %5 %6").arg(
	        placement, turn ? "w" : "b", castling, enPassant,
	        QString::number(m_halfmoveClock), QString::number(m_fullmoveNumber));
}

QString RetroChessWindow::positionHash() const
{
	return ChessDebugWidget::hashFen(currentFen());
}

void RetroChessWindow::appendDebugEvent(const QString &event)
{
	if (m_debugWidget) m_debugWidget->appendEvent(event);
}

void RetroChessWindow::sendMoveAction(int fromTile, int toTile, char promotion)
{
	const int sequence = m_boardHistory.size() - 1;
	const QString hash = positionHash();
	const QString move = chessSquareName(fromTile) + chessSquareName(toTile)
	        + (promotion == '-' ? QString() : QString(QChar(promotion)).toLower());
	appendDebugEvent(QString("TX move %1 sequence=%2 hash=%3 FEN=%4")
	        .arg(move).arg(sequence).arg(hash, currentFen()));
	sendGameAction(QString("move:%1:%2:%3:%4:%5")
	        .arg(sequence).arg(fromTile).arg(toTile).arg(QChar(promotion)).arg(hash));
}

bool RetroChessWindow::loadFen(const QString &fen, QString *error)
{
	const QStringList fields = fen.simplified().split(' ');
	if (fields.size() != 6) {
		if (error) *error = tr("FEN must contain exactly six fields.");
		return false;
	}
	const QStringList ranks = fields.at(0).split('/');
	if (ranks.size() != 8 || (fields.at(1) != "w" && fields.at(1) != "b")) {
		if (error) *error = tr("Invalid board or side-to-move field.");
		return false;
	}

	struct FenPiece { int row; int col; char name; int color; };
	QVector<FenPiece> pieces;
	int kings[2] = {0, 0};
	for (int row = 0; row < 8; ++row) {
		int col = 0;
		for (const QChar encoded : ranks.at(row)) {
			if (encoded.isDigit()) {
				const int empty = encoded.digitValue();
				if (empty < 1 || empty > 8) col = 99;
				else col += empty;
				continue;
			}
			const QChar upper = encoded.toUpper();
			if (col >= 8 || !QString("KQRBNP").contains(upper)) { col = 99; break; }
			const char name = upper == 'N' ? 'H' : upper.toLatin1();
			const int color = encoded.isUpper() ? 1 : 0;
			pieces.push_back({row, col++, name, color});
			if (name == 'K') ++kings[color];
		}
		if (col != 8) {
			if (error) *error = tr("Every FEN rank must contain eight squares.");
			return false;
		}
	}
	if (kings[0] != 1 || kings[1] != 1) {
		if (error) *error = tr("FEN must contain exactly one king of each color.");
		return false;
	}
	bool halfOk = false, fullOk = false;
	const int halfmove = fields.at(4).toInt(&halfOk);
	const int fullmove = fields.at(5).toInt(&fullOk);
	if (!halfOk || !fullOk || halfmove < 0 || fullmove < 1) {
		if (error) *error = tr("Invalid FEN move counters.");
		return false;
	}

	for (int row = 0; row < 8; ++row)
		for (int col = 0; col < 8; ++col) {
			tile[row][col]->piece = 0;
			tile[row][col]->pieceName = 0;
			tile[row][col]->display(0);
			tile[row][col]->tileDisplay();
		}
	for (const FenPiece &piece : pieces) {
		Tile *square = tile[piece.row][piece.col];
		square->piece = 1;
		square->pieceName = piece.name;
		square->pieceColor = piece.color;
		square->display(piece.name);
	}
	turn = fields.at(1) == "w" ? 1 : 0;
	const QString rights = fields.at(2);
	m_kingMoved[1] = !rights.contains('K') && !rights.contains('Q');
	m_kingMoved[0] = !rights.contains('k') && !rights.contains('q');
	m_rookMoved[1][1] = !rights.contains('K');
	m_rookMoved[1][0] = !rights.contains('Q');
	m_rookMoved[0][1] = !rights.contains('k');
	m_rookMoved[0][0] = !rights.contains('q');
	m_enPassantPawnTile = -1;
	if (fields.at(3) != "-") {
		const QString ep = fields.at(3).toLower();
		if (ep.size() != 2 || ep.at(0) < 'a' || ep.at(0) > 'h'
		        || (ep.at(1) != '3' && ep.at(1) != '6')) {
			if (error) *error = tr("Invalid FEN en-passant square.");
			return false;
		}
		const int targetCol = ep.at(0).toLatin1() - 'a';
		const int targetRow = 8 - ep.at(1).digitValue();
		const int pawnRow = targetRow + (turn ? 1 : -1);
		Tile *pawn = tile[pawnRow][targetCol];
		if (pawn->piece && pawn->pieceName == 'P' && pawn->pieceColor != turn)
			m_enPassantPawnTile = pawn->tileNum;
	}
	m_halfmoveClock = halfmove;
	m_fullmoveNumber = fullmove;
	m_flag_finished = 0;
	m_desynchronized = false;
	count = max = 0;
	click1 = nullptr;
	m_move_history.clear();
	m_moveTable->setRowCount(0);
	m_positionOccurrences.clear();
	m_boardHistory.clear();
	m_boardHistoryMoves.clear();
	clearLastMove();
	recordCurrentPosition();
	recordBoardSnapshot();
	playerTurnNotice();
	return true;
}

void RetroChessWindow::showChessDebugWindow()
{
	if (!m_debugWidget) return;
	m_debugWidget->refresh();
	m_debugWidget->show();
	m_debugWidget->raise();
	m_debugWidget->activateWindow();
}

void RetroChessWindow::updateDebugWindow()
{
	if (m_debugWidget) m_debugWidget->refresh();
}

void RetroChessWindow::stopForDesynchronization(const QString &reason)
{
	if (m_desynchronized) return;
	m_desynchronized = true;
	m_flag_finished = 7;
	appendDebugEvent("DESYNC: " + reason + " Local FEN=" + currentFen());
	showGameStatus(tr("Boards are out of sync — game paused"));
	QMessageBox::critical(this, tr("Chess synchronization error"),
	        tr("Boards are out of sync and the game has been paused.\n\n%1\n\n"
	           "Open Debug to save the report.").arg(reason));
}

void RetroChessWindow::sendGameAction(const QString &action)
{
	if (mIsGxs) {
		rsRetroChess->sendGameActionGxs(mGxsId, action.toStdString());
	} else {
		QVariantMap map;
		map.insert("type", "game_action");
		map.insert("action", action);
		rsRetroChess->qvm_msg_peer(RsPeerId(mPeerId), map);
	}
}

void RetroChessWindow::showGameStatus(const QString &status)
{
	m_ui->m_status_bar->setText(status);
	m_ui->m_status_bar->show();
}

void RetroChessWindow::applyGameAction(const QString &action, bool remote)
{
	if (action.startsWith("move:")) {
		if (!remote || m_flag_finished || turn == m_localplayer_turn) return;
		const QStringList parts = action.split(':');
		const bool verifiedPacket = parts.size() == 6;
		const bool legacyPacket = parts.size() == 4;
		bool sequenceOk = legacyPacket;
		bool fromOk = false, toOk = false;
		const int sequence = verifiedPacket ? parts.at(1).toInt(&sequenceOk) : -1;
		const int fromIndex = verifiedPacket ? 2 : 1;
		const int toIndex = verifiedPacket ? 3 : 2;
		const int promotionIndex = verifiedPacket ? 4 : 3;
		const int fromTile = (verifiedPacket || legacyPacket)
		        ? parts.at(fromIndex).toInt(&fromOk) : -1;
		const int toTile = (verifiedPacket || legacyPacket)
		        ? parts.at(toIndex).toInt(&toOk) : -1;
		const char promotion = (verifiedPacket || legacyPacket)
		        && !parts.at(promotionIndex).isEmpty()
		        ? parts.at(promotionIndex).at(0).toLatin1() : '-';
		if (!verifiedPacket && !legacyPacket) {
			stopForDesynchronization(tr("Malformed move packet"));
			return;
		}
		if (verifiedPacket && (!sequenceOk || sequence != m_boardHistory.size())) {
			stopForDesynchronization(tr("Expected move sequence %1 but received %2")
			        .arg(m_boardHistory.size()).arg(sequence));
			return;
		}
		if (!fromOk || !toOk || fromTile < 0 || fromTile >= 64
		        || toTile < 0 || toTile >= 64) {
			stopForDesynchronization(tr("Move packet contains an invalid square"));
			return;
		}

		Tile *source = tile[fromTile / 8][fromTile % 8];
		if (!source->piece || source->pieceColor != turn) {
			stopForDesynchronization(tr("Received move %1%2 for the wrong piece or turn")
			        .arg(chessSquareName(fromTile), chessSquareName(toTile)));
			return;
		}
		appendDebugEvent(QString("RX move %1%2 sequence=%3 remoteHash=%4")
		        .arg(chessSquareName(fromTile), chessSquareName(toTile))
		        .arg(verifiedPacket ? QString::number(sequence) : "legacy")
		        .arg(verifiedPacket ? parts.at(5) : "unavailable"));
		if (promotion == 'Q' || promotion == 'R' || promotion == 'B' || promotion == 'H')
			m_pendingPromotionChoice = promotion;
		else
			m_pendingPromotionChoice = 0;

		if (click1) click1->tileDisplay();
		disOrange();
		click1 = nullptr;
		max = 0;
		count = 0;
		source->validate(++count);
		const bool moved = tile[toTile / 8][toTile % 8]->validate(++count);
		if (!moved) {
			if (click1) click1->tileDisplay();
			disOrange();
			click1 = nullptr;
			max = 0;
			count = 0;
			m_pendingPromotionChoice = 0;
			stopForDesynchronization(tr("Received move %1%2 is illegal in the local position")
			        .arg(chessSquareName(fromTile), chessSquareName(toTile)));
			return;
		}
		if (verifiedPacket) {
			const QString localHash = positionHash();
			const QString remoteHash = parts.at(5);
			if (localHash != remoteHash) {
				stopForDesynchronization(tr("Position hash differs after move %1: local %2, remote %3")
				        .arg(sequence).arg(localHash, remoteHash));
				return;
			}
			appendDebugEvent(QString("SYNC OK sequence=%1 hash=%2 FEN=%3")
			        .arg(sequence).arg(localHash, currentFen()));
		} else {
			appendDebugEvent("Legacy move accepted without sequence/hash verification");
		}
		return;
	}
	if (action.startsWith("promotion:")) {
		const char choice = action.size() > 10 ? action.at(10).toLatin1() : 'Q';
		if (choice == 'Q' || choice == 'R' || choice == 'B' || choice == 'H')
			m_pendingPromotionChoice = choice;
		return;
	}
	if (action == "draw_repetition") {
		if (!canClaimThreefoldRepetition()) return;
		m_flag_finished = 1;
		m_suppressLeave = true;
		showGameResultDialog(false, true, tr("Draw by threefold repetition"));
		emit gameEnded(QString::fromStdString(mPeerId));
		return;
	}
	if (action == "draw_fifty_move") {
		if (!canClaimFiftyMoveRule()) return;
		m_flag_finished = 1;
		m_suppressLeave = true;
		showGameResultDialog(false, true, tr("Draw by the 50-move rule"));
		emit gameEnded(QString::fromStdString(mPeerId));
		return;
	}
	if (action == "draw_offer" && remote) {
		// The question box spins a nested event loop; other network slots may
		// destroy this window meanwhile (rematch accept, close). Do not touch
		// any member after exec without checking.
		QPointer<RetroChessWindow> self(this);
		const bool accepted = QMessageBox::question(
		        this, tr("Draw offer"), tr("Your opponent offers a draw. Accept?"),
		        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
		if (!self)
			return;
		sendGameAction(accepted ? "draw_accept" : "draw_decline");
		if (accepted) applyGameAction("draw_accept", false);
		return;
	}
	if (action == "draw_decline") {
		const QString declinedText = tr("Draw offer declined");
		m_ui->m_status_bar->setText(declinedText);
		m_ui->m_status_bar->show();
		QTimer::singleShot(3500, this, [this, declinedText]() {
			if (m_ui->m_status_bar->text() == declinedText)
				m_ui->m_status_bar->hide();
		});
		return;
	}
	if (action == "rematch_decline") {
		m_rematchRequested = false;
		m_ui->m_status_bar->setText(tr("Rematch request declined"));
		m_ui->m_status_bar->show();
		return;
	}
	if (m_flag_finished) return;

	if (action == "abort") {
		m_flag_finished = 1;
		m_suppressLeave = true;
		m_ui->m_status_bar->setText(remote ? tr("Opponent aborted the game") : tr("Game aborted"));
		m_ui->m_status_bar->show();
		emit gameEnded(QString::fromStdString(mPeerId));
	} else if (action == "resign") {
		m_flag_finished = 1;
		m_suppressLeave = true;
		showGameResultDialog(remote);
		emit gameEnded(QString::fromStdString(mPeerId));
	} else if (action == "draw_accept") {
		m_flag_finished = 1;
		m_suppressLeave = true;
		showGameResultDialog(false, true);
		emit gameEnded(QString::fromStdString(mPeerId));
	}
}

void RetroChessWindow::drawLastMove()
{
    const QString highlightStyle = QString("QLabel { background-color: %1; }")
            .arg(RetroChessSettings::boardTheme().lastMove.name());
    // draw last move
    for( QQueue<int>::iterator it = this->m_last_move_que.begin();
         it != this->m_last_move_que.end();
         ++it)
    {
        int tile_num = *it;
        tile[ tile_num / 8][ tile_num % 8]->setStyleSheet(highlightStyle);
    }
}

void RetroChessWindow::clearLastMove()
{
    while( !this->m_last_move_que.empty())
    {
        int tile_num = this->m_last_move_que.front();
        this->m_last_move_que.pop_front();

        tile[ tile_num / 8][ tile_num % 8]->tileDisplay();	// revoery tile's background color
    }	// clear the last move queue
}

// 0: ongoing, 1: black win, 2: white win, 3: stalemate, 4: dead position,
// 5: fivefold repetition, 6: 75-move rule
int RetroChessWindow::resultJudge()
{
	if (isDeadPosition()) {
		clearKingCheckHighlight();
		showGameResultDialog(false, true, tr("Draw by dead position"));
		return 4;
	}

	if (hasAnyLegalMove(turn)) {
		// FIDE 9.6 draws are automatic. They are checked only after confirming
		// that the last move did not produce checkmate or stalemate.
		if (m_positionOccurrences.value(currentPositionKey()) >= 5) {
			clearKingCheckHighlight();
			showGameResultDialog(false, true, tr("Draw by fivefold repetition"));
			return 5;
		}
		if (m_halfmoveClock >= 150) {
			clearKingCheckHighlight();
			showGameResultDialog(false, true, tr("Draw by the 75-move rule"));
			return 6;
		}
		if (isKingInCheck(turn)) {
			highlightCheckedKing(turn);
			showGameStatus(tr("Check"));
		} else {
			clearKingCheckHighlight();
			if (m_ui->m_status_bar->text() == tr("Check")) m_ui->m_status_bar->hide();
		}
		return 0;
	}

	if (!isKingInCheck(turn)) {
		showGameResultDialog(false, true, tr("Draw by stalemate"));
		return 3;
	}

	const int winningColor = 1 - turn;
	highlightCheckedKing(turn);
	QLabel *blackResult = m_ui->m_player1_result;
	QLabel *whiteResult = m_ui->m_player2_result;
	blackResult->setText(winningColor == 0 ? tr("Win") : tr("Defeat"));
	whiteResult->setText(winningColor == 1 ? tr("Win") : tr("Defeat"));
	blackResult->setStyleSheet(winningColor == 0 ? "QLabel {color: green;}" : "QLabel {color: red;}");
	whiteResult->setStyleSheet(winningColor == 1 ? "QLabel {color: green;}" : "QLabel {color: red;}");
	blackResult->setVisible(true);
	whiteResult->setVisible(true);
	showGameResultDialog(m_localplayer_turn == winningColor, false, tr("by checkmate"));
	return winningColor == 0 ? 1 : 2;
}

void RetroChessWindow::showPlayerLeaveMsg()
{
	// Stop all local interaction as soon as the opponent leaves.
	m_flag_finished = 1;
    QString name;
    if (mIsGxs) {
        // Resolve GXS nickname
        RsIdentityDetails details;
        if (rsIdentity->getIdDetails(mGxsId, details)) {
            name = QString::fromUtf8(details.mNickname.c_str());
        } else {
            name = tr("Distant Friend");
        }
    } else {
        // Resolve Peer name
        name = QString::fromStdString(rsPeers->getPeerName(RsPeerId(mPeerId)));
    }

    QString status_msg = name + tr(" has left");
    m_ui->m_status_bar->setText(status_msg);
    m_ui->m_status_bar->setVisible(true);
}

void RetroChessWindow::playerTurnNotice()
{
	QLabel *opponentStatus = m_localplayer_turn == 0
	        ? m_ui->m_player2_result : m_ui->m_player1_result;
	QLabel *localStatus = m_localplayer_turn == 0
	        ? m_ui->m_player1_result : m_ui->m_player2_result;
	const bool localTurn = turn == m_localplayer_turn;

	opponentStatus->setText(tr("Opponent's turn"));
	localStatus->setText(localTurn ? tr("Your turn") : tr("Waiting for opponent"));
	opponentStatus->setStyleSheet("QLabel { font-size: 13px; color: #555; }");
	localStatus->setStyleSheet(localTurn
	        ? "QLabel { font-size: 13px; color: green; font-weight: bold; }"
	        : "QLabel { font-size: 13px; color: #777; }");
	opponentStatus->setVisible(!localTurn);
	localStatus->show();
}
