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

#if QT_VERSION < 0x040600
#define toReal toDouble
#endif

OverlayEditorScene::OverlayEditorScene(const OverlaySettings &srcos, QObject *p) : QGraphicsScene(p), os(srcos) {
	tsColor = Settings::Talking;
	uiZoom = 2;

	if (g.ocIntercept)
		uiSize = g.ocIntercept->uiHeight;
	else
		uiSize = 1080.f;

	qgiGroup = new OverlayGroup();
	qgiGroup->setAcceptHoverEvents(true);
	qgiGroup->setPos(0.0f, 0.0f);
	addItem(qgiGroup);

	qgpiMuted = new QGraphicsPixmapItem(qgiGroup);
	qgpiMuted->hide();

	qgpiAvatar = new QGraphicsPixmapItem(qgiGroup);
	qgpiAvatar->hide();

	qgpiName = new QGraphicsPixmapItem(qgiGroup);
	qgpiName->hide();

	qgpiChannel = new QGraphicsPixmapItem(qgiGroup);
	qgpiChannel->hide();

	qgpiBox = new QGraphicsPathItem(qgiGroup);
	qgpiBox->hide();

	qgpiSelected = NULL;

	qgriSelected = new QGraphicsRectItem;
	qgriSelected->hide();

	qgriSelected->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
	qgriSelected->setOpacity(1.0f);
	qgriSelected->setBrush(Qt::NoBrush);
	qgriSelected->setPen(QPen(Qt::black, 4.0f));
	qgriSelected->setZValue(5.0f);

	addItem(qgriSelected);

	qgpiChannel->setZValue(2.0f);
	qgpiName->setZValue(1.0f);
	qgpiMuted->setZValue(3.0f);

	qgpiBox->setZValue(-1.0f);

	resync();
}

#define SCALESIZE(var) iroundf(uiSize * uiZoom * os.qrf##var .width()), iroundf(uiSize * uiZoom * os.qrf##var .height())

void OverlayEditorScene::updateMuted() {
	QImageReader qir(QLatin1String("skin:muted_self.svg"));
	QSize sz = qir.size();
	sz.scale(SCALESIZE(MutedDeafened), Qt::KeepAspectRatio);
	qir.setScaledSize(sz);
	qgpiMuted->setPixmap(QPixmap::fromImage(qir.read()));

	moveMuted();
}

void OverlayEditorScene::moveMuted() {
	qgpiMuted->setVisible(os.bMutedDeafened);
	qgpiMuted->setPos(OverlayUser::alignedPosition(OverlayUser::scaledRect(os.qrfMutedDeafened, uiSize * uiZoom), qgpiMuted->boundingRect(), os.qaMutedDeafened));
	qgpiMuted->setOpacity(os.fMutedDeafened);
}

void OverlayEditorScene::updateUserName() {
	QString qsName;

	switch (tsColor) {
		case Settings::Passive:
			qsName = Overlay::tr("Silent");
			break;
		case Settings::Talking:
			qsName = Overlay::tr("Talking");
			break;
		case Settings::Whispering:
			qsName = Overlay::tr("Whisper");
			break;
		case Settings::Shouting:
			qsName = Overlay::tr("Shout");
			break;
	}

	const QPixmap &pm = OverlayTextLine(qsName, os.qfUserName).createPixmap(SCALESIZE(UserName), os.qcUserName[tsColor]);
	qgpiName->setPixmap(pm);

	moveUserName();
}

void OverlayEditorScene::moveUserName() {
	qgpiName->setVisible(os.bUserName);
	qgpiName->setPos(OverlayUser::alignedPosition(OverlayUser::scaledRect(os.qrfUserName, uiSize * uiZoom), qgpiName->boundingRect(), os.qaUserName));
	qgpiName->setOpacity(os.fUserName);
}

void OverlayEditorScene::updateChannel() {
	const QPixmap &pm = OverlayTextLine(Overlay::tr("Channel"), os.qfChannel).createPixmap(SCALESIZE(Channel), os.qcChannel);
	qgpiChannel->setPixmap(pm);

	moveChannel();
}

void OverlayEditorScene::moveChannel() {
	qgpiChannel->setVisible(os.bChannel);
	qgpiChannel->setPos(OverlayUser::alignedPosition(OverlayUser::scaledRect(os.qrfChannel, uiSize * uiZoom), qgpiChannel->boundingRect(), os.qaChannel));
	qgpiChannel->setOpacity(os.fChannel);
}

void OverlayEditorScene::updateAvatar() {
	QImage img;
	QImageReader qir(QLatin1String("skin:default_avatar.svg"));
	QSize sz = qir.size();
	sz.scale(SCALESIZE(Avatar), Qt::KeepAspectRatio);
	qir.setScaledSize(sz);
	img = qir.read();
	qgpiAvatar->setPixmap(QPixmap::fromImage(img));

	moveAvatar();
}

void OverlayEditorScene::moveAvatar() {
	qgpiAvatar->setVisible(os.bAvatar);
	qgpiAvatar->setPos(OverlayUser::alignedPosition(OverlayUser::scaledRect(os.qrfAvatar, uiSize * uiZoom), qgpiAvatar->boundingRect(), os.qaAvatar));
	qgpiAvatar->setOpacity(os.fAvatar);
}

void OverlayEditorScene::moveBox() {
	QRectF children = os.qrfAvatar | os.qrfChannel | os.qrfMutedDeafened | os.qrfUserName;

	bool haspen = (os.qcBoxPen != os.qcBoxFill) && (! qFuzzyCompare(os.qcBoxPen.alphaF(), static_cast<qreal>(0.0f)));
	qreal pw = haspen ? qMax<qreal>(1.0f, os.fBoxPenWidth * uiSize * uiZoom) : 0.0f;
	qreal pad = os.fBoxPad * uiSize * uiZoom;

	QPainterPath pp;
	pp.addRoundedRect(children.x() * uiSize * uiZoom + -pw / 2.0f - pad, children.y() * uiSize * uiZoom + -pw / 2.0f - pad, children.width() * uiSize * uiZoom + pw + 2.0f * pad, children.height() * uiSize * uiZoom + pw + 2.0f * pad, 2.0f * pw, 2.0f * pw);
	qgpiBox->setPath(pp);
	qgpiBox->setPos(0.0f, 0.0f);
	qgpiBox->setPen(haspen ? QPen(os.qcBoxPen, pw) : Qt::NoPen);
	qgpiBox->setBrush(qFuzzyCompare(os.qcBoxFill.alphaF(), static_cast<qreal>(0.0f)) ? Qt::NoBrush : os.qcBoxFill);
	qgpiBox->setOpacity(1.0f);

	qgpiBox->setVisible(os.bBox);
}

void OverlayEditorScene::updateSelected() {
	if (qgpiSelected == qgpiAvatar)
		updateAvatar();
	else if (qgpiSelected == qgpiName)
		updateUserName();
	else if (qgpiSelected == qgpiAvatar)
		updateAvatar();
	else if (qgpiSelected == qgpiMuted)
		updateMuted();
}

void OverlayEditorScene::resync() {
	QRadialGradient gradient(0, 0, 10 * uiZoom);
	gradient.setSpread(QGradient::ReflectSpread);
	gradient.setColorAt(0.0f, QColor(255, 255, 255, 64));
	gradient.setColorAt(0.2f, QColor(0, 0, 0, 64));
	gradient.setColorAt(0.4f, QColor(255, 128, 0, 64));
	gradient.setColorAt(0.6f, QColor(0, 0, 0, 64));
	gradient.setColorAt(0.8f, QColor(0, 128, 255, 64));
	gradient.setColorAt(1.0f, QColor(0, 0, 0, 64));
	setBackgroundBrush(gradient);

	updateMuted();
	updateUserName();
	updateChannel();
	updateAvatar();

	moveMuted();
	moveUserName();
	moveChannel();
	moveAvatar();

	moveBox();

	qgiGroup->setOpacity(os.fUser[tsColor]);

	qgpiSelected = NULL;
	qgriSelected->setVisible(false);
}

void OverlayEditorScene::drawBackground(QPainter *p, const QRectF &rect) {
	p->setBrushOrigin(0, 0);
	p->fillRect(rect, backgroundBrush());

	QRectF upscaled = OverlayUser::scaledRect(rect, 128.f / (uiSize * uiZoom));

	{
		int min = iroundf(upscaled.left() - 0.5f);
		int max = iroundf(upscaled.right() + 0.5f);

		for (int i=min;i<=max;++i) {
			qreal v = (i / 128.f) * uiSize * uiZoom;

			if (i != 0)
				p->setPen(QPen(QColor(128, 128, 128, 255), 0.0f));
			else
				p->setPen(QPen(QColor(0, 0, 0, 255), 2.0f));

			p->drawLine(QPointF(v, rect.top()), QPointF(v, rect.bottom()));
		}
	}

	{
		int min = iroundf(upscaled.top() - 0.5f);
		int max = iroundf(upscaled.bottom() + 0.5f);

		for (int i=min;i<=max;++i) {
			qreal v = (i / 128.f) * uiSize * uiZoom;

			if (i != 0)
				p->setPen(QPen(QColor(128, 128, 128, 255), 0.0f));
			else
				p->setPen(QPen(QColor(0, 0, 0, 255), 2.0f));

			p->drawLine(QPointF(rect.left(), v), QPointF(rect.right(), v));
		}
	}
}

QGraphicsPixmapItem *OverlayEditorScene::childAt(const QPointF &pos) {
	QGraphicsItem *item = NULL;

	if (qgriSelected->isVisible()) {
		if (qgriSelected->rect().contains(pos)) {
			return qgpiSelected;
		}
	}

	foreach(QGraphicsItem *qgi, items(Qt::AscendingOrder)) {
		if (!qgi->isVisible() || ! qgraphicsitem_cast<QGraphicsPixmapItem *>(qgi))
			continue;

		QPointF qp = pos - qgi->pos();
		if (qgi->contains(qp)) {
			item = qgi;
		}
	}
	return static_cast<QGraphicsPixmapItem *>(item);
}

QRectF OverlayEditorScene::selectedRect() const {
	const QRectF *qrf = NULL;

	if (qgpiSelected == qgpiMuted)
		qrf = & os.qrfMutedDeafened;
	else if (qgpiSelected == qgpiAvatar)
		qrf = & os.qrfAvatar;
	else if (qgpiSelected == qgpiChannel)
		qrf = & os.qrfChannel;
	else if (qgpiSelected == qgpiName)
		qrf = & os.qrfUserName;

	if (! qrf)
		return QRectF();

	return OverlayUser::scaledRect(*qrf, uiSize * uiZoom).toAlignedRect();
}


void OverlayEditorScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	QGraphicsScene::mousePressEvent(event);

	if (event->isAccepted())
		return;

	if (event->button() == Qt::LeftButton) {
		event->accept();

		if (wfsHover == Qt::NoSection) {
			qgpiSelected = childAt(event->scenePos());
			if (qgpiSelected) {
				qgriSelected->setRect(selectedRect());
				qgriSelected->show();
			} else {
				qgriSelected->hide();
			}
		}

		updateCursorShape(event->scenePos());
	}
}

void OverlayEditorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
	QGraphicsScene::mouseReleaseEvent(event);

	if (event->isAccepted())
		return;

	if (event->button() == Qt::LeftButton) {
		event->accept();

		QRectF rect = qgriSelected->rect();

		if (! qgpiSelected || (rect == selectedRect())) {
			return;
		}

		QRectF scaled(rect.x() / (uiSize * uiZoom), rect.y() / (uiSize * uiZoom), rect.width() / (uiSize * uiZoom), rect.height() / (uiSize * uiZoom));

		if (qgpiSelected == qgpiMuted) {
			os.qrfMutedDeafened = scaled;
			updateMuted();
		} else if (qgpiSelected == qgpiAvatar) {
			os.qrfAvatar = scaled;
			updateAvatar();
		} else if (qgpiSelected == qgpiChannel) {
			os.qrfChannel = scaled;
			updateChannel();
		} else if (qgpiSelected == qgpiName) {
			os.qrfUserName = scaled;
			updateUserName();
		}

		moveBox();
	}
}

void OverlayEditorScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	QGraphicsScene::mouseMoveEvent(event);

	if (event->isAccepted())
		return;

	if (qgpiSelected && (event->buttons() & Qt::LeftButton)) {
		event->accept();

		if (wfsHover == Qt::NoSection)
			return;

		QPointF delta = event->scenePos() - event->buttonDownScenePos(Qt::LeftButton);

		bool square = event->modifiers() & Qt::ShiftModifier;

		QRectF orig = selectedRect();
		switch (wfsHover) {
			case Qt::TitleBarArea:
				orig.translate(delta);
				break;
			case Qt::TopSection:
				orig.setTop(orig.top() + delta.y());
				if (orig.height() < 8.0f)
					orig.setTop(orig.bottom() - 8.0f);
				if (square)
					orig.setRight(orig.left() + orig.height());
				break;
			case Qt::BottomSection:
				orig.setBottom(orig.bottom() + delta.y());
				if (orig.height() < 8.0f)
					orig.setBottom(orig.top() + 8.0f);
				if (square)
					orig.setRight(orig.left() + orig.height());
				break;
			case Qt::LeftSection:
				orig.setLeft(orig.left() + delta.x());
				if (orig.width() < 8.0f)
					orig.setLeft(orig.right() - 8.0f);
				if (square)
					orig.setBottom(orig.top() + orig.width());
				break;
			case Qt::RightSection:
				orig.setRight(orig.right() + delta.x());
				if (orig.width() < 8.0f)
					orig.setRight(orig.left() + 8.0f);
				if (square)
					orig.setBottom(orig.top() + orig.width());
				break;
			case Qt::TopLeftSection:
				orig.setTopLeft(orig.topLeft() + delta);
				if (orig.height() < 8.0f)
					orig.setTop(orig.bottom() - 8.0f);
				if (orig.width() < 8.0f)
					orig.setLeft(orig.right() - 8.0f);
				if (square) {
					qreal size = qMin(orig.width(), orig.height());
					QPointF sz(-size, -size);
					orig.setTopLeft(orig.bottomRight() + sz);
				}
				break;
			case Qt::TopRightSection:
				orig.setTopRight(orig.topRight() + delta);
				if (orig.height() < 8.0f)
					orig.setTop(orig.bottom() - 8.0f);
				if (orig.width() < 8.0f)
					orig.setRight(orig.left() + 8.0f);
				if (square) {
					qreal size = qMin(orig.width(), orig.height());
					QPointF sz(size, -size);
					orig.setTopRight(orig.bottomLeft() + sz);
				}
				break;
			case Qt::BottomLeftSection:
				orig.setBottomLeft(orig.bottomLeft() + delta);
				if (orig.height() < 8.0f)
					orig.setBottom(orig.top() + 8.0f);
				if (orig.width() < 8.0f)
					orig.setLeft(orig.right() - 8.0f);
				if (square) {
					qreal size = qMin(orig.width(), orig.height());
					QPointF sz(-size, size);
					orig.setBottomLeft(orig.topRight() + sz);
				}
				break;
			case Qt::BottomRightSection:
				orig.setBottomRight(orig.bottomRight() + delta);
				if (orig.height() < 8.0f)
					orig.setBottom(orig.top() + 8.0f);
				if (orig.width() < 8.0f)
					orig.setRight(orig.left() + 8.0f);
				if (square) {
					qreal size = qMin(orig.width(), orig.height());
					QPointF sz(size, size);
					orig.setBottomRight(orig.topLeft() + sz);
				}
				break;
		}

		qgriSelected->setRect(orig);
	} else {
		updateCursorShape(event->scenePos());
	}
}

void OverlayEditorScene::updateCursorShape(const QPointF &point) {
	Qt::CursorShape	cs;

	if (qgriSelected->isVisible()) {
		wfsHover = rectSection(qgriSelected->rect(), point);
	} else {
		wfsHover = Qt::NoSection;
	}

	switch (wfsHover) {
		case Qt::TopLeftSection:
		case Qt::BottomRightSection:
			cs = Qt::SizeFDiagCursor;
			break;
		case Qt::TopRightSection:
		case Qt::BottomLeftSection:
			cs = Qt::SizeBDiagCursor;
			break;
		case Qt::TopSection:
		case Qt::BottomSection:
			cs = Qt::SizeVerCursor;
			break;
		case Qt::LeftSection:
		case Qt::RightSection:
			cs = Qt::SizeHorCursor;
			break;
		case Qt::TitleBarArea:
			cs = Qt::OpenHandCursor;
			break;
		default:
			cs = Qt::ArrowCursor;
			break;
	}


	foreach(QGraphicsView *v, views()) {
		if (v->viewport()->cursor().shape() != cs) {
			v->viewport()->setCursor(cs);

			// But an embedded, injected GraphicsView doesn't propagage mouse cursors...
			QWidget *p = v->parentWidget();
			if (p) {
				QGraphicsProxyWidget *qgpw = p->graphicsProxyWidget();
				if (qgpw) {
					qgpw->setCursor(cs);
					if (g.ocIntercept)
						g.ocIntercept->updateMouse();
				}
			}
		}
	}
}

void OverlayEditorScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
	QGraphicsScene::contextMenuEvent(event);

	if (event->isAccepted())
		return;

	if (! event->widget())
		return;

	QGraphicsPixmapItem *item = childAt(event->scenePos());

	QMenu qm(event->widget());

	QMenu *qmLayout = qm.addMenu(tr("Layout preset"));
	QAction *qaLayoutLargeAvatar = qmLayout->addAction(tr("Large square avatar"));
	QAction *qaLayoutText = qmLayout->addAction(tr("Avatar and Name"));

	QMenu *qmTrans = qm.addMenu(tr("User Opacity"));
	QActionGroup *qagUser = new QActionGroup(&qm);
	QAction *userOpacity[8];
	for (int i=0;i<8;++i) {
		qreal o = (i + 1) / 8.0f;

		userOpacity[i] = new QAction(tr("%1%").arg(o * 100.0f, 0, 'f', 1), qagUser);
		userOpacity[i]->setCheckable(true);
		userOpacity[i]->setData(o);

		if (qFuzzyCompare(qgiGroup->opacity(), o))
			userOpacity[i]->setChecked(true);

		qmTrans->addAction(userOpacity[i]);
	}

	QAction *color = NULL;
	QAction *font = NULL;
	QAction *objectOpacity[8];
	for (int i=0;i<8;++i)
		objectOpacity[i] = NULL;
	QAction *boxpen[4] = { NULL, NULL, NULL, NULL};
	QAction *boxpad[4] = { NULL, NULL, NULL, NULL};
	QAction *boxpencolor = NULL;
	QAction *boxfillcolor = NULL;

	QAction *align[6];
	for (int i=0;i<6;++i)
		align[i] = NULL;

	if (item) {
		qm.addSeparator();
		QMenu *qmObjTrans = qm.addMenu(tr("Object Opacity"));
		QActionGroup *qagObject = new QActionGroup(&qm);
		for (int i=0;i<8;++i) {
			qreal o = (i + 1) / 8.0f;

			objectOpacity[i] = new QAction(tr("%1%").arg(o * 100.0f, 0, 'f', 1), qagObject);
			objectOpacity[i]->setCheckable(true);
			objectOpacity[i]->setData(o);
			if (qFuzzyCompare(item->opacity(), o))
				objectOpacity[i]->setChecked(true);
			qmObjTrans->addAction(objectOpacity[i]);
		}

		QMenu *qmObjAlign = qm.addMenu(tr("Alignment"));
		Qt::Alignment a;
		if (item == qgpiAvatar)
			a = os.qaAvatar;
		else if (item == qgpiChannel)
			a = os.qaChannel;
		else if (item == qgpiMuted)
			a = os.qaMutedDeafened;
		else
			a = os.qaUserName;

		align[0] = qmObjAlign->addAction(tr("Left"));
		align[0]->setCheckable(true);
		align[0]->setData(Qt::AlignLeft);
		if (a & Qt::AlignLeft)
			align[0]->setChecked(true);
		align[1] = qmObjAlign->addAction(tr("Center"));
		align[1]->setCheckable(true);
		align[1]->setData(Qt::AlignHCenter);
		if (a & Qt::AlignHCenter)
			align[1]->setChecked(true);
		align[2] = qmObjAlign->addAction(tr("Right"));
		align[2]->setCheckable(true);
		align[2]->setData(Qt::AlignRight);
		if (a & Qt::AlignRight)
			align[2]->setChecked(true);

		qmObjAlign->addSeparator();

		align[3] = qmObjAlign->addAction(tr("Top"));
		align[3]->setCheckable(true);
		align[3]->setData(Qt::AlignTop);
		if (a & Qt::AlignTop)
			align[3]->setChecked(true);
		align[4] = qmObjAlign->addAction(tr("Center"));
		align[4]->setCheckable(true);
		align[4]->setData(Qt::AlignVCenter);
		if (a & Qt::AlignVCenter)
			align[4]->setChecked(true);
		align[5] = qmObjAlign->addAction(tr("Bottom"));
		align[5]->setCheckable(true);
		align[5]->setData(Qt::AlignBottom);
		if (a & Qt::AlignBottom)
			align[5]->setChecked(true);

		if ((item != qgpiAvatar) && (item != qgpiMuted)) {
			color = qm.addAction(tr("Color..."));
			font = qm.addAction(tr("Font..."));
		}
	}

	if (qgpiBox->isVisible()) {
		qm.addSeparator();
		QMenu *qmBox = qm.addMenu(tr("Bounding box"));
		QMenu *qmPen = qmBox->addMenu(tr("Pen width"));
		QMenu *qmPad = qmBox->addMenu(tr("Padding"));
		boxpencolor = qmBox->addAction(tr("Pen color"));
		boxfillcolor = qmBox->addAction(tr("Fill color"));

		QActionGroup *qagPen = new QActionGroup(qmPen);
		QActionGroup *qagPad = new QActionGroup(qmPad);
		for (int i=0;i<4;++i) {
			qreal v = (i) ? powf(2.0f, -10 + i) : 0.0f;
			boxpen[i] = new QAction(QString::number(i), qagPen);
			boxpen[i]->setData(v);
			boxpen[i]->setCheckable(true);
			if (qFuzzyCompare(os.fBoxPenWidth, v))
				boxpen[i]->setChecked(true);
			qmPen->addAction(boxpen[i]);

			boxpad[i] = new QAction(QString::number(i), qagPad);
			boxpad[i]->setData(v);
			boxpad[i]->setCheckable(true);
			if (qFuzzyCompare(os.fBoxPad, v))
				boxpad[i]->setChecked(true);
			qmPad->addAction(boxpad[i]);
		}
	}

	QAction *act = qm.exec(event->screenPos());

	if (! act)
		return;

	for (int i=0;i<8;++i) {
		if (userOpacity[i] == act) {
			float o = act->data().toReal();
			os.fUser[tsColor] = o;

			qgiGroup->setOpacity(o);
		}
	}

	for (int i=0;i<8;++i) {
		if (objectOpacity[i] == act) {
			qreal o = act->data().toReal();

			if (item == qgpiMuted)
				os.fMutedDeafened = o;
			else if (item == qgpiAvatar)
				os.fAvatar = o;
			else if (item == qgpiChannel)
				os.fChannel = o;
			else if (item == qgpiName)
				os.fUserName = o;

			item->setOpacity(o);
		}
	}

	for (int i=0;i<4;++i) {
		if (boxpen[i] == act) {
			os.fBoxPenWidth = act->data().toReal();
			moveBox();
		} else if (boxpad[i] == act) {
			os.fBoxPad = act->data().toReal();
			moveBox();
		}
	}

	for (int i=0;i<6;++i) {
		if (align[i] == act) {
			Qt::Alignment *aptr;
			if (item == qgpiAvatar)
				aptr = & os.qaAvatar;
			else if (item == qgpiChannel)
				aptr = & os.qaChannel;
			else if (item == qgpiMuted)
				aptr = & os.qaMutedDeafened;
			else
				aptr = & os.qaUserName;

			Qt::Alignment a = static_cast<Qt::Alignment>(act->data().toInt());
			if (a & Qt::AlignHorizontal_Mask) {
				*aptr = (*aptr & ~Qt::AlignHorizontal_Mask) | a;
			} else {
				*aptr = (*aptr & ~Qt::AlignVertical_Mask) | a;
			}

			updateSelected();
		}
	}

	if (act == boxpencolor) {
		QColor qc = QColorDialog::getColor(os.qcBoxPen, event->widget(), tr("Pick pen color"), QColorDialog::DontUseNativeDialog | QColorDialog::ShowAlphaChannel);
		if (! qc.isValid())
			return;
		os.qcBoxPen = qc;
		moveBox();
	} else if (act == boxfillcolor) {
		QColor qc = QColorDialog::getColor(os.qcBoxFill, event->widget(), tr("Pick fill color"), QColorDialog::DontUseNativeDialog | QColorDialog::ShowAlphaChannel);
		if (! qc.isValid())
			return;
		os.qcBoxFill = qc;
		moveBox();
	} else if (act == color) {
		QColor *col = NULL;
		if (item == qgpiChannel)
			col = & os.qcChannel;
		else if (item == qgpiName)
			col = & os.qcUserName[tsColor];
		if (! col)
			return;

		QColor qc = QColorDialog::getColor(*col, event->widget(), tr("Pick color"), QColorDialog::DontUseNativeDialog);
		if (! qc.isValid())
			return;
		qc.setAlpha(255);

		if (qc == *col)
			return;

		*col = qc;
		updateSelected();
	} else if (act == font) {
		QFont *fontptr = (item == qgpiChannel) ? &os.qfChannel : &os.qfUserName;

		qgpiSelected = NULL;
		qgriSelected->hide();

		// QFontDialog doesn't really like graphics view. At all.

		QFontDialog qfd;
		qfd.setOptions(QFontDialog::DontUseNativeDialog);
		qfd.setCurrentFont(*fontptr);
		qfd.setWindowTitle(tr("Pick font"));

		int ret;
		if (g.ocIntercept) {
			QGraphicsProxyWidget *qgpw = new QGraphicsProxyWidget(NULL, Qt::Window);
			qgpw->setWidget(&qfd);

			addItem(qgpw);

			qgpw->setZValue(3.0f);
#if QT_VERSION >= 0x040600
			qgpw->setPanelModality(QGraphicsItem::PanelModal);
#endif
			qgpw->setPos(- qgpw->boundingRect().width() / 2.0f, - qgpw->boundingRect().height() / 2.0f);
			qgpw->show();

			ret = qfd.exec();

			qgpw->hide();
			qgpw->setWidget(NULL);
			delete qgpw;
		} else {
			ret = qfd.exec();
		}

		if (! ret)
			return;
		*fontptr = qfd.selectedFont();

		resync();
	} else if (act == qaLayoutLargeAvatar) {
		os.fUserName = 0.75f;
		os.fChannel = 0.75f;
		os.fMutedDeafened = 0.5f;
		os.fAvatar = 1.0f;

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
		os.qfUserName = QFont(QLatin1String("Verdana"), 20);
#else
		os.qfUserName = QFont(QLatin1String("Arial"), 20);
#endif
		os.qfChannel = os.qfUserName;

		os.fUser[Settings::Passive] = 0.5f;
		os.fUser[Settings::Talking] = (7.0f / 8.0f);
		os.fUser[Settings::Whispering] = (7.0f / 8.0f);
		os.fUser[Settings::Shouting] = (7.0f / 8.0f);

		os.qrfUserName = QRectF(-0.0625f, 0.101563f - 0.0625f, 0.125f, 0.023438f);
		os.qrfChannel = QRectF(-0.03125f, -0.0625f, 0.09375f, 0.015625f);
		os.qrfMutedDeafened = QRectF(-0.0625f, -0.0625f, 0.0625f, 0.0625f);
		os.qrfAvatar = QRectF(-0.0625f, -0.0625f, 0.125f, 0.125f);

		os.qcBoxPen = QColor(0, 0, 0, 224);
		os.qcBoxFill = QColor(128, 128, 128, 128);
		os.fBoxPenWidth = (1.f / 256.0f);
		os.fBoxPad = (1.f / 256.0f);

		os.bUserName = true;
		os.bChannel = true;
		os.bMutedDeafened = true;
		os.bAvatar = true;
		os.bBox = false;

		os.qaUserName = Qt::AlignCenter;
		os.qaMutedDeafened = Qt::AlignLeft | Qt::AlignTop;
		os.qaAvatar = Qt::AlignCenter;
		os.qaChannel = Qt::AlignCenter;

		resync();
	} else if (act == qaLayoutText) {
		os.fUserName = 1.0f;
		os.fChannel = (7.0f / 8.0f);
		os.fMutedDeafened = (7.0f / 8.0f);
		os.fAvatar = 1.0f;

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
		os.qfUserName = QFont(QLatin1String("Verdana"), 20);
#else
		os.qfUserName = QFont(QLatin1String("Arial"), 20);
#endif
		os.qfChannel = os.qfUserName;

		os.fUser[Settings::Passive] = 0.5f;
		os.fUser[Settings::Talking] = (7.0f / 8.0f);
		os.fUser[Settings::Whispering] = (7.0f / 8.0f);
		os.fUser[Settings::Shouting] = (7.0f / 8.0f);

		os.qrfUserName = QRectF(0.0f, -0.015625f, 0.250f, 0.03125f);
		os.qrfChannel = QRectF(0.03125f, -0.015625f, 0.1875f, 0.015625f);
		os.qrfMutedDeafened = QRectF(-0.03125f, -0.015625f, 0.03125f, 0.03125f);
		os.qrfAvatar = QRectF(-0.03125f, -0.015625f, 0.03125f, 0.03125f);

		os.qcBoxPen = QColor(0, 0, 0, 224);
		os.qcBoxFill = QColor(128, 128, 128, 128);
		os.fBoxPenWidth = 0.0f;
		os.fBoxPad = (1.f / 512.0f);

		os.bUserName = true;
		os.bChannel = false;
		os.bMutedDeafened = true;
		os.bAvatar = true;
		os.bBox = true;

		os.qaUserName = Qt::AlignLeft | Qt::AlignVCenter;
		os.qaMutedDeafened = Qt::AlignRight | Qt::AlignVCenter;
		os.qaAvatar = Qt::AlignRight | Qt::AlignVCenter;
		os.qaChannel = Qt::AlignLeft | Qt::AlignTop;

		resync();
	}
}

static qreal distancePointLine(const QPointF &a, const QPointF &b, const QPointF &p) {
	qreal xda = a.x() - p.x();
	qreal xdb = p.x() - b.x();

	qreal xd = 0;

	if (xda > 0)
		xd = xda;
	if (xdb > 0)
		xd = qMax(xd, xdb);

	qreal yda = a.y() - p.y();
	qreal ydb = p.y() - b.y();

	qreal yd = 0;

	if (yda > 0)
		yd = yda;
	if (ydb > 0)
		yd = qMax(yd, ydb);

	return qMax(xd, yd);
}

Qt::WindowFrameSection OverlayEditorScene::rectSection(const QRectF &qrf, const QPointF &qp, qreal dist) {
	qreal left, right, top, bottom;

	top = distancePointLine(qrf.topLeft(), qrf.topRight(), qp);
	bottom = distancePointLine(qrf.bottomLeft(), qrf.bottomRight(), qp);
	left = distancePointLine(qrf.topLeft(), qrf.bottomLeft(), qp);
	right = distancePointLine(qrf.topRight(), qrf.bottomRight(), qp);

	if ((top < dist) && (top < bottom)) {
		if ((left < dist) && (left < right))
			return Qt::TopLeftSection;
		else if (right < dist)
			return Qt::TopRightSection;
		return Qt::TopSection;
	} else if (bottom < dist) {
		if ((left < dist) && (left < right))
			return Qt::BottomLeftSection;
		else if (right < dist)
			return Qt::BottomRightSection;
		return Qt::BottomSection;
	} else if (left < dist) {
		return Qt::LeftSection;
	} else if (right < dist) {
		return Qt::RightSection;
	}
	if (qrf.contains(qp))
		return Qt::TitleBarArea;

	return Qt::NoSection;
}
