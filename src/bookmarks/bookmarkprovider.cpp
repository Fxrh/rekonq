/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2008-2010 by Andrea Diamantini <adjam7 at gmail dot com>
* Copyright (C) 2009 by Paweł Prażak <pawelprazak at gmail dot com>
* Copyright (C) 2009-2010 by Lionel Chauvin <megabigbug@yahoo.fr>
* Copyright (C) 2010 by Yoann Laissus <yoann dot laissus at gmail dot com>
*
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License or (at your option) version 3 or any later version
* accepted by the membership of KDE e.V. (or its successor approved
* by the membership of KDE e.V.), which shall act as a proxy
* defined in Section 14 of version 3 of the license.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* ============================================================ */


// Self Includes
#include "bookmarkprovider.h"
#include "bookmarkprovider.moc"

// Local Includes
#include "application.h"
#include "bookmarkspanel.h"
#include "bookmarkstoolbar.h"
#include "bookmarkowner.h"
#include "iconmanager.h"
#include "mainwindow.h"
#include "webtab.h"

// KDE Includes
#include <KActionCollection>
#include <KStandardDirs>

// Qt Includes
#include <QtCore/QFile>


BookmarkProvider::BookmarkProvider(QObject *parent)
        : QObject(parent)
        , m_manager(0)
        , m_owner(0)
        , m_actionCollection(new KActionCollection(this))
{
    kDebug() << "Loading Bookmarks Manager...";

    // NOTE
    // This hackish code is needed to continue sharing bookmarks with konqueror,
    // until we can (hopefully) start using an akonadi resource.
    //
    // The cleanest code has a subdole bug inside does not allowing people to start
    // using rekonq and then using konqueror. So if konqueror bk file has not just been created,
    // bk are stored in rekonq dir and then they will be lost when you start using konqi

    KUrl bookfile = KUrl("~/.kde/share/apps/konqueror/bookmarks.xml");  // share konqueror bookmarks
    if (!QFile::exists(bookfile.path()))
    {
        bookfile = KUrl("~/.kde4/share/apps/konqueror/bookmarks.xml");
        if (!QFile::exists(bookfile.path()))
        {
            QString bookmarksDefaultPath = KStandardDirs::locate("appdata" , "defaultbookmarks.xbel");
            QFile bkms(bookmarksDefaultPath);
            QString bookmarksPath = KStandardDirs::locateLocal("appdata", "bookmarks.xml", true);
            bookmarksPath.replace("rekonq", "konqueror");
            bkms.copy(bookmarksPath);
            bookfile = KUrl(bookmarksPath);
        }
    }

    m_manager = KBookmarkManager::managerForFile(bookfile.path(), "rekonq");
    m_manager->setEditorOptions("", true);

    connect(m_manager, SIGNAL(changed(const QString &, const QString &)),
            this, SLOT(slotBookmarksChanged()));

    // setup menu
    m_owner = new BookmarkOwner(m_manager, this);
    connect(m_owner, SIGNAL(openUrl(const KUrl&, const Rekonq::OpenType&)),
            this, SIGNAL(openUrl(const KUrl&, const Rekonq::OpenType&)));

    KAction *a = KStandardAction::addBookmark(bookmarkOwner(), SLOT(bookmarkCurrentPage()), this);
    m_actionCollection->addAction(QL1S("rekonq_add_bookmark"), a);

    kDebug() << "Loading Bookmarks Manager... DONE!";
}


BookmarkProvider::~BookmarkProvider()
{
    qDeleteAll(m_bookmarkMenus);
    delete m_manager;
}


KActionMenu* BookmarkProvider::bookmarkActionMenu(QWidget *parent)
{
    kDebug() << "new Bookmarks menu...";

    KMenu *menu = new KMenu(parent);
    KActionMenu *bookmarkActionMenu = new KActionMenu(menu);
    bookmarkActionMenu->setMenu(menu);
    bookmarkActionMenu->setText(i18n("&Bookmarks"));
    BookmarkMenu *bMenu = new BookmarkMenu(m_manager, m_owner, menu, m_actionCollection);
    m_bookmarkMenus.append(bMenu);

    kDebug() << "new Bookmarks menu... DONE";

    return bookmarkActionMenu;
}


void BookmarkProvider::registerBookmarkBar(BookmarkToolBar *toolbar)
{
    if (m_bookmarkToolBars.contains(toolbar))
        return;

    kDebug() << "new bookmark bar...";

    m_bookmarkToolBars.append(toolbar);

    kDebug() << "new bookmark bar... DONE!";
}


void BookmarkProvider::removeBookmarkBar(BookmarkToolBar *toolbar)
{
    m_bookmarkToolBars.removeOne(toolbar);
}


void BookmarkProvider::registerBookmarkPanel(BookmarksPanel *panel)
{
    if (panel && !m_bookmarkPanels.contains(panel))
    {
        m_bookmarkPanels.append(panel);
        connect(panel, SIGNAL(expansionChanged()), this, SLOT(slotPanelChanged()));
    }
}


void BookmarkProvider::removeBookmarkPanel(BookmarksPanel *panel)
{
    if (!panel)
        return;

    m_bookmarkPanels.removeOne(panel);
    panel->disconnect(this);

    if (m_bookmarkPanels.isEmpty())
        Application::bookmarkProvider()->bookmarkManager()->emitChanged();
}


QAction* BookmarkProvider::actionByName(const QString &name)
{
    QAction *action = m_actionCollection->action(name);
    if (action)
        return action;
    return new QAction(this);
}


KBookmarkGroup BookmarkProvider::rootGroup()
{
    return m_manager->root();
}


QList<KBookmark> BookmarkProvider::find(const QString &text)
{
    QList<KBookmark> list;

    KBookmarkGroup root = Application::bookmarkProvider()->rootGroup();
    if (!root.isNull())
        for (KBookmark bookmark = root.first(); !bookmark.isNull(); bookmark = root.next(bookmark))
            find(&list, bookmark, text);

    return list;
}


KBookmark BookmarkProvider::bookmarkForUrl(const KUrl &url)
{
    KBookmarkGroup root = rootGroup();
    if (root.isNull())
        return KBookmark();

    return bookmarkForUrl(root, url);
}


void BookmarkProvider::slotBookmarksChanged()
{
    foreach(BookmarkToolBar *bookmarkToolBar, m_bookmarkToolBars)
    {
        if (bookmarkToolBar)
        {
            bookmarkToolBar->toolBar()->clear();
            fillBookmarkBar(bookmarkToolBar);
        }
    }
    if(Application::instance()->mainWindow()->currentTab()->url().toMimeDataString().contains("about:bookmarks"))
        Application::instance()->loadUrl(KUrl("about:bookmarks"), Rekonq::CurrentTab);
}


void BookmarkProvider::fillBookmarkBar(BookmarkToolBar *toolBar)
{
    KBookmarkGroup root = m_manager->toolbar();
    if (root.isNull())
        return;

    for (KBookmark bookmark = root.first(); !bookmark.isNull(); bookmark = root.next(bookmark))
    {
        if (bookmark.isGroup())
        {
            KBookmarkActionMenu *menuAction = new KBookmarkActionMenu(bookmark.toGroup(), this);
            menuAction->setDelayed(false);
            BookmarkMenu *bMenu = new BookmarkMenu(bookmarkManager(), bookmarkOwner(), menuAction->menu(), bookmark.address());
            m_bookmarkMenus.append(bMenu);

            connect(menuAction->menu(), SIGNAL(aboutToShow()), toolBar, SLOT(menuDisplayed()));
            connect(menuAction->menu(), SIGNAL(aboutToHide()), toolBar, SLOT(menuHidden()));

            toolBar->toolBar()->addAction(menuAction);
            toolBar->toolBar()->widgetForAction(menuAction)->installEventFilter(toolBar);
        }
        else if (bookmark.isSeparator())
        {
            toolBar->toolBar()->addSeparator();
        }
        else
        {
            KBookmarkAction *action = new KBookmarkAction(bookmark, m_owner, this);
            action->setIcon(Application::iconManager()->iconForUrl( KUrl(bookmark.url()) ));
            connect(action, SIGNAL(hovered()), toolBar, SLOT(actionHovered()));
            toolBar->toolBar()->addAction(action);
            toolBar->toolBar()->widgetForAction(action)->installEventFilter(toolBar);
        }
    }
}


void BookmarkProvider::slotPanelChanged()
{
    foreach (BookmarksPanel *panel, m_bookmarkPanels)
    {
        if (panel && panel != sender())
            panel->loadFoldedState();
    }
    if(Application::instance()->mainWindow()->currentTab()->url().toMimeDataString().contains("about:bookmarks"))
        Application::instance()->loadUrl(KUrl("about:bookmarks"), Rekonq::CurrentTab);
}


void BookmarkProvider::find(QList<KBookmark> *list, const KBookmark &bookmark, const QString &text)
{
    if (bookmark.isGroup())
    {
        KBookmarkGroup group = bookmark.toGroup();
        for (KBookmark bm = group.first(); !bm.isNull(); bm = group.next(bm))
            find(list, bm, text);
    }
    else
    {
        QStringList words = text.split(' ');
        bool matches = true;
        foreach (const QString &word, words)
        {
            if (!bookmark.url().url().contains(word, Qt::CaseInsensitive)
                && !bookmark.fullText().contains(word, Qt::CaseInsensitive))
            {
                matches = false;
                break;
            }
        }
        if (matches)
            *list << bookmark;
    }
}


KBookmark BookmarkProvider::bookmarkForUrl(const KBookmark &bookmark, const KUrl &url)
{
    KBookmark found;

    if (bookmark.isGroup())
    {
        KBookmarkGroup group = bookmark.toGroup();
        KBookmark bookmark = group.first();

        while (!bookmark.isNull() && found.isNull())
        {
            found = bookmarkForUrl(bookmark, url);
            bookmark = group.next(bookmark);
        }
    }
    else if (!bookmark.isSeparator() && bookmark.url() == url)
    {
        found = bookmark;
    }

    return found;
}
