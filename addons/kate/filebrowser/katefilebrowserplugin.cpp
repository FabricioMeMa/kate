/* This file is part of the KDE project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2001 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2001 Anders Lund <anders.lund@lund.tdcadsl.dk>
   Copyright (C) 2007 Mirko Stocker <me@misto.ch>
   Copyright (C) 2009 Dominik Haumann <dhaumann kde org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

//BEGIN Includes
#include "katefilebrowserplugin.h"
#include "katefilebrowserconfig.h"
#include "katefilebrowser.h"

#include <kate/mainwindow.h>
#include <ktexteditor/view.h>

#include <kaboutdata.h>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KIconLoader>

#include <QtGui/QKeyEvent>
//END Includes

K_PLUGIN_FACTORY_WITH_JSON (KateFileBrowserPluginFactory, "katefilebrowserplugin.json", registerPlugin<KateFileBrowserPlugin>();)

//BEGIN KateFileBrowserPlugin
KateFileBrowserPlugin::KateFileBrowserPlugin(QObject* parent, const QList<QVariant>&)
  : Kate::Plugin ((Kate::Application*)parent)
{
}

Kate::PluginView *KateFileBrowserPlugin::createView (Kate::MainWindow *mainWindow)
{
  KateFileBrowserPluginView* view = new KateFileBrowserPluginView (mainWindow);
  connect(view, SIGNAL(destroyed(QObject*)), this, SLOT(viewDestroyed(QObject*)));
  m_views.append(view);

  return view;
}

void KateFileBrowserPlugin::viewDestroyed(QObject* view)
{
  // do not access the view pointer, since it is partially destroyed already
  m_views.removeAll(static_cast<KateFileBrowserPluginView *>(view));
}

int KateFileBrowserPlugin::configPages() const
{
  return 1;
}

KTextEditor::ConfigPage *KateFileBrowserPlugin::configPage (int number, QWidget *parent)
{
  if (number != 0)
    return 0;
  return new KateFileBrowserConfigPage(parent, m_views[0]->m_fileBrowser);
}

QString KateFileBrowserPlugin::configPageName (int number) const
{
  if (number != 0) return QString();
  return i18n("Filesystem Browser");
}

QString KateFileBrowserPlugin::configPageFullName (int number) const
{
  if (number != 0) return QString();
  return i18n("Filesystem Browser Settings");
}

QIcon KateFileBrowserPlugin::configPageIcon (int number) const
{
  if (number != 0) return QIcon();
  return QIcon::fromTheme("document-open");
}
//END KateFileBrowserPlugin



//BEGIN KateFileBrowserPluginView
KateFileBrowserPluginView::KateFileBrowserPluginView (Kate::MainWindow *mainWindow)
  : Kate::PluginView (mainWindow)
  , m_toolView(
        mainWindow->createToolView(
            "kate_private_plugin_katefileselectorplugin"
          , Kate::MainWindow::Left
          , KIconLoader::global()->loadIcon("document-open", KIconLoader::Small)
          , i18n("Filesystem Browser")
          )
      )
  , m_fileBrowser(new KateFileBrowser(mainWindow, m_toolView))
{
  m_toolView->installEventFilter(this);
}

KateFileBrowserPluginView::~KateFileBrowserPluginView ()
{
  // cleanup, kill toolview + console
  delete m_fileBrowser->parentWidget();
}

void KateFileBrowserPluginView::readSessionConfig(KConfigBase* config, const QString& group)
{
  m_fileBrowser->readSessionConfig(config, group);
}

void KateFileBrowserPluginView::writeSessionConfig(KConfigBase* config, const QString& group)
{
  m_fileBrowser->writeSessionConfig(config, group);
}

bool KateFileBrowserPluginView::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress)
  {
    QKeyEvent* ke = static_cast<QKeyEvent*>(event);
    if ((obj == m_toolView) && (ke->key() == Qt::Key_Escape))
    {
      mainWindow()->hideToolView(m_toolView);
      event->accept();
      return true;
    }
  }
  return QObject::eventFilter(obj, event);
}
//ENDKateFileBrowserPluginView

#include "katefilebrowserplugin.moc"

// kate: space-indent on; indent-width 2; replace-tabs on;
