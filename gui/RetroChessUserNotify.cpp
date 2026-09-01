#include "RetroChessUserNotify.h"

#include "NEMainpage.h"
#include "gui/MainWindow.h"

RetroChessUserNotify::RetroChessUserNotify(NEMainpage *page, QObject *parent)
	: UserNotify(parent), mPage(page)
{
	connect(page, &NEMainpage::lobbyUnreadCountChanged,
	        this, &UserNotify::updateIcon);
}

QIcon RetroChessUserNotify::getIcon()
{
	return QIcon(":/images/chess-notify.png");
}

QIcon RetroChessUserNotify::getMainIcon(bool hasNew)
{
	return QIcon(hasNew ? ":/images/chess-notify.png" : ":/images/chess.png");
}

unsigned int RetroChessUserNotify::getNewCount()
{
	return mPage->notificationCount();
}

void RetroChessUserNotify::iconClicked()
{
	MainWindow::showWindow(mPage);
}
