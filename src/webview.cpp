/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2008-2010 by Andrea Diamantini <adjam7 at gmail dot com>
* Copyright (C) 2009-2010 by Lionel Chauvin <megabigbug@yahoo.fr>
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
#include "webview.h"
#include "webview.moc"

// Auto Includes
#include "rekonq.h"

// Local Includes
#include "application.h"
#include "bookmarkprovider.h"
#include "bookmarkowner.h"
#include "iconmanager.h"
#include "mainview.h"
#include "mainwindow.h"
#include "searchengine.h"
#include "urlbar.h"
#include "webpage.h"
#include "websnap.h"

// KDE Includes
#include <KAction>
#include <KActionMenu>
#include <KLocalizedString>
#include <KMenu>
#include <KStandardAction>
#include <KStandardDirs>

// Qt Includes
#include <QtCore/QFile>
#include <QtCore/QTimer>

#include <QtGui/QClipboard>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QWindowsStyle>

#include <QtWebKit/QWebFrame>
#include <QtWebKit/QWebHistory>


WebView::WebView(QWidget* parent)
        : KWebView(parent, false)
        , m_mousePos(QPoint(0, 0))
        , m_autoScrollTimer(new QTimer(this))
        , m_vScrollSpeed(0)
        , m_hScrollSpeed(0)
        , m_canEnableAutoScroll(true)
        , m_isAutoScrollEnabled(false)
        , m_smoothScrollTimer(new QTimer(this))
        , m_smoothScrolling(false)
        , m_dy(0)
        , m_smoothScrollSteps(0)
        , m_autoReloadMenu(0)
        , m_autoReloadTimer(0)
{
    WebPage *page = new WebPage(this);
    setPage(page);

    // // NOTE This is a lot hackish. We copied it from Arora, but using a "Windows Style"
    // // seems really a pity to me. The problem is that using a KStyle everything seems "broken"
    // // (at least with dark colors). So I think the code should be somthing like:
    // KStyle s;
    // setPalette( s.standardPalette() );
    // // But it doesn't work :(
    // // We'll see in next KDE releases...
    QPalette p;
    if (p.color(QPalette::ButtonText).lightness() > 50) { //if it is a dark theme
        QWindowsStyle s;
        p = s.standardPalette();
        setPalette(p);
    }

    // download system
    connect(this, SIGNAL(linkShiftClicked(const KUrl &)), page, SLOT(downloadUrl(const KUrl &)));

    // middle click || ctrl + click signal
    connect(this, SIGNAL(linkMiddleOrCtrlClicked(const KUrl &)), this, SLOT(loadUrlInNewTab(const KUrl &)));

    // loadUrl signal
    connect(this, SIGNAL(loadUrl(const KUrl &, const Rekonq::OpenType &)),
            Application::instance(), SLOT(loadUrl(const KUrl &, const Rekonq::OpenType &)));

    // Auto scroll timer
    connect(m_autoScrollTimer, SIGNAL(timeout()), this, SLOT(scrollFrameChanged()));
    m_autoScrollTimer->setInterval(100);

    // Smooth scroll timer
    connect(m_smoothScrollTimer, SIGNAL(timeout()), this, SLOT(scrollTick()));
    m_smoothScrollTimer->setInterval(16);
}


WebView::~WebView()
{
    if (m_smoothScrolling)
        stopScrolling();

    WebPage* p = page();

    QPixmap preview = WebSnap::renderClosingPagePreview(*p);
    QString path = WebSnap::imagePathFromUrl(p->mainFrame()->url().toString());
    QFile::remove(path);
    preview.save(path);
}


WebPage *WebView::page()
{
    WebPage *const page = qobject_cast<WebPage *>(KWebView::page());
    Q_ASSERT(page);
    return page;
}


void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    QWebHitTestResult result = page()->mainFrame()->hitTestContent(event->pos());
    MainWindow *mainwindow = Application::instance()->mainWindow();

    KMenu menu(this);
    QAction *a;

    KAction *inspectAction = new KAction(KIcon("layer-visible-on"), i18n("Inspect Element"), this);
    connect(inspectAction, SIGNAL(triggered(bool)), this, SLOT(inspect()));

    // is a link?
    if (!result.linkUrl().isEmpty())
    {
        // link actions
        a = new KAction(KIcon("tab-new"), i18n("Open in New &Tab"), this);
        a->setData(result.linkUrl());
        connect(a, SIGNAL(triggered(bool)), this, SLOT(openLinkInNewTab()));
        menu.addAction(a);

        a = new KAction(KIcon("window-new"), i18n("Open in New &Window"), this);
        a->setData(result.linkUrl());
        connect(a, SIGNAL(triggered(bool)), this, SLOT(openLinkInNewWindow()));
        menu.addAction(a);

        menu.addAction(pageAction(KWebPage::DownloadLinkToDisk));
        menu.addAction(pageAction(KWebPage::CopyLinkToClipboard));
        menu.addSeparator();
    }

    // add find action.
    if(result.pixmap().isNull())
    {
        a = mainwindow->actionByName(KStandardAction::name(KStandardAction::Find));
        menu.addAction(a);
    }
    
    // is content editable && selected? Add CUT
    if (result.isContentEditable() && result.isContentSelected())
    {
        // actions for text selected in field
        menu.addAction(pageAction(KWebPage::Cut));
    }

    // is content selected) Add COPY
    if (result.isContentSelected())
    {
        a = pageAction(KWebPage::Copy);
        if (!result.linkUrl().isEmpty())
            a->setText(i18n("Copy Text")); //for link
        else
            a->setText(i18n("Copy"));
        menu.addAction(a);
        if(selectedText().contains('.') && selectedText().indexOf('.') < selectedText().length() && !selectedText().trimmed().contains(" "))
        {
            QString text = selectedText();
            text = text.trimmed();
            KUrl urlLikeText(text);
            if(urlLikeText.isValid())
            {
                QString truncatedUrl = text;
                const int maxTextSize = 18;
                if (truncatedUrl.length() > maxTextSize)
                {
                    const int truncateSize = 15;
                    truncatedUrl.truncate(truncateSize);
                    truncatedUrl += QL1S("...");
                }
                //open selected text url in a new tab
                QAction * const openInNewTabAction = new KAction(KIcon("tab-new"), i18n("Open '%1' in New Tab", truncatedUrl), this);
                openInNewTabAction->setData(QUrl(urlLikeText));
                connect(openInNewTabAction, SIGNAL(triggered(bool)), this, SLOT(openLinkInNewTab()));
                menu.addAction(openInNewTabAction);
                //open selected text url in a new window
                QAction * const openInNewWindowAction = new KAction(KIcon("window-new"), i18n("Open '%1' in New Window", truncatedUrl), this);
                openInNewWindowAction->setData(QUrl(urlLikeText));
                connect(openInNewWindowAction, SIGNAL(triggered(bool)), this, SLOT(openLinkInNewWindow()));
                menu.addAction(openInNewWindowAction);
                menu.addSeparator();
            }
        }
    }

    // is content editable? Add PASTE
    if (result.isContentEditable())
    {
        menu.addAction(pageAction(KWebPage::Paste));
    }
    a = mainwindow->actionByName(KStandardAction::name(KStandardAction::Print));
    menu.addAction(a);
    // is content selected? Add SEARCH actions
    if (result.isContentSelected())
    {
        KActionMenu *searchMenu = new KActionMenu(KIcon("edit-find"), i18n("Search with"), this);

        foreach(KService::Ptr engine, SearchEngine::favorites())
        {
            a = new KAction(engine->name(), this);
            a->setIcon(Application::iconManager()->iconForUrl(SearchEngine::buildQuery(engine, "")));
            a->setData(engine->entryPath());
            connect(a, SIGNAL(triggered(bool)), this, SLOT(search()));
            searchMenu->addAction(a);
        }

        if (!searchMenu->menu()->isEmpty())
        {
            menu.addAction(searchMenu);
        }

        menu.addSeparator();

        menu.addAction(inspectAction);
        // TODO Add translate, show translation
    }

    // is an image?
    if (!result.pixmap().isNull())
    {
        menu.addSeparator();

        // TODO remove copy_this_image action
        a = new KAction(KIcon("view-media-visualization"), i18n("&View Image"), this);
        a->setData(result.imageUrl());
        connect(a, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)), this, SLOT(viewImage(Qt::MouseButtons, Qt::KeyboardModifiers)));
        menu.addAction(a);

        menu.addAction(pageAction(KWebPage::DownloadImageToDisk));
        menu.addAction(pageAction(KWebPage::CopyImageToClipboard));

        a = new KAction(KIcon("view-media-visualization"), i18n("&Copy Image Location"), this);
        a->setData(result.imageUrl());
        connect(a, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)), this, SLOT(slotCopyImageLocation()));
        menu.addAction(a);

        menu.addAction(pageAction(KWebPage::CopyLinkToClipboard));
        menu.addSeparator();

        menu.addAction(inspectAction);
    }

    // Open url text in new tab/window
    if (result.linkUrl().isEmpty())
    {

        QString text = selectedText();
        text = text.trimmed();
        if (text.startsWith(QL1S("http://"))
                || text.startsWith(QL1S("https://"))
                || text.startsWith(QL1S("www."))
           )
        {
            QString truncatedURL = text;
            if (text.length() > 18)
            {
                truncatedURL.truncate(15);
                truncatedURL += "...";
            }

            //open selected text url in a new tab
            a = new KAction(KIcon("tab-new"), i18n("Open '%1' in New Tab", truncatedURL), this);
            a->setData(QUrl(text));
            connect(a, SIGNAL(triggered(bool)), this, SLOT(openLinkInNewTab()));
            menu.addAction(a);

            //open selected text url in a new window
            a = new KAction(KIcon("window-new"), i18n("Open '%1' in New Window", truncatedURL), this);
            a->setData(QUrl(text));
            connect(a, SIGNAL(triggered(bool)), this, SLOT(openLinkInNewWindow()));
            menu.addAction(a);

            menu.addSeparator();
        }
    }

    // page actions
    if (!result.isContentSelected() && result.linkUrl().isEmpty())
    {

        // navigation
        QWebHistory *history = page()->history();
        if (history->canGoBack())
        {
            menu.addAction(pageAction(KWebPage::Back));
        }

        if (history->canGoForward())
        {
            menu.addAction(pageAction(KWebPage::Forward));
        }

        menu.addAction(mainwindow->actionByName("view_redisplay"));

        if (result.pixmap().isNull())
        {
            menu.addSeparator();

            menu.addAction(mainwindow->actionByName("new_tab"));
            menu.addAction(mainwindow->actionByName("new_window"));

            menu.addSeparator();

            //Frame
            KActionMenu *frameMenu = new KActionMenu(i18n("Current Frame"), this);

            frameMenu->addAction(pageAction(KWebPage::OpenFrameInNewWindow));

            a = new KAction(KIcon("document-print-frame"), i18n("Print Frame"), this);
            connect(a, SIGNAL(triggered()), this, SLOT(printFrame()));
            frameMenu->addAction(a);

            menu.addAction(frameMenu);

            menu.addSeparator();

            // auto reload actions
            if( !m_autoReloadMenu )
            {
                m_autoReloadMenu = new KMenu(i18n("Auto Reload"), this);
                QActionGroup *actionGroup = new QActionGroup(this);
                actionGroup->setExclusive(true);
                QAction *action = actionGroup->addAction(i18n("No Reload"));
                action->setData(0);
                action->setCheckable(true);
                action->setChecked(true);
                m_autoReloadMenu->addAction(action);
                action = actionGroup->addAction(i18n("5 Seconds"));
                action->setData(5);
                action->setCheckable(true);
                m_autoReloadMenu->addAction(action);
                action = actionGroup->addAction(i18n("15 Seconds"));
                action->setData(15);
                action->setCheckable(true);
                m_autoReloadMenu->addAction(action);
                action = actionGroup->addAction(i18n("30 Seconds"));
                action->setData(30);
                action->setCheckable(true);
                m_autoReloadMenu->addAction(action);
                action = actionGroup->addAction(i18n("1 Minute"));
                action->setData(60);
                action->setCheckable(true);
                m_autoReloadMenu->addAction(action);
                action = actionGroup->addAction(i18n("2 Minutes"));
                action->setData(120);
                action->setCheckable(true);
                m_autoReloadMenu->addAction(action);
                action = actionGroup->addAction(i18n("5 Minutes"));
                action->setData(300);
                action->setCheckable(true);
                m_autoReloadMenu->addAction(action);
                action = actionGroup->addAction(i18n("15 Minutes"));
                action->setData(15*60);
                action->setCheckable(true);
                m_autoReloadMenu->addAction(action);
                action = actionGroup->addAction(i18n("30 Minutes"));
                action->setData(30*60);
                action->setCheckable(true);
                m_autoReloadMenu->addAction(action);

                connect( actionGroup, SIGNAL(triggered(QAction*)), this, SLOT(changeAutoReload(QAction*)) );
            }
            menu.addMenu(m_autoReloadMenu);
            menu.addSeparator();

            // Page Actions
            menu.addAction(pageAction(KWebPage::SelectAll));

            menu.addAction(mainwindow->actionByName(KStandardAction::name(KStandardAction::SaveAs)));

            if (!KStandardDirs::findExe("kget").isNull() && ReKonfig::kgetList())
            {
                a = new KAction(KIcon("kget"), i18n("List All Links"), this);
                connect(a, SIGNAL(triggered(bool)), page(), SLOT(downloadAllContentsWithKGet()));
                menu.addAction(a);
            }

            menu.addAction(mainwindow->actionByName("page_source"));

            menu.addAction(inspectAction);

            a = Application::bookmarkProvider()->actionByName("rekonq_add_bookmark");
            menu.addAction(a);
        }

        if (mainwindow->isFullScreen())
        {
            menu.addSeparator();
            menu.addAction(mainwindow->actionByName("fullscreen"));
        }
    }

    menu.exec(mapToGlobal(event->pos()));
}


void WebView::mousePressEvent(QMouseEvent *event)
{
    if (m_isAutoScrollEnabled)
    {
        setCursor(Qt::ArrowCursor);
        m_vScrollSpeed = 0;
        m_hScrollSpeed = 0;
        m_autoScrollTimer->stop();
        m_isAutoScrollEnabled = false;
        return;
    }

    QWebHitTestResult result = page()->mainFrame()->hitTestContent(event->pos());
    m_canEnableAutoScroll = ReKonfig::autoScroll() && !result.isContentEditable()  && result.linkUrl().isEmpty();

    switch (event->button())
    {
    case Qt::XButton1:
        triggerPageAction(KWebPage::Back);
        break;

    case Qt::XButton2:
        triggerPageAction(KWebPage::Forward);
        break;

    case Qt::MidButton:
        if (m_canEnableAutoScroll
            && !m_isAutoScrollEnabled
            && !page()->currentFrame()->scrollBarGeometry(Qt::Horizontal).contains(event->pos())
            && !page()->currentFrame()->scrollBarGeometry(Qt::Vertical).contains(event->pos()))
        {
            if (!page()->currentFrame()->scrollBarGeometry(Qt::Horizontal).isNull()
                    || !page()->currentFrame()->scrollBarGeometry(Qt::Vertical).isNull())
            {
                setCursor(KIcon("transform-move").pixmap(32));
                m_clickPos = event->pos();
                m_isAutoScrollEnabled = true;
            }
        }
        break;

    default:
        break;
    };
    KWebView::mousePressEvent(event);
}


void WebView::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();

    if (m_isAutoScrollEnabled)
    {
        QPoint r = m_mousePos - m_clickPos;
        m_hScrollSpeed = r.x() / 2;  // you are too fast..
        m_vScrollSpeed = r.y() / 2;
        if (!m_autoScrollTimer->isActive())
            m_autoScrollTimer->start();

        return;
    }

    MainWindow *w = Application::instance()->mainWindow();
    if (w->isFullScreen())
    {
        if (event->pos().y() >= 0 && event->pos().y() <= 4)
        {
            w->setWidgetsVisible(true);
        }
        else
        {
            if(!w->mainView()->currentUrlBar()->hasFocus())
                w->setWidgetsVisible(false);
        }
    }
    KWebView::mouseMoveEvent(event);
}


void WebView::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat("application/rekonq-bookmark"))
    {
        QByteArray addresses = event->mimeData()->data("application/rekonq-bookmark");
        KBookmark bookmark =  Application::bookmarkProvider()->bookmarkManager()->findByAddress(QString::fromLatin1(addresses.data()));
        if (bookmark.isGroup())
        {
            Application::bookmarkProvider()->bookmarkOwner()->openFolderinTabs(bookmark.toGroup());
        }
        else
        {
            emit loadUrl(bookmark.url(), Rekonq::CurrentTab);
        }
    }
    else
    {
        KWebView::dropEvent(event);
    }
}

void WebView::search()
{
    KAction *a = qobject_cast<KAction*>(sender());
    KService::Ptr engine = KService::serviceByDesktopPath(a->data().toString());
    KUrl urlSearch = KUrl(SearchEngine::buildQuery(engine, selectedText()));

    emit loadUrl(urlSearch, Rekonq::NewTab);
}


void WebView::printFrame()
{
    Application::instance()->mainWindow()->printRequested(page()->currentFrame());
}


void WebView::viewImage(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    KAction *a = qobject_cast<KAction*>(sender());
    KUrl url(a->data().toUrl());

    if (modifiers & Qt::ControlModifier || buttons == Qt::MidButton)
    {
        emit loadUrl(url, Rekonq::NewTab);
    }
    else
    {
        emit loadUrl(url, Rekonq::CurrentTab);
    }
}

void WebView::slotCopyImageLocation()
{
    KAction *a = qobject_cast<KAction*>(sender());
    KUrl imageUrl (a->data().toUrl());
#ifndef QT_NO_MIMECLIPBOARD
    // Set it in both the mouse selection and in the clipboard
    QMimeData* mimeData = new QMimeData;
    imageUrl.populateMimeData( mimeData );
    QApplication::clipboard()->setMimeData( mimeData, QClipboard::Clipboard );
    mimeData = new QMimeData;
    imageUrl.populateMimeData( mimeData );
    QApplication::clipboard()->setMimeData( mimeData, QClipboard::Selection );
#else
    QApplication::clipboard()->setText( imageUrl.url() );
#endif
}


void WebView::openLinkInNewWindow()
{
    KAction *a = qobject_cast<KAction*>(sender());
    KUrl url(a->data().toUrl());

    emit loadUrl(url, Rekonq::NewWindow);
}


void WebView::openLinkInNewTab()
{
    KAction *a = qobject_cast<KAction*>(sender());
    KUrl url(a->data().toUrl());

    emit loadUrl(url, Rekonq::NewTab);
}


void WebView::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_C)
        {
            triggerPageAction(KWebPage::Copy);
            return;
        }

        if (event->key() == Qt::Key_A)
        {
            triggerPageAction(KWebPage::SelectAll);
            return;
        }
    }

    if (!m_canEnableAutoScroll)
    {
        KWebView::keyPressEvent(event);
        return;
    }

    // Auto Scrolling
    if (event->modifiers() == Qt::ShiftModifier)
    {
        if (event->key() == Qt::Key_Up)
        {
            m_vScrollSpeed--;
            if (!m_autoScrollTimer->isActive())
                m_autoScrollTimer->start();
            return;
        }

        if (event->key() == Qt::Key_Down)
        {
            m_vScrollSpeed++;
            if (!m_autoScrollTimer->isActive())
                m_autoScrollTimer->start();
            return;
        }

        if (event->key() == Qt::Key_Right)
        {
            m_hScrollSpeed++;
            if (!m_autoScrollTimer->isActive())
                m_autoScrollTimer->start();
            return;
        }

        if (event->key() == Qt::Key_Left)
        {
            m_hScrollSpeed--;
            if (!m_autoScrollTimer->isActive())
                m_autoScrollTimer->start();
            return;
        }

        if (m_autoScrollTimer->isActive())
        {
            m_autoScrollTimer->stop();
        }
        else
        {
            if (m_vScrollSpeed || m_hScrollSpeed)
                m_autoScrollTimer->start();
        }
    }

    KWebView::keyPressEvent(event);
}


void WebView::wheelEvent(QWheelEvent *event)
{
    if( event->orientation() == Qt::Vertical || !ReKonfig::hScrollWheelHistory() )
    {
        // To let some websites (eg: google maps) to handle wheel events
        int prevPos = page()->currentFrame()->scrollPosition().y();
        KWebView::wheelEvent(event);
        int newPos = page()->currentFrame()->scrollPosition().y();
        
        // Sync with the zoom slider
        if (event->modifiers() == Qt::ControlModifier)
        {
            // Limits of the slider
            if (zoomFactor() > 1.9)
                setZoomFactor(1.9);
            else if (zoomFactor() < 0.1)
                setZoomFactor(0.1);
            
            // Round the factor (Fix slider's end value)
            int newFactor = zoomFactor() * 10;
            if ((zoomFactor() * 10 - newFactor) > 0.5)
                newFactor++;
            
            emit zoomChanged(newFactor);
        }
        else if (ReKonfig::smoothScrolling() && prevPos != newPos)
        {
            
            page()->currentFrame()->setScrollPosition(QPoint(page()->currentFrame()->scrollPosition().x(), prevPos));
            
            if ((event->delta() > 0) != !m_scrollBottom)
                stopScrolling();
            
            if (event->delta() > 0)
                m_scrollBottom = false;
            else
                m_scrollBottom = true;
            
            
            setupSmoothScrolling(abs(newPos - prevPos));
        }
    }
    // use horizontal wheel events to go back and forward in tab history
    else {
        // left -> go to previous page
        if( event->delta() > 0 ){
            emit openPreviousInHistory();
        }
        // right -> go to next page
        if( event->delta() < 0 ){
            emit openNextInHistory();
        }
    }
}


void WebView::inspect()
{
    QAction *a = Application::instance()->mainWindow()->actionByName("web_inspector");
    if (a && !a->isChecked())
        a->trigger();
    pageAction(QWebPage::InspectElement)->trigger();
}


void WebView::loadUrlInNewTab(const KUrl &url)
{
    emit loadUrl(url, Rekonq::NewTab);
}


void WebView::scrollFrameChanged()
{
    // do the scrolling
    page()->currentFrame()->scroll(m_hScrollSpeed, m_vScrollSpeed);

    // check if we reached the end
    int y = page()->currentFrame()->scrollPosition().y();
    if (y == 0 || y == page()->currentFrame()->scrollBarMaximum(Qt::Vertical))
        m_vScrollSpeed = 0;

    int x = page()->currentFrame()->scrollPosition().x();
    if (x == 0 || x == page()->currentFrame()->scrollBarMaximum(Qt::Horizontal))
        m_hScrollSpeed = 0;
}


void WebView::setupSmoothScrolling(int posY)
{
    int ddy = qMax(m_smoothScrollSteps ? abs(m_dy)/m_smoothScrollSteps : 0,3);

    m_dy += posY;

    if (m_dy <= 0)
    {
        stopScrolling();
        return;
    }

    m_smoothScrollSteps = 8;

    if (m_dy / m_smoothScrollSteps < ddy)
    {
        m_smoothScrollSteps = (abs(m_dy)+ddy-1)/ddy;
        if (m_smoothScrollSteps < 1)
            m_smoothScrollSteps = 1;
    }

    m_smoothScrollTime.start();

    if (!m_smoothScrolling)
    {
        m_smoothScrolling = true;
        m_smoothScrollTimer->start();
        scrollTick();
    }
}


void WebView::scrollTick()
{
    if (m_dy == 0)
    {
        stopScrolling();
        return;
    }

    if (m_smoothScrollSteps < 1)
        m_smoothScrollSteps = 1;

    int takesteps = m_smoothScrollTime.restart() / 16;
    int scroll_y = 0;

    if (takesteps < 1)
        takesteps = 1;

    if (takesteps > m_smoothScrollSteps)
        takesteps = m_smoothScrollSteps;

    for(int i = 0; i < takesteps; i++)
    {
        int ddy = (m_dy / (m_smoothScrollSteps+1)) * 2;

        // limit step to requested scrolling distance
        if (abs(ddy) > abs(m_dy))
            ddy = m_dy;

        // update remaining scroll
        m_dy -= ddy;
        scroll_y += ddy;
        m_smoothScrollSteps--;
    }

    if (m_scrollBottom)
        page()->currentFrame()->scroll(0, scroll_y);
    else
        page()->currentFrame()->scroll(0, -scroll_y);
}


void WebView::stopScrolling()
{
    m_smoothScrollTimer->stop();
    m_dy = 0;
    m_smoothScrolling = false;
}

void WebView::changeAutoReload( QAction *action )
{
    if( !m_autoReloadTimer )
    {
        if( action->data() == 0 )
            return;
        m_autoReloadTimer = new QTimer(this);
        connect( m_autoReloadTimer, SIGNAL(timeout()), this, SLOT(reload()) );
    }
    m_autoReloadTimer->stop();
    if( action->data() != 0 )
        m_autoReloadTimer->start(action->data().toInt() * 1000);
}

