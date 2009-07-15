/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2008 Benjamin C. Meyer <ben@meyerhome.net>
* Copyright (C) 2009 by Andrea Diamantini <adjam7 at gmail dot com>
*
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General
* Public License as published by the Free Software Foundation;
* either version 3, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* ============================================================ */


// Self Includes
#include "webpage.h"
#include "webpage.moc"

// Auto Includes
#include "rekonq.h"

// Local Includes
#include "application.h"
#include "mainwindow.h"
#include "mainview.h"
#include "cookiejar.h"
#include "networkaccessmanager.h"
#include "history.h"
#include "webview.h"

// KDE Includes
#include <KStandardDirs>
#include <KUrl>
#include <KActionCollection>
#include <KDebug>


#include <KDE/KParts/BrowserRun>
#include <KDE/KMimeTypeTrader>
#include <KDE/KRun>
#include <KDE/KFileDialog>
#include <KDE/KInputDialog>
#include <KDE/KMessageBox>
#include <KDE/KJobUiDelegate>

// Qt Includes
#include <QtGui/QContextMenuEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QClipboard>
#include <QtGui/QKeyEvent>

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtWebKit/QWebFrame>
#include <QtWebKit/QWebHitTestResult>
#include <QtWebKit/QWebPage>
#include <QtWebKit/QWebSettings>
#include <QtWebKit/QWebView>

#include <QUiLoader>

WebPage::WebPage(QObject *parent)
        : QWebPage(parent)
{
    setForwardUnsupportedContent(true);

    setNetworkAccessManager(Application::networkAccessManager());
    connect(networkAccessManager(), SIGNAL(finished(QNetworkReply*)), this, SLOT(manageNetworkErrors(QNetworkReply*)));
}


QWebPage *WebPage::createWindow(QWebPage::WebWindowType type)
{
    kDebug() << "creating window as new tab.. ";

    // added to manage web modal dialogs
    if (type == QWebPage::WebModalDialog)
    {
        // FIXME : need a "real" implementation..
        kDebug() << "Modal Dialog ---------------------------------------";
    }

    WebView *w = Application::instance()->newWebView();
    return w->page();
}


// FIXME: implement here (perhaps) mimetype discerning && file loading (KToolInvocation??)
void WebPage::slotHandleUnsupportedContent(QNetworkReply *reply)
{

    const KUrl url(reply->request().url());
    kDebug() << "title:" << url;
    kDebug() << "error:" << reply->errorString();

    QString filename = url.fileName();
    QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    KService::Ptr offer = KMimeTypeTrader::self()->preferredService(mimetype);

    KParts::BrowserRun::AskSaveResult res = KParts::BrowserRun::askSave(
                                                url,
                                                offer,
                                                mimetype,
                                                filename
                                            );
    switch (res) 
    {
    case KParts::BrowserRun::Save:
        slotDownloadRequested(reply->request());
        return;
    case KParts::BrowserRun::Cancel:
        return;
    default: // non existant case
        break;
    }

    KUrl::List list;
    list.append(url);
    KRun::run(*offer,url,0);
    return;
}


void WebPage::manageNetworkErrors(QNetworkReply* reply)
{
    if(reply->error() == QNetworkReply::NoError)
        return;

    viewErrorPage(reply);
}


void WebPage::viewErrorPage(QNetworkReply *reply)
{
    // display "not found" page
    QString notfoundFilePath =  KStandardDirs::locate("data", "rekonq/htmls/notfound.html");
    QFile file(notfoundFilePath);
    bool isOpened = file.open(QIODevice::ReadOnly);
    if (!isOpened)
    {
        kWarning() << "Couldn't open the notfound.html file";
        return;
    }
    QString title = i18n("Error loading page: ") + reply->url().toString();

    QString imagePath = KIconLoader::global()->iconPath("rekonq", KIconLoader::NoGroup, false);

    QString html = QString(QLatin1String(file.readAll()))
                   .arg(title)
                   .arg("file://" + imagePath)
                   .arg(reply->errorString())
                   .arg(reply->url().toString());

    // test
    QList<QWebFrame*> frames;
    frames.append(mainFrame());
    while (!frames.isEmpty())
    {
        QWebFrame *firstFrame = frames.takeFirst();
        if (firstFrame->url() == reply->url())
        {
            firstFrame->setHtml(html, reply->url());
            return;
        }
        QList<QWebFrame *> children = firstFrame->childFrames();
        foreach(QWebFrame *frame, children)
        {
            frames.append(frame);
        }
    }
//     if (m_loadingUrl == reply->url())
//     {
//         mainFrame()->setHtml(html, reply->url());
//         // Don't put error pages to the history.
//         Application::historyManager()->removeHistoryEntry(reply->url(), mainFrame()->title());
//     }
}


void WebPage::javaScriptAlert(QWebFrame *frame, const QString &msg)
{
    KMessageBox::error(frame->page()->view(), msg, i18n("JavaScript"));
}


bool WebPage::javaScriptConfirm(QWebFrame *frame, const QString &msg)
{
    return (KMessageBox::warningYesNo(frame->page()->view(), msg, i18n("JavaScript"), KStandardGuiItem::ok(), KStandardGuiItem::cancel())
            == KMessageBox::Yes);
}


bool WebPage::javaScriptPrompt(QWebFrame *frame, const QString &msg, const QString &defaultValue, QString *result)
{
    bool ok = false;
    *result = KInputDialog::getText(i18n("JavaScript"), msg, defaultValue, &ok, frame->page()->view());
    return ok;
}


QObject *WebPage::createPlugin(const QString &classId, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues)
{
    kDebug() << "create Plugin requested:";
    kDebug() << "classid:" << classId;
    kDebug() << "url:" << url;
    kDebug() << "paramNames:" << paramNames << " paramValues:" << paramValues;

    QUiLoader loader;
    return loader.createWidget(classId, view());
}


void WebPage::slotDownloadRequested(const QNetworkRequest &request)
{
    const KUrl url(request.url());
    kDebug() << url;

//     const QString fileName = d->getFileNameForDownload(request, reply);
// 
//     // parts of following code are based on khtml_ext.cpp
//     // DownloadManager <-> konqueror integration
//     // find if the integration is enabled
//     // the empty key  means no integration
//     // only use download manager for non-local urls!
//     bool downloadViaKIO = true;
//     if (!url.isLocalFile()) {
//         KConfigGroup cfg = KSharedConfig::openConfig("konquerorrc", KConfig::NoGlobals)->group("HTML Settings");
//         const QString downloadManger = cfg.readPathEntry("DownloadManager", QString());
//         if (!downloadManger.isEmpty()) {
//             // then find the download manager location
//             kDebug() << "Using: " << downloadManger << " as Download Manager";
//             QString cmd = KStandardDirs::findExe(downloadManger);
//             if (cmd.isEmpty()) {
//                 QString errMsg = i18n("The Download Manager (%1) could not be found in your $PATH.", downloadManger);
//                 QString errMsgEx = i18n("Try to reinstall it. \n\nThe integration with Konqueror will be disabled.");
//                 KMessageBox::detailedSorry(view(), errMsg, errMsgEx);
//                 cfg.writePathEntry("DownloadManager", QString());
//                 cfg.sync();
//             } else {
//                 downloadViaKIO = false;
//                 cmd += ' ' + KShell::quoteArg(url.url());
//                 kDebug() << "Calling command" << cmd;
//                 KRun::runCommand(cmd, view());
//             }
//         }
//     }
// 
//     if (downloadViaKIO) {
        const QString destUrl = KFileDialog::getSaveFileName(url.fileName(), QString(), view());
        if (destUrl.isEmpty()) return;
        KIO::Job *job = KIO::file_copy(url, KUrl(destUrl), -1, KIO::Overwrite);
        //job->setMetaData(metadata); //TODO: add metadata from request
        job->addMetaData("MaxCacheSize", "0"); // Don't store in http cache.
        job->addMetaData("cache", "cache"); // Use entry from cache if available.
        job->uiDelegate()->setAutoErrorHandlingEnabled(true);
//     }


}


QString WebPage::chooseFile(QWebFrame *frame, const QString &suggestedFile)
{
    return KFileDialog::getOpenFileName(suggestedFile, QString(), frame->page()->view());
}


WebPage *WebPage::newWindow(WebWindowType type)
{
    Q_UNUSED(type);
    return 0;
}

