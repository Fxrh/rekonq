/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2007-2008 Trolltech ASA. All rights reserved
* Copyright (C) 2008 Benjamin C. Meyer <ben@meyerhome.net>
* Copyright (C) 2008-2009 by Andrea Diamantini <adjam7 at gmail dot com>
* Copyright (C) 2009 by Paweł Prażak <pawelprazak at gmail dot com>
*
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General
* Public License as published by the Free Software Foundation;
* either version 2, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* ============================================================ */


#ifndef WEBPAGE_H
#define WEBPAGE_H

// KDE Includes
#include <KUrl>

#include <kdewebkit/kwebpage.h>
#include <kdewebkit/kwebview.h>

// Qt Includes
#include <QWebPage>

// Forward Declarations
class MainWindow;
class Application;

class KActionCollection;

class QWebFrame;
class QAuthenticator;
class QMouseEvent;
class QNetworkProxy;
class QNetworkReply;
class QSslError;


class WebPage : public KWebPage
{
    Q_OBJECT

signals:
    void loadingUrl(const QUrl &url);   // WARNING has to be QUrl!!

public:
    WebPage(QObject *parent = 0);


protected:
    bool acceptNavigationRequest(QWebFrame *frame,
                                 const QNetworkRequest &request,
                                 NavigationType type);

    KWebPage *createWindow(QWebPage::WebWindowType type);

private slots:
    void handleUnsupportedContent(QNetworkReply *reply);

private:
    friend class WebView;

    // set the webview mousepressedevent
    Qt::KeyboardModifiers m_keyboardModifiers;
    Qt::MouseButtons m_pressedButtons;
    KUrl m_loadingUrl;
};

#endif
