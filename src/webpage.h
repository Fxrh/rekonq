/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2008 Benjamin C. Meyer <ben@meyerhome.net>
* Copyright (C) 2008 Dirk Mueller <mueller@kde.org>
* Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
* Copyright (C) 2008 Michael Howell <mhowell123@gmail.com>
* Copyright (C) 2008-2009 by Andrea Diamantini <adjam7 at gmail dot com>
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


#ifndef WEBPAGE_H
#define WEBPAGE_H


// KDE Includes

// Qt Includes
#include <QWebPage>
#include <QUrl>

// Forward Declarations
class QWebFrame;
class QNetworkReply;
class QUrl;

class WebPage : public QWebPage
{
    Q_OBJECT

public:
    explicit WebPage(QObject *parent = 0);
    ~WebPage();
    
public slots:
    void manageNetworkErrors(QNetworkReply* reply);

protected:
    WebPage *createWindow(WebWindowType type);
    virtual WebPage *newWindow(WebWindowType type);
    
    virtual bool acceptNavigationRequest(QWebFrame *frame, 
                                         const QNetworkRequest &request, 
                                         NavigationType type);
    
    void javaScriptAlert(QWebFrame *frame, 
                         const QString &msg);
                         
    bool javaScriptConfirm(QWebFrame *frame, 
                           const QString &msg);
                           
    bool javaScriptPrompt(QWebFrame *frame, 
                          const QString &msg, 
                          const QString &defaultValue, QString *result);
    
    QObject *createPlugin(const QString &classId, 
                          const QUrl &url, 
                          const QStringList &paramNames, 
                          const QStringList &paramValues);
                          
protected Q_SLOTS:    
    virtual void slotHandleUnsupportedContent(QNetworkReply *reply);
    virtual void slotDownloadRequested(const QNetworkRequest &request);
    
private:
    friend class WebView;
    QString errorPage(QNetworkReply *);

    // keyboard/mouse modifiers
    Qt::KeyboardModifiers m_keyboardModifiers;
    Qt::MouseButtons m_pressedButtons;

    QUrl m_requestedUrl;
};

#endif
