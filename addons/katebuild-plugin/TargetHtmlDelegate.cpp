/***************************************************************************
 *   This file is part of Kate search plugin                               *
 *   Copyright 2014 Kåre Särs <kare.sars@iki.fi>                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "TargetHtmlDelegate.h"
#include "TargetModel.h"

#include <QPainter>
#include <QModelIndex>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextCharFormat>
#include <QLineEdit>
#include <KLineEdit>
#include <kurlrequester.h>
#include <klocalizedstring.h>

#include "UrlInserter.h"

#include <QDebug>

TargetHtmlDelegate::TargetHtmlDelegate( QObject* parent )
: QStyledItemDelegate(parent), m_isEditing(false)
{
    connect(this, SIGNAL(sendEditStart()),
            this, SLOT(editStarted()));
}

TargetHtmlDelegate::~TargetHtmlDelegate() {}

void TargetHtmlDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    QTextDocument doc;

    QString str;
    if (!index.parent().isValid()) {
        if (index.column() == 0) {
            str = i18nc("T as in Target set", "<B>T:</B> %1", index.data().toString());
        }
        else if (index.column() == 1) {
            str = i18nc("D as in working Directory", "<B>Dir:</B> %1", index.data().toString());
        }
    }
    else {
        str = index.data().toString();
    }
    doc.setHtml(str);
    doc.setDocumentMargin(2);

    painter->save();

    // paint background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    }
    else {
        painter->fillRect(option.rect, option.palette.base());
    }

    options.text = QString();  // clear old text
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter, options.widget);

    // draw text
    painter->translate(option.rect.x(), option.rect.y());
    if (index.column() == 0 && index.internalId() != TargetModel::InvalidIndex) {
        painter->translate(25, 0);
    }
    QAbstractTextDocumentLayout::PaintContext pcontext;
    doc.documentLayout()->draw(painter, pcontext);

    painter->restore();
}


QSize TargetHtmlDelegate::sizeHint(const QStyleOptionViewItem& /* option */, const QModelIndex& index) const
{
    QTextDocument doc;
    doc.setHtml(index.data().toString());
    doc.setDocumentMargin(2);
    if (index.column() == 0 && index.internalId() != TargetModel::InvalidIndex) {
        return doc.size().toSize() + QSize(30, 0); // add margin for the check-box;
    }
    return doc.size().toSize();
}



QWidget *TargetHtmlDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex &index) const
{
    QWidget *editor;
    if (index.internalId() == TargetModel::InvalidIndex && index.column() == 1) {
        KUrlRequester *requester = new KUrlRequester(parent);
        requester->setMode(KFile::Directory | KFile::ExistingOnly |KFile::LocalOnly);
        editor = requester;
        editor->setToolTip(i18n("Leave empty to use the directory of the current document."));
    }
    else if (index.column() == 1) {
        UrlInserter *urlEditor = new UrlInserter(parent);
        editor = urlEditor;
        editor->setToolTip(i18n("Use:\n\"%f\" for current file\n\"%d\" for directory of current file\n\"%n\" for current file name without suffix"));
    }
    else {
        editor =  new QLineEdit(parent);
    }
    editor->setAutoFillBackground(true);
    emit sendEditStart();
    connect(editor, SIGNAL(destroyed(QObject*)), this, SLOT(editEnded()));
    return editor;
}

void TargetHtmlDelegate::setEditorData(QWidget *editor,  const QModelIndex &index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();

    if (index.internalId() == TargetModel::InvalidIndex && index.column() == 1) {
        KUrlRequester *ledit = static_cast<KUrlRequester*>(editor);
        if (ledit) ledit->lineEdit()->setText(value);
    }
    else if (index.column() == 1) {
        UrlInserter *ledit = static_cast<UrlInserter*>(editor);
        if (ledit) ledit->lineEdit()->setText(value);
    }
    else {
        QLineEdit *ledit = static_cast<QLineEdit*>(editor);
        if (ledit) ledit->setText(value);
    }
}

void TargetHtmlDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QString value;
    if (index.internalId() == TargetModel::InvalidIndex && index.column() == 1) {
        KUrlRequester *ledit = static_cast<KUrlRequester*>(editor);
        value = ledit->lineEdit()->text();
    }
    else if (index.column() == 1) {
        UrlInserter *ledit = static_cast<UrlInserter*>(editor);
        value = ledit->lineEdit()->text();
    }
    else {
        QLineEdit *ledit = static_cast<QLineEdit*>(editor);
        value = ledit->text();
    }
    model->setData(index, value, Qt::EditRole);
}

void TargetHtmlDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QRect rect = option.rect;
    int heightDiff = KUrlRequester().sizeHint().height() - rect.height();
    int half = heightDiff/2;
    rect.adjust(0, -half, 0, heightDiff-half);
    if (index.column() == 0 && index.internalId() != TargetModel::InvalidIndex) {
        rect.adjust(25, 0, 0, 0);
    }
    editor->setGeometry(rect);
}

void TargetHtmlDelegate::editStarted() { m_isEditing = true; }
void TargetHtmlDelegate::editEnded() { m_isEditing = false; }
bool TargetHtmlDelegate::isEditing() const { return m_isEditing; }
