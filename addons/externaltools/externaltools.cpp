/*
   This file is part of the Kate text editor of the KDE project.

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

   ---
   Copyright (C) 2004, Anders Lund <anders@alweb.dk>
*/
// TODO
// Icons
// Direct shortcut setting
#include "externaltools.h"
#include "externaltoolsplugin.h"
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <KActionCollection>
#include <KConfig>
#include <KConfigGroup>
#include <KIconButton>
#include <KIconLoader>
#include <KMessageBox>
#include <KMimeTypeChooser>
#include <KRun>
#include <KSharedConfig>
#include <KXMLGUIFactory>
#include <KXmlGuiWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QStandardPaths>

#include <QBitmap>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QRegExp>
#include <QTextEdit>
#include <QToolButton>

#include <unistd.h>

// BEGIN KateExternalTool
KateExternalTool::KateExternalTool(const QString& name, const QString& command, const QString& icon,
                                   const QString& tryexec, const QStringList& mimetypes, const QString& acname,
                                   const QString& cmdname, int save)
    : name(name)
    , command(command)
    , icon(icon)
    , tryexec(tryexec)
    , mimetypes(mimetypes)
    , acname(acname)
    , cmdname(cmdname)
    , save(save)
{
    // if ( ! tryexec.isEmpty() )
    hasexec = checkExec();
}

bool KateExternalTool::checkExec()
{
    // if tryexec is empty, it is the first word of command
    if (tryexec.isEmpty())
        tryexec = command.section(QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty);

    // NOTE this code is modified taken from kdesktopfile.cpp, from KDesktopFile::tryExec()
    if (!tryexec.isEmpty()) {
        m_exec = QStandardPaths::findExecutable(tryexec);
        return !m_exec.isEmpty();
    }
    return false;
}

bool KateExternalTool::valid(const QString& mt) const
{
    return mimetypes.isEmpty() || mimetypes.contains(mt);
}
// END KateExternalTool

// BEGIN KateExternalToolsCommand
KateExternalToolsCommand::KateExternalToolsCommand(KateExternalToolsPlugin* plugin)
    : KTextEditor::Command({})
    , m_plugin(plugin)
{
    reload();
}

// const QStringList& KateExternalToolsCommand::cmds()
// {
//     return m_list;
// }

void KateExternalToolsCommand::reload()
{
    m_list.clear();
    m_map.clear();
    m_name.clear();

    KConfig _config(QStringLiteral("externaltools"), KConfig::NoGlobals, QStandardPaths::ApplicationsLocation);
    KConfigGroup config(&_config, "Global");
    const QStringList tools = config.readEntry("tools", QStringList());

    for (QStringList::const_iterator it = tools.begin(); it != tools.end(); ++it) {
        if (*it == QStringLiteral("---"))
            continue;

        config = KConfigGroup(&_config, *it);

        KateExternalTool t
            = KateExternalTool(config.readEntry(QStringLiteral("name"), ""), config.readEntry("command", ""),
                               config.readEntry(QStringLiteral("icon"), ""), config.readEntry("executable", ""),
                               config.readEntry(QStringLiteral("mimetypes"), QStringList()),
                               config.readEntry(QStringLiteral("acname"), ""), config.readEntry("cmdname", ""));
        // FIXME test for a command name first!
        if (t.hasexec && (!t.cmdname.isEmpty())) {
            m_list.append(QStringLiteral("exttool-") + t.cmdname);
            m_map.insert(QStringLiteral("exttool-") + t.cmdname, t.acname);
            m_name.insert(QStringLiteral("exttool-") + t.cmdname, t.name);
        }
    }
}

bool KateExternalToolsCommand::exec(KTextEditor::View* view, const QString& cmd, QString& msg,
                                    const KTextEditor::Range& range)
{
    Q_UNUSED(msg)
    Q_UNUSED(range)

    QWidget* wv = dynamic_cast<QWidget*>(view);
    if (!wv) {
        //   qDebug()<<"KateExternalToolsCommand::exec: Could not get view widget";
        return false;
    }

    //  qDebug()<<"cmd="<<cmd.trimmed();
    QString actionName = m_map[cmd.trimmed()];
    if (actionName.isEmpty())
        return false;
    //  qDebug()<<"actionName is not empty:"<<actionName;
    /*  KateExternalToolsMenuAction *a =
        dynamic_cast<KateExternalToolsMenuAction*>(dmw->action("tools_external"));
      if (!a) return false;*/
    KateExternalToolsPluginView* extview = m_plugin->extView(wv->window());
    if (!extview)
        return false;
    if (!extview->externalTools)
        return false;
    //  qDebug()<<"trying to find action";
    QAction* a1 = extview->externalTools->actionCollection()->action(actionName);
    if (!a1)
        return false;
    //  qDebug()<<"activating action";
    a1->trigger();
    return true;
}

bool KateExternalToolsCommand::help(KTextEditor::View*, const QString&, QString&)
{
    return false;
}
// END KateExternalToolsCommand

// BEGIN KateExternalToolAction
KateExternalToolAction::KateExternalToolAction(QObject* parent, KateExternalTool* t)
    : QAction(QIcon::fromTheme(t->icon), t->name, parent)
    , tool(t)
{
    // setText( t->name );
    // if ( ! t->icon.isEmpty() )
    //  setIcon( KIcon( t->icon ) );

    connect(this, SIGNAL(triggered(bool)), SLOT(slotRun()));
}

bool KateExternalToolAction::expandMacro(const QString& str, QStringList& ret)
{
    KTextEditor::MainWindow* mw = qobject_cast<KTextEditor::MainWindow*>(parent()->parent());
    Q_ASSERT(mw);

    KTextEditor::View* view = mw->activeView();
    if (!view)
        return false;

    KTextEditor::Document* doc = view->document();
    QUrl url = doc->url();

    if (str == QStringLiteral("URL"))
        ret += url.url();
    else if (str == QStringLiteral("directory")) // directory of current doc
        ret += url.toString(QUrl::RemoveFilename);
    else if (str == QStringLiteral("filename"))
        ret += url.fileName();
    else if (str == QStringLiteral("line")) // cursor line of current doc
        ret += QString::number(view->cursorPosition().line());
    else if (str == QStringLiteral("col")) // cursor col of current doc
        ret += QString::number(view->cursorPosition().column());
    else if (str == QStringLiteral("selection")) // selection of current doc if any
        ret += view->selectionText();
    else if (str == QStringLiteral("text")) // text of current doc
        ret += doc->text();
    else if (str == QStringLiteral("URLs")) {
        foreach (KTextEditor::Document* it, KTextEditor::Editor::instance()->application()->documents())
            if (!it->url().isEmpty())
                ret += it->url().url();
    } else
        return false;

    return true;
}

KateExternalToolAction::~KateExternalToolAction()
{
    delete (tool);
}

void KateExternalToolAction::slotRun()
{
    // expand the macros in command if any,
    // and construct a command with an absolute path
    QString cmd = tool->command;

    KTextEditor::MainWindow* mw = qobject_cast<KTextEditor::MainWindow*>(parent()->parent());
    if (!expandMacrosShellQuote(cmd)) {
        KMessageBox::sorry(mw->window(), i18n("Failed to expand the command '%1'.", cmd), i18n("Kate External Tools"));
        return;
    }
    qDebug() << "externaltools: Running command: " << cmd;

    // save documents if requested
    if (tool->save == 1)
        mw->activeView()->document()->save();
    else if (tool->save == 2) {
        foreach (KXMLGUIClient* client, mw->guiFactory()->clients()) {
            if (QAction* a = client->actionCollection()->action(QStringLiteral("file_save_all"))) {
                a->trigger();
                break;
            }
        }
    }

    KRun::runCommand(cmd, tool->tryexec, tool->icon, mw->window());
}
// END KateExternalToolAction

// BEGIN KateExternalToolsMenuAction
KateExternalToolsMenuAction::KateExternalToolsMenuAction(const QString& text, KActionCollection* collection,
                                                         QObject* parent, KTextEditor::MainWindow* mw)
    : KActionMenu(text, parent)
    , mainwindow(mw)
{

    m_actionCollection = collection;

    // connect to view changed...
    connect(mw, &KTextEditor::MainWindow::viewChanged, this, &KateExternalToolsMenuAction::slotViewChanged);

    reload();
}

KateExternalToolsMenuAction::~KateExternalToolsMenuAction()
{
    // kDebug() << "deleted KateExternalToolsMenuAction";
}

void KateExternalToolsMenuAction::reload()
{
    bool needs_readd = (m_actionCollection->takeAction(this) != nullptr);
    m_actionCollection->clear();
    if (needs_readd)
        m_actionCollection->addAction(QStringLiteral("tools_external"), this);
    menu()->clear();

    // load all the tools, and create a action for each of them
    KSharedConfig::Ptr pConfig = KSharedConfig::openConfig(QStringLiteral("externaltools"), KConfig::NoGlobals,
                                                           QStandardPaths::ApplicationsLocation);
    KConfigGroup config(pConfig, "Global");
    QStringList tools = config.readEntry("tools", QStringList());

    // if there are tools that are present but not explicitly removed,
    // add them to the end of the list
    pConfig->setReadDefaults(true);
    QStringList dtools = config.readEntry("tools", QStringList());
    int gver = config.readEntry("version", 1);
    pConfig->setReadDefaults(false);

    int ver = config.readEntry("version", 0);
    if (ver <= gver) {
        QStringList removed = config.readEntry("removed", QStringList());
        bool sepadded = false;
        for (QStringList::iterator itg = dtools.begin(); itg != dtools.end(); ++itg) {
            if (!tools.contains(*itg) && !removed.contains(*itg)) {
                if (!sepadded) {
                    tools << QStringLiteral("---");
                    sepadded = true;
                }
                tools << *itg;
            }
        }

        config.writeEntry("tools", tools);
        config.sync();
        config.writeEntry("version", gver);
    }

    for (QStringList::const_iterator it = tools.constBegin(); it != tools.constEnd(); ++it) {
        if (*it == QStringLiteral("---")) {
            menu()->addSeparator();
            // a separator
            continue;
        }

        config = KConfigGroup(pConfig, *it);

        KateExternalTool* t = new KateExternalTool(
            config.readEntry("name", ""), config.readEntry("command", ""), config.readEntry("icon", ""),
            config.readEntry("executable", ""), config.readEntry("mimetypes", QStringList()),
            config.readEntry("acname", ""), config.readEntry("cmdname", ""), config.readEntry("save", 0));

        if (t->hasexec) {
            QAction* a = new KateExternalToolAction(this, t);
            m_actionCollection->addAction(t->acname, a);
            addAction(a);
        } else
            delete t;
    }

    config = KConfigGroup(pConfig, "Shortcuts");
    m_actionCollection->readSettings(&config);
    slotViewChanged(mainwindow->activeView());
}

void KateExternalToolsMenuAction::slotViewChanged(KTextEditor::View* view)
{
    // no active view, oh oh
    if (!view) {
        return;
    }

    // try to enable/disable to match current mime type
    KTextEditor::Document* doc = view->document();
    if (doc) {
        const QString mimeType = doc->mimeType();
        foreach (QAction* kaction, m_actionCollection->actions()) {
            KateExternalToolAction* action = dynamic_cast<KateExternalToolAction*>(kaction);
            if (action) {
                const QStringList l = action->tool->mimetypes;
                const bool b = (!l.count() || l.contains(mimeType));
                action->setEnabled(b);
            }
        }
    }
}
// END KateExternalToolsMenuAction

// BEGIN ToolItem
/**
 * This is a QListBoxItem, that has a KateExternalTool. The text is the Name
 * of the tool.
 */
class ToolItem : public QListWidgetItem
{
public:
    ToolItem(QListWidget* lb, const QPixmap& icon, KateExternalTool* tool)
        : QListWidgetItem(icon, tool->name, lb)
        , tool(tool)
    {
    }

    ~ToolItem() {}

    KateExternalTool* tool;
};
// END ToolItem

// BEGIN KateExternalToolServiceEditor
KateExternalToolServiceEditor::KateExternalToolServiceEditor(KateExternalTool* tool, QWidget* parent)
    : QDialog(parent)
    , tool(tool)
{
    setWindowTitle(i18n("Edit External Tool"));

    auto vbox = new QVBoxLayout(this);
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &KateExternalToolServiceEditor::slotOKClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // create a entry for each property
    // fill in the values from the service if available
    QWidget* w = new QWidget(this);
    vbox->addWidget(w);
    vbox->addWidget(buttonBox);

    QGridLayout* lo = new QGridLayout(w);
//     lo->setSpacing(KDialog::spacingHint()); // int spacing =  QApplication::style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);

    QLabel* l;

    leName = new QLineEdit(w);
    lo->addWidget(leName, 1, 2);
    l = new QLabel(w);
    l->setBuddy(leName);
    l->setText(i18n("&Label:"));
    l->setAlignment(l->alignment() | Qt::AlignRight);
    lo->addWidget(l, 1, 1);
    if (tool)
        leName->setText(tool->name);
    leName->setWhatsThis(i18n("The name will be displayed in the 'Tools->External' menu"));

    btnIcon = new KIconButton(w);
    btnIcon->setIconSize(KIconLoader::SizeSmall);
    lo->addWidget(btnIcon, 1, 3);
    if (tool && !tool->icon.isEmpty())
        btnIcon->setIcon(tool->icon);

    teCommand = new QTextEdit(w);
    lo->addWidget(teCommand, 2, 2, 1, 2);
    l = new QLabel(w);
    l->setBuddy(teCommand);
    l->setText(i18n("S&cript:"));
    l->setAlignment(Qt::AlignTop | Qt::AlignRight);
    lo->addWidget(l, 2, 1);
    if (tool)
        teCommand->setText(tool->command);
    teCommand->setWhatsThis(i18n("<p>The script to execute to invoke the tool. The script is passed "
                                 "to /bin/sh for execution. The following macros "
                                 "will be expanded:</p>"
                                 "<ul><li><code>%URL</code> - the URL of the current document.</li>"
                                 "<li><code>%URLs</code> - a list of the URLs of all open documents.</li>"
                                 "<li><code>%directory</code> - the URL of the directory containing "
                                 "the current document.</li>"
                                 "<li><code>%filename</code> - the filename of the current document.</li>"
                                 "<li><code>%line</code> - the current line of the text cursor in the "
                                 "current view.</li>"
                                 "<li><code>%column</code> - the column of the text cursor in the "
                                 "current view.</li>"
                                 "<li><code>%selection</code> - the selected text in the current view.</li>"
                                 "<li><code>%text</code> - the text of the current document.</li></ul>"));

    leExecutable = new QLineEdit(w);
    lo->addWidget(leExecutable, 3, 2, 1, 2);
    l = new QLabel(w);
    l->setBuddy(leExecutable);
    l->setText(i18n("&Executable:"));
    l->setAlignment(l->alignment() | Qt::AlignRight);
    lo->addWidget(l, 3, 1);
    if (tool)
        leExecutable->setText(tool->tryexec);
    leExecutable->setWhatsThis(i18n("The executable used by the command. This is used to check if a tool "
                                    "should be displayed; if not set, the first word of <em>command</em> "
                                    "will be used."));

    leMimetypes = new QLineEdit(w);
    lo->addWidget(leMimetypes, 4, 2);
    l = new QLabel(w);
    l->setBuddy(leMimetypes);
    l->setText(i18n("&Mime types:"));
    l->setAlignment(l->alignment() | Qt::AlignRight);
    lo->addWidget(l, 4, 1);
    if (tool)
        leMimetypes->setText(tool->mimetypes.join(QStringLiteral("; ")));
    leMimetypes->setWhatsThis(i18n("A semicolon-separated list of mime types for which this tool should "
                                   "be available; if this is left empty, the tool is always available. "
                                   "To choose from known mimetypes, press the button on the right."));

    QToolButton* btnMTW = new QToolButton(w);
    lo->addWidget(btnMTW, 4, 3);
    btnMTW->setIcon(QIcon::fromTheme(QStringLiteral("wizard")));
    connect(btnMTW, SIGNAL(clicked()), this, SLOT(showMTDlg()));
    btnMTW->setWhatsThis(i18n("Click for a dialog that can help you create a list of mimetypes."));

    cmbSave = new QComboBox(w);
    lo->addWidget(cmbSave, 5, 2, 1, 2);
    l = new QLabel(w);
    l->setBuddy(cmbSave);
    l->setText(i18n("&Save:"));
    l->setAlignment(l->alignment() | Qt::AlignRight);
    lo->addWidget(l, 5, 1);
    QStringList sl;
    sl << i18n("None") << i18n("Current Document") << i18n("All Documents");
    cmbSave->addItems(sl);
    if (tool)
        cmbSave->setCurrentIndex(tool->save);
    cmbSave->setWhatsThis(i18n("You can choose to save the current or all [modified] documents prior to "
                               "running the command. This is helpful if you want to pass URLs to "
                               "an application like, for example, an FTP client."));

    leCmdLine = new QLineEdit(w);
    lo->addWidget(leCmdLine, 6, 2, 1, 2);
    l = new QLabel(i18n("&Command line name:"), w);
    l->setBuddy(leCmdLine);
    l->setAlignment(l->alignment() | Qt::AlignRight);
    lo->addWidget(l, 6, 1);
    if (tool)
        leCmdLine->setText(tool->cmdname);
    leCmdLine->setWhatsThis(i18n("If you specify a name here, you can invoke the command from the view "
                                 "command line with exttool-the_name_you_specified_here. "
                                 "Please do not use spaces or tabs in the name."));
}

void KateExternalToolServiceEditor::slotOKClicked()
{
    if (leName->text().isEmpty() || teCommand->document()->isEmpty()) {
        QMessageBox::information(this, i18n("External Tool"), i18n("You must specify at least a name and a command"));
        return;
    }
    accept();
}

void KateExternalToolServiceEditor::showMTDlg()
{
    QString text = i18n("Select the MimeTypes for which to enable this tool.");
    QStringList list = leMimetypes->text().split(QRegExp(QStringLiteral("\\s*;\\s*")), QString::SkipEmptyParts);
    KMimeTypeChooserDialog d(i18n("Select Mime Types"), text, list, QStringLiteral("text"), this);
    if (d.exec() == QDialog::Accepted) {
        leMimetypes->setText(d.chooser()->mimeTypes().join(QStringLiteral(";")));
    }
}
// END KateExternalToolServiceEditor

// BEGIN KateExternalToolsConfigWidget
KateExternalToolsConfigWidget::KateExternalToolsConfigWidget(QWidget* parent, KateExternalToolsPlugin* plugin)
    : KTextEditor::ConfigPage(parent)
    , m_changed(false)
    , m_plugin(plugin)
{
    setupUi(this);

    btnMoveUp->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    btnMoveDown->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));

    connect(lbTools, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));
    connect(lbTools, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(slotEdit()));
    connect(btnNew, SIGNAL(clicked()), this, SLOT(slotNew()));
    connect(btnRemove, SIGNAL(clicked()), this, SLOT(slotRemove()));
    connect(btnEdit, SIGNAL(clicked()), this, SLOT(slotEdit()));
    connect(btnSeparator, SIGNAL(clicked()), this, SLOT(slotInsertSeparator()));
    connect(btnMoveUp, SIGNAL(clicked()), this, SLOT(slotMoveUp()));
    connect(btnMoveDown, SIGNAL(clicked()), this, SLOT(slotMoveDown()));

    config = new KConfig(QStringLiteral("externaltools"), KConfig::NoGlobals, QStandardPaths::ApplicationsLocation);
    reset();
    slotSelectionChanged();
}

KateExternalToolsConfigWidget::~KateExternalToolsConfigWidget()
{
    delete config;
}

QString KateExternalToolsConfigWidget::name() const
{
    return i18n("External Tools");
}

QString KateExternalToolsConfigWidget::fullName() const
{
    return i18n("External Tools");
}

QIcon KateExternalToolsConfigWidget::icon() const
{
    return QIcon();
}

void KateExternalToolsConfigWidget::reset()
{
    // m_tools.clear();
    lbTools->clear();

    // load the files from a KConfig
    const QStringList tools = config->group("Global").readEntry("tools", QStringList());

    for (QStringList::const_iterator it = tools.begin(); it != tools.end(); ++it) {
        if (*it == QStringLiteral("---")) {
            new QListWidgetItem(QStringLiteral("---"), lbTools);
        } else {
            KConfigGroup cg(config, *it);

            KateExternalTool* t
                = new KateExternalTool(cg.readEntry("name", ""), cg.readEntry("command", ""), cg.readEntry("icon", ""),
                                       cg.readEntry("executable", ""), cg.readEntry("mimetypes", QStringList()),
                                       cg.readEntry("acname"), cg.readEntry("cmdname"), cg.readEntry("save", 0));

            if (t->hasexec) // we only show tools that are also in the menu.
                new ToolItem(lbTools, t->icon.isEmpty() ? blankIcon() : SmallIcon(t->icon), t);
            else
                delete t;
        }
    }
    m_changed = false;
}

QPixmap KateExternalToolsConfigWidget::blankIcon()
{
    QPixmap pm(KIconLoader::SizeSmall, KIconLoader::SizeSmall);
    pm.fill();
    pm.setMask(pm.createHeuristicMask());
    return pm;
}

void KateExternalToolsConfigWidget::apply()
{
    if (!m_changed)
        return;
    m_changed = false;

    // save a new list
    // save each item
    QStringList tools;
    for (int i = 0; i < lbTools->count(); i++) {
        if (lbTools->item(i)->text() == QStringLiteral("---")) {
            tools << QStringLiteral("---");
            continue;
        }
        KateExternalTool* t = static_cast<ToolItem*>(lbTools->item(i))->tool;
        //     qDebug()<<"adding tool: "<<t->name;
        tools << t->acname;

        KConfigGroup cg(config, t->acname);

        cg.writeEntry("name", t->name);
        cg.writeEntry("command", t->command);
        cg.writeEntry("icon", t->icon);
        cg.writeEntry("executable", t->tryexec);
        cg.writeEntry("mimetypes", t->mimetypes);
        cg.writeEntry("acname", t->acname);
        cg.writeEntry("cmdname", t->cmdname);
        cg.writeEntry("save", t->save);
    }

    config->group("Global").writeEntry("tools", tools);

    // if any tools was removed, try to delete their groups, and
    // add the group names to the list of removed items.
    if (m_removed.count()) {
        for (QStringList::iterator it = m_removed.begin(); it != m_removed.end(); ++it) {
            if (config->hasGroup(*it))
                config->deleteGroup(*it);
        }
        QStringList removed = config->group("Global").readEntry("removed", QStringList());
        removed += m_removed;

        // clean up the list of removed items, so that it does not contain
        // non-existing groups (we can't remove groups from a non-owned global file).
        config->sync();
        QStringList::iterator it1 = removed.begin();
        while (it1 != removed.end()) {
            if (!config->hasGroup(*it1))
                it1 = removed.erase(it1);
            else
                ++it1;
        }
        config->group("Global").writeEntry("removed", removed);
    }

    config->sync();
    m_plugin->reload();
}

void KateExternalToolsConfigWidget::slotSelectionChanged()
{
    // update button state
    bool hs = lbTools->currentItem() != nullptr;
    btnEdit->setEnabled(hs && dynamic_cast<ToolItem*>(lbTools->currentItem()));
    btnRemove->setEnabled(hs);
    btnMoveUp->setEnabled((lbTools->currentRow() > 0) && hs);
    btnMoveDown->setEnabled((lbTools->currentRow() < (int)lbTools->count() - 1) && hs);
}

void KateExternalToolsConfigWidget::slotNew()
{
    // display a editor, and if it is OK'd, create a new tool and
    // create a listbox item for it
    KateExternalToolServiceEditor editor(nullptr, this);

    if (editor.exec()) {
        KateExternalTool* t = new KateExternalTool(
            editor.leName->text(), editor.teCommand->toPlainText(), editor.btnIcon->icon(), editor.leExecutable->text(),
            editor.leMimetypes->text().split(QRegExp(QStringLiteral("\\s*;\\s*")), QString::SkipEmptyParts));

        // This is sticky, it does not change again, so that shortcuts sticks
        // TODO check for dups
        t->acname = QStringLiteral("externaltool_") + QString(t->name).remove(QRegExp(QStringLiteral("\\W+")));

        new ToolItem(lbTools, t->icon.isEmpty() ? blankIcon() : SmallIcon(t->icon), t);

        emit changed();
        m_changed = true;
    }
}

void KateExternalToolsConfigWidget::slotRemove()
{
    // add the tool action name to a list of removed items,
    // remove the current listbox item
    if (lbTools->currentRow() > -1) {
        ToolItem* i = dynamic_cast<ToolItem*>(lbTools->currentItem());
        if (i)
            m_removed << i->tool->acname;

        delete lbTools->takeItem(lbTools->currentRow());
        emit changed();
        m_changed = true;
    }
}

void KateExternalToolsConfigWidget::slotEdit()
{
    if (!dynamic_cast<ToolItem*>(lbTools->currentItem()))
        return;
    // show the item in an editor
    KateExternalTool* t = static_cast<ToolItem*>(lbTools->currentItem())->tool;
    KateExternalToolServiceEditor editor(t, this);
    editor.resize(config->group("Editor").readEntry("Size", QSize()));
    if (editor.exec() /*== KDialog::Ok*/) {

        bool elementChanged = ((editor.btnIcon->icon() != t->icon) || (editor.leName->text() != t->name));

        t->name = editor.leName->text();
        t->cmdname = editor.leCmdLine->text();
        t->command = editor.teCommand->toPlainText();
        t->icon = editor.btnIcon->icon();
        t->tryexec = editor.leExecutable->text();
        t->mimetypes = editor.leMimetypes->text().split(QRegExp(QStringLiteral("\\s*;\\s*")), QString::SkipEmptyParts);
        t->save = editor.cmbSave->currentIndex();

        // if the icon has changed or name changed, I have to renew the listbox item :S
        if (elementChanged) {
            int idx = lbTools->row(lbTools->currentItem());
            delete lbTools->takeItem(idx);
            lbTools->insertItem(idx, new ToolItem(nullptr, t->icon.isEmpty() ? blankIcon() : SmallIcon(t->icon), t));
        }

        emit changed();
        m_changed = true;
    }

    config->group("Editor").writeEntry("Size", editor.size());
    config->sync();
}

void KateExternalToolsConfigWidget::slotInsertSeparator()
{
    lbTools->insertItem(lbTools->currentRow() + 1, QStringLiteral("---"));
    emit changed();
    m_changed = true;
}

void KateExternalToolsConfigWidget::slotMoveUp()
{
    // move the current item in the listbox upwards if possible
    QListWidgetItem* item = lbTools->currentItem();
    if (!item)
        return;

    int idx = lbTools->row(item);

    if (idx < 1)
        return;

    if (dynamic_cast<ToolItem*>(item)) {
        KateExternalTool* tool = static_cast<ToolItem*>(item)->tool;
        delete lbTools->takeItem(idx);
        lbTools->insertItem(idx - 1,
                            new ToolItem(nullptr, tool->icon.isEmpty() ? blankIcon() : SmallIcon(tool->icon), tool));
    } else // a separator!
    {
        delete lbTools->takeItem(idx);
        lbTools->insertItem(idx - 1, new QListWidgetItem(QStringLiteral("---")));
    }

    lbTools->setCurrentRow(idx - 1);
    slotSelectionChanged();
    emit changed();
    m_changed = true;
}

void KateExternalToolsConfigWidget::slotMoveDown()
{
    // move the current item in the listbox downwards if possible
    QListWidgetItem* item = lbTools->currentItem();
    if (!item)
        return;

    int idx = lbTools->row(item);

    if (idx > lbTools->count() - 1)
        return;

    if (dynamic_cast<ToolItem*>(item)) {
        KateExternalTool* tool = static_cast<ToolItem*>(item)->tool;
        delete lbTools->takeItem(idx);
        lbTools->insertItem(idx + 1,
                            new ToolItem(nullptr, tool->icon.isEmpty() ? blankIcon() : SmallIcon(tool->icon), tool));
    } else // a separator!
    {
        delete lbTools->takeItem(idx);
        lbTools->insertItem(idx + 1, new QListWidgetItem(QStringLiteral("---")));
    }

    lbTools->setCurrentRow(idx + 1);
    slotSelectionChanged();
    emit changed();
    m_changed = true;
}
// END KateExternalToolsConfigWidget
// kate: space-indent on; indent-width 4; replace-tabs on;
