#ifndef RETROCHESSUSERNOTIFY_H
#define RETROCHESSUSERNOTIFY_H

#include "gui/common/UserNotify.h"

class NEMainpage;

class RetroChessUserNotify : public UserNotify
{
	Q_OBJECT

public:
	RetroChessUserNotify(NEMainpage *page, QObject *parent);

private:
	QIcon getIcon() override;
	QIcon getMainIcon(bool hasNew) override;
	unsigned int getNewCount() override;
	void iconClicked() override;

	NEMainpage *mPage;
};

#endif // RETROCHESSUSERNOTIFY_H
