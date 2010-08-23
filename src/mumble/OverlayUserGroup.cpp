/* Copyright (C) 2005-2010, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "Overlay.h"
#include "OverlayText.h"
#include "User.h"
#include "Channel.h"
#include "Global.h"
#include "Message.h"
#include "Database.h"
#include "NetworkConfig.h"
#include "ServerHandler.h"
#include "MainWindow.h"
#include "GlobalShortcut.h"

OverlayUserGroup::OverlayUserGroup(OverlaySettings *osptr) :
	OverlayGroup(),
	os(osptr),
	qgeiHandle(NULL),
	bShowExamples(false)
{ }

OverlayUserGroup::~OverlayUserGroup() {
	reset();
}

void OverlayUserGroup::reset() {
	foreach(OverlayUser *ou, qlExampleUsers)
		delete ou;
	qlExampleUsers.clear();

	foreach(OverlayUser *ou, qmUsers)
		delete ou;
	qmUsers.clear();

	delete qgeiHandle;
	qgeiHandle = NULL;
}

int OverlayUserGroup::type() const {
	return Type;
}

void OverlayUserGroup::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
	event->accept();

	QMenu qm(g.mw);
	QMenu *qmShow = qm.addMenu(OverlayClient::tr("Filter"));

	QAction *qaShowTalking = qmShow->addAction(OverlayClient::tr("Only talking"));
	qaShowTalking->setCheckable(true);
	if (os->osShow == OverlaySettings::Talking)
		qaShowTalking->setChecked(true);

	QAction *qaShowHome = qmShow->addAction(OverlayClient::tr("All in current channel"));
	qaShowHome->setCheckable(true);
	if (os->osShow == OverlaySettings::HomeChannel)
		qaShowHome->setChecked(true);

	QAction *qaShowLinked = qmShow->addAction(OverlayClient::tr("All in linked channels"));
	qaShowLinked->setCheckable(true);
	if (os->osShow == OverlaySettings::LinkedChannels)
		qaShowLinked->setChecked(true);

	qmShow->addSeparator();

	QAction *qaShowSelf = qmShow->addAction(OverlayClient::tr("Always show yourself"));
	qaShowSelf->setCheckable(true);
	qaShowSelf->setEnabled(os->osShow == OverlaySettings::Talking);
	if (os->bAlwaysSelf)
		qaShowSelf->setChecked(true);

	QMenu *qmColumns = qm.addMenu(OverlayClient::tr("Columns"));
	QAction *qaColumns[6];
	for (unsigned int i=1;i<=5;++i) {
		qaColumns[i] = qmColumns->addAction(QString::number(i));
		qaColumns[i]->setCheckable(true);
		qaColumns[i]->setChecked(i == os->uiColumns);
	}

	QAction *qaEdit = qm.addAction(OverlayClient::tr("Edit..."));
	QAction *qaZoom = qm.addAction(OverlayClient::tr("Reset Zoom"));

	QAction *act = qm.exec(event->screenPos());

	if (! act)
		return;

	if (act == qaEdit) {
		if (g.ocIntercept) {
			QMetaObject::invokeMethod(g.ocIntercept, "openEditor", Qt::QueuedConnection);
		} else {
			OverlayEditor oe(qApp->activeModalWidget(), NULL, os);
			connect(&oe, SIGNAL(applySettings()), this, SLOT(updateLayout()));
			oe.exec();
		}
	} else if (act == qaZoom) {
		os->fZoom = 1.0f;
		updateLayout();
	} else if (act == qaShowTalking) {
		os->osShow = OverlaySettings::Talking;
		updateUsers();
	} else if (act == qaShowHome) {
		os->osShow = OverlaySettings::HomeChannel;
		updateUsers();
	} else if (act == qaShowLinked) {
		os->osShow = OverlaySettings::LinkedChannels;
		updateUsers();
	} else if (act == qaShowSelf) {
		os->bAlwaysSelf = ! os->bAlwaysSelf;
		updateUsers();
	} else {
		for (int i=1;i<=5;++i) {
			if (act == qaColumns[i]) {
				os->uiColumns = i;
				updateLayout();
			}
		}
	}
}

void OverlayUserGroup::wheelEvent(QGraphicsSceneWheelEvent *event) {
	event->accept();

	qreal scale = 0.875f;

	if (event->delta() > 0)
		scale = 1.0f / 0.875f;

	if ((scale < 1.0f) && (os->fZoom <= (1.0f / 4.0f)))
		return;
	else if ((scale > 1.0f) && (os->fZoom >= 4.0f))
		return;

	os->fZoom *= scale;

	updateLayout();
}

bool OverlayUserGroup::sceneEventFilter(QGraphicsItem *watched, QEvent *event) {
	switch (event->type()) {
		case QEvent::GraphicsSceneMouseMove:
		case QEvent::GraphicsSceneMouseRelease:
			QMetaObject::invokeMethod(this, "moveUsers", Qt::QueuedConnection);
			break;
		default:
			break;

	}
	return OverlayGroup::sceneEventFilter(watched, event);
}

void OverlayUserGroup::moveUsers() {
	if (! qgeiHandle)
		return;

	const QRectF &sr = scene()->sceneRect();
	const QPointF &p = qgeiHandle->pos();

	os->fX = qBound<qreal>(0.0f, p.x() / sr.width(), 1.0f);
	os->fY = qBound<qreal>(0.0f, p.y() / sr.height(), 1.0f);

	qgeiHandle->setPos(os->fX * sr.width(), os->fY * sr.height());
	updateUsers();
}

void OverlayUserGroup::updateLayout() {
	prepareGeometryChange();
	reset();
	updateUsers();
}

void OverlayUserGroup::updateUsers() {
	const QRectF &sr = scene()->sceneRect();

	unsigned int uiHeight = iroundf(sr.height());

	QList<QGraphicsItem *> items;
	foreach(QGraphicsItem *qgi, childItems())
		items << qgi;

	QList<OverlayUser *> users;
	if (bShowExamples) {
		if (qlExampleUsers.isEmpty()) {
			qlExampleUsers << new OverlayUser(Settings::Passive, uiHeight, os);
			qlExampleUsers << new OverlayUser(Settings::Talking, uiHeight, os);
			qlExampleUsers << new OverlayUser(Settings::Whispering, uiHeight, os);
			qlExampleUsers << new OverlayUser(Settings::Shouting, uiHeight, os);
		}

		users = qlExampleUsers;
		foreach(OverlayUser *ou, users)
			items.removeAll(ou);

		if (! qgeiHandle) {
			qgeiHandle = new QGraphicsEllipseItem(QRectF(-4.0f, -4.0f, 8.0f, 8.0f));
			qgeiHandle->setPen(QPen(Qt::darkRed, 0.0f));
			qgeiHandle->setBrush(Qt::red);
			qgeiHandle->setZValue(0.5f);
			qgeiHandle->setFlag(QGraphicsItem::ItemIsMovable);
			qgeiHandle->setFlag(QGraphicsItem::ItemIsSelectable);
			qgeiHandle->setPos(sr.width() * os->fX, sr.height() * os->fY);
			scene()->addItem(qgeiHandle);
			qgeiHandle->show();
			qgeiHandle->installSceneEventFilter(this);
		}
	} else {
		delete qgeiHandle;
		qgeiHandle = NULL;
	}

	ClientUser *self = ClientUser::get(g.uiSession);
	if (self) {
		QList<ClientUser *> showusers;
		Channel *home = ClientUser::get(g.uiSession)->cChannel;

		switch (os->osShow) {
			case OverlaySettings::LinkedChannels:
				foreach(Channel *c, home->allLinks())
					foreach(User *p, c->qlUsers)
						showusers << static_cast<ClientUser *>(p);
				foreach(ClientUser *cu, ClientUser::getTalking())
					if (! showusers.contains(cu))
						showusers << cu;
				break;
			case OverlaySettings::HomeChannel:
				foreach(User *p, home->qlUsers)
					showusers << static_cast<ClientUser *>(p);
				foreach(ClientUser *cu, ClientUser::getTalking())
					if (! showusers.contains(cu))
						showusers << cu;
				break;
			default:
				showusers = ClientUser::getTalking();
				if (os->bAlwaysSelf && (self->tsState == Settings::Passive))
					showusers << self;
				break;
		}

		ClientUser::sortUsers(showusers);

		foreach(ClientUser *cu, showusers) {
			OverlayUser *ou = qmUsers.value(cu);
			if (! ou) {
				ou = new OverlayUser(cu, uiHeight, os);
				connect(cu, SIGNAL(destroyed(QObject *)), this, SLOT(userDestroyed(QObject *)));
				qmUsers.insert(cu, ou);
				ou->hide();
			} else {
				items.removeAll(ou);
			}
			users << ou;
		}
	}

	foreach(QGraphicsItem *qgi, items) {
		scene()->removeItem(qgi);
		qgi->hide();
	}

	QRectF children = os->qrfAvatar | os->qrfChannel | os->qrfMutedDeafened | os->qrfUserName;

	int pad = os->bBox ? iroundf(uiHeight * os->fZoom * (os->fBoxPad + os->fBoxPenWidth)) : 0;
	int width = iroundf(children.width() * uiHeight * os->fZoom) + 2 * pad;
	int height = iroundf(children.height() * uiHeight * os->fZoom) + 2 * pad;

	int xofs = - iroundf(children.left() * uiHeight * os->fZoom) + pad;
	int yofs = - iroundf(children.top() * uiHeight * os->fZoom) + pad;

	unsigned int y = 0;
	unsigned int x = 0;

	foreach(OverlayUser *ou, users) {
		if (ou->parentItem() == NULL)
			ou->setParentItem(this);

		ou->setPos(x * (width+4) + xofs, y * (height + 4) + yofs);
		ou->updateUser();
		ou->show();

		if (x >= (os->uiColumns - 1)) {
			x = 0;
			++y;
		} else {
			++x;
		}
	}

	QRectF br = boundingRect<OverlayUser::Type>();

	int basex = qBound<int>(0, iroundf(sr.width() * os->fX), iroundf(sr.width() - br.width()));
	int basey = qBound<int>(0, iroundf(sr.height() * os->fY), iroundf(sr.height() - br.height()));

	setPos(basex, basey);
}

void OverlayUserGroup::userDestroyed(QObject *obj) {
	OverlayUser *ou = qmUsers.take(obj);
	delete ou;
}
