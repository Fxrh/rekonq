/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2008-2009 by Andrea Diamantini <adjam7 at gmail dot com>
* Copyright (C) 2009 by Paweł Prażak <pawelprazak at gmail dot com>
* Copyright (C) 2009 by Lionel Chauvin <megabigbug@yahoo.fr>
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


#ifndef TABWIDGET_H
#define TABWIDGET_H

// Local Includes
#include "webview.h"
#include "webpage.h"
#include "application.h"

// KDE Includes
#include <KTabWidget>

// Qt Includes
#include <QtGui/QToolButton>

// Forward Declarations
class QUrl;
class QWebFrame;
class QLabel;


class StackedUrlBar;
class TabBar;
class UrlBar;


/**
 *  This class represent rekonq Main View. It contains all WebViews and a stack widget
 *  of associated line edits.
 *
 */

class MainView : public KTabWidget
{
    Q_OBJECT

public:
    MainView(QWidget *parent = 0);
    ~MainView();

public:

    UrlBar *urlBar(int index) const;
    UrlBar *currentUrlBar() const;
    WebView *webView(int index) const;
    QToolButton *addTabButton() const;

    // inlines
    TabBar *tabBar() const;
    StackedUrlBar *urlBarStack() const;
    WebView *currentWebView() const;
    int webViewIndex(WebView *webView) const;

    /**
     * show and hide TabBar if user doesn't choose
     * "Always Show TabBar" option
     *
     */
    void showTabBar();
    void clear();


signals:
    // tab widget signals
    void tabsChanged();
    void lastTabClosed();

    // current tab signals
    void setCurrentTitle(const QString &url);
    void showStatusBarMessage(const QString &message, Rekonq::Notify status = Rekonq::Info);
    void linkHovered(const QString &link);
    void browserLoading(bool);

    void printRequested(QWebFrame *frame);

public slots:
    /**
     * Core browser slot. This create a new tab with a WebView inside
     * for browsing.
     *
     * @return a pointer to the new WebView
     */
    WebView *newTab(bool focused = true);

    void slotCloneTab(int index = -1);
    void slotCloseTab(int index = -1);
    void slotCloseOtherTabs(int index);
    void slotReloadTab(int index = -1);
    void slotReloadAllTabs();
    void nextTab();
    void previousTab();

    // WEB slot actions
    void slotWebReload();
    void slotWebStop();
    void slotWebBack();
    void slotWebForward();
    void slotWebUndo();
    void slotWebRedo();
    void slotWebCut();
    void slotWebCopy();
    void slotWebPaste();

private slots:
    void slotCurrentChanged(int index);

    void webViewLoadStarted();
    void webViewLoadFinished(bool ok);
    void webViewIconChanged();
    void webViewTitleChanged(const QString &title);
    void webViewUrlChanged(const QUrl &url);

    void windowCloseRequested();

    /**
     * This functions move tab info "from index to index"
     *
     * @param fromIndex the index from which we move
     *
     * @param toIndex the index to which we move
     */
    void moveTab(int fromIndex, int toIndex);

    void postLaunch();

protected:

    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void resizeEvent(QResizeEvent *event);

private:

    void addTabButtonPosition();

    /**
     * This function creates (if not exists) and returns a QLabel
     * with a loading QMovie.
     * Imported from Arora's code.
     *
     * @param index the tab index where inserting the animated label
     * @param addMovie creates or not a loading movie
     *
     * @return animated label's pointer
     */
    QLabel *animatedLoading(int index, bool addMovie);

    StackedUrlBar *m_urlBars;
    TabBar *m_tabBar;

    QString m_loadingGitPath;

    QToolButton *m_addTabButton;
};

#endif
