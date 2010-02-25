/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2008 Benjamin C. Meyer <ben@meyerhome.net>
* Copyright (C) 2008 Dirk Mueller <mueller@kde.org>
* Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
* Copyright (C) 2008 Michael Howell <mhowell123@gmail.com>
* Copyright (C) 2008-2010 by Andrea Diamantini <adjam7 at gmail dot com>
* Copyright (C) 2010 by Matthieu Gicquel <matgic78 at gmail dot com>
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
#include "webpage.h"
#include "webpage.moc"

// Auto Includes
#include "rekonq.h"

// Local Includes
#include "application.h"
#include "mainwindow.h"
#include "mainview.h"
#include "webtab.h"
#include "webpluginfactory.h"
#include "networkaccessmanager.h"
#include "adblockmanager.h"

// KDE Includes
#include <KStandardDirs>
#include <KUrl>
#include <KDebug>
#include <KToolInvocation>
#include <KProtocolManager>
#include <kwebwallet.h>

#include <kparts/browseropenorsavequestion.h>

#include <kio/renamedialog.h>

#include <KDE/KMimeTypeTrader>
#include <KDE/KRun>
#include <KDE/KFileDialog>
#include <KDE/KMessageBox>
#include <KDE/KJobUiDelegate>

// Qt Includes
#include <QtGui/QContextMenuEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QClipboard>
#include <QtGui/QKeyEvent>


WebPage::WebPage(QWidget *parent)
    : KWebPage(parent, KWalletIntegration)
{
    // ----- handling unsupported content...
    setForwardUnsupportedContent(true);
    connect(this, SIGNAL(unsupportedContent(QNetworkReply *)), this, SLOT(handleUnsupportedContent(QNetworkReply *)));

    // ----- rekonq Network Manager
    NetworkAccessManager *manager = new NetworkAccessManager(this);
    manager->setCache(0);   // disable QtWebKit cache to just use KIO one..
    
    // set cookieJar window ID..
    if (parent && parent->window())
        manager->setCookieJarWindowId(parent->window()->winId());

    setNetworkAccessManager(manager);
    
    // ----- Web Plugin Factory
    setPluginFactory(new WebPluginFactory(this));
    
    // ----- last stuffs
    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(manageNetworkErrors(QNetworkReply*)));
    connect(this, SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));

    // protocol handler signals
    connect(&m_protHandler, SIGNAL(downloadUrl(const KUrl &)), this, SLOT(downloadUrl(const KUrl &)));
}


WebPage::~WebPage()
{
    disconnect();
}


bool WebPage::acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, NavigationType type)
{
    // advise users on resubmitting data
    if(type == QWebPage::NavigationTypeFormResubmitted)
    {
        int risp = KMessageBox::warningContinueCancel(view(), 
                                                      i18n("Are you sure you want to send your data again?"), 
                                                      i18n("Resend form data") );
        if(risp == KMessageBox::Cancel)
            return false;
    }

    if ( frame && m_protHandler.preHandling(request, frame) )
    {
        return false;
    }

    return KWebPage::acceptNavigationRequest(frame, request, type);
}


WebPage *WebPage::createWindow(QWebPage::WebWindowType type)
{
    kDebug() << "WebPage createWindow slot";

    // added to manage web modal dialogs
    if (type == QWebPage::WebModalDialog)
        kDebug() << "Modal Dialog";

    WebTab *w = 0;
    if(ReKonfig::openTabNoWindow())
    {
        w = Application::instance()->mainWindow()->mainView()->newWebTab(!ReKonfig::openTabsBack(), ReKonfig::openTabsNearCurrent());
    }
    else
    {
        w = Application::instance()->newMainWindow()->mainView()->currentWebTab();
    }
    return w->page();
}


void WebPage::handleUnsupportedContent(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError)
    {
        const KUrl url( reply->url() );

        QString mimeType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        KService::Ptr offer = KMimeTypeTrader::self()->preferredService(mimeType);

        bool isLocal = url.isLocalFile();
        
        if( offer.isNull() ) // no service can handle this. We can just download it..
        {
            kDebug() << "no service can handle this. We can just download it..";
            
            isLocal 
                ? KMessageBox::sorry(view(), i18n("No service can handle this :(") ) 
                : downloadRequest( reply->request() );
            return;
        }

        if(!isLocal)
        {
        
            KParts::BrowserOpenOrSaveQuestion dlg(Application::instance()->mainWindow(), url, mimeType);
            switch ( dlg.askEmbedOrSave() )
            {
                case KParts::BrowserOpenOrSaveQuestion::Save:
                    kDebug() << "service handling: download!";
                    downloadRequest( reply->request() );
                    return;
                case KParts::BrowserOpenOrSaveQuestion::Cancel:
                    return;
                default: // non extant case
                    break;
            }
        }
        // case KParts::BrowserRun::Embed
        KUrl::List list;
        list.append(url);
        KRun::run(*offer,url,0);
    }
}


void WebPage::loadFinished(bool)
{
    Application::adblockManager()->applyHidingRules(this);
    
    // KWallet Integration
    // TODO: Add check for sites exempt from automatic form filling...
    if (wallet()) 
    {
        wallet()->fillFormData(mainFrame());
    }
}


void WebPage::manageNetworkErrors(QNetworkReply *reply)
{
    WebView *v = 0;
    
    // NOTE 
    // These are not all networkreply errors, 
    // but just that supported directly by KIO
    
    switch( reply->error() )
    {
        
    case QNetworkReply::NoError:                            // no error. Simple :)
        break;

    case QNetworkReply::UnknownNetworkError:                 // unknown network-related error detected

        if( m_protHandler.postHandling(reply->request(), mainFrame()) )
            break;

    case QNetworkReply::ContentAccessDenied:                 // access to remote content denied (similar to HTTP error 401)
        kDebug() << "We (hopefully) are managing this through the adblock :)";
        break;
        
    case QNetworkReply::ConnectionRefusedError:              // remote server refused connection
    case QNetworkReply::HostNotFoundError:                   // invalid hostname
    case QNetworkReply::TimeoutError:                        // connection time out
    case QNetworkReply::OperationCanceledError:              // operation canceled via abort() or close() calls
    case QNetworkReply::ProxyNotFoundError:                  // invalid proxy hostname
    case QNetworkReply::ContentOperationNotPermittedError:   // operation requested on remote content not permitted
    case QNetworkReply::ContentNotFoundError:                // remote content not found on server (similar to HTTP error 404)
    case QNetworkReply::ProtocolUnknownError:                // Unknown protocol
    case QNetworkReply::ProtocolInvalidOperationError:       // requested operation is invalid for this protocol

        // don't bother on elements loading errors: 
        // we'll manage just main url page ones
        v = qobject_cast<WebView *>(view());
        if( reply->url() != v->url() )
            break;
        
        mainFrame()->setHtml( errorPage(reply), reply->url() );
        break;

    default:
        kDebug() << "Nothing to do here..";
        break;

    }
}


QString WebPage::errorPage(QNetworkReply *reply)
{
    // display "not found" page
    QString notfoundFilePath =  KStandardDirs::locate("data", "rekonq/htmls/rekonqinfo.html");
    QFile file(notfoundFilePath);

    bool isOpened = file.open(QIODevice::ReadOnly);
    if (!isOpened)
    {
        return QString("Couldn't open the rekonqinfo.html file");
    }

    QString title = i18n("Error loading: %1", reply->url().path()); 
    QString msg = "<h1>" + reply->errorString() + "</h1>";
    QString urlString = reply->url().toString( QUrl::RemoveUserInfo | QUrl::RemoveQuery );
    
    msg += "<h2>" + i18nc("%1=an URL, e.g.'kde.org'", "When connecting to: %1", urlString ) + "</h2>";
    msg += "<ul><li>" + i18n("Check the address for errors such as <b>ww</b>.kde.org instead of <b>www</b>.kde.org");
    msg += "</li><li>" + i18n("If the address is correct, try to check the network connection.") + "</li><li>" ;
    msg += i18n("If your computer or network is protected by a firewall or proxy, make sure that rekonq is permitted to access the network.");
    msg += "</li><li>" + i18n("Of course, if rekonq does not work properly, you can always say it is a programmer error ;)");
    msg += "</li></ul><br/><br/>";
    msg += "<input type=\"button\" id=\"reloadButton\" onClick=\"document.location.href='" + urlString + "';\" value=\"";
    msg += i18n("Try Again") + "\" />";
    
    QString html = QString(QLatin1String(file.readAll()))
                            .arg(title)
                            .arg(msg)
                            ;
    return html;
}


void WebPage::downloadRequest(const QNetworkRequest &request)
{
    if (ReKonfig::kgetDownload())
    {
        //*Copy of kwebpage code (Shouldn't be done in kwebpage ?)

        KUrl destUrl;
        KUrl srcUrl (request.url());
        int result = KIO::R_OVERWRITE;

        do 
        {
            destUrl = KFileDialog::getSaveFileName(srcUrl.fileName(), QString(), view());

            if (destUrl.isLocalFile()) 
            {
                QFileInfo finfo (destUrl.toLocalFile());
                if (finfo.exists()) 
                {
                    QDateTime now = QDateTime::currentDateTime();
                    KIO::RenameDialog dlg (view(), i18n("Overwrite File?"), srcUrl, destUrl,
                                        KIO::RenameDialog_Mode(KIO::M_OVERWRITE | KIO::M_SKIP),
                                        -1, finfo.size(),
                                        now.toTime_t(), finfo.created().toTime_t(),
                                        now.toTime_t(), finfo.lastModified().toTime_t());
                    result = dlg.exec();
                }
            }
        } 
        while (result == KIO::R_CANCEL && destUrl.isValid());

        if (result == KIO::R_OVERWRITE && destUrl.isValid()) 
        {
            //*End of copy code
            
            //KGet integration:
            if(!QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.kget"))
            {
                KToolInvocation::kdeinitExecWait("kget");
            }
            QDBusInterface kget("org.kde.kget", "/KGet", "org.kde.kget.main");
            kget.call("addTransfer", srcUrl.prettyUrl(), destUrl.prettyUrl(), true);
        }
        
        return;
    }
    
    KWebPage::downloadRequest(request);
}


void WebPage::downloadAllContentsWithKGet()
{
    QSet<QString> contents;
    KUrl baseUrl( currentFrame()->url() );
    KUrl relativeUrl;

    QWebElementCollection images = mainFrame()->documentElement().findAll("img");
    foreach(QWebElement img, images)
    {
	relativeUrl.setEncodedUrl(img.attribute("src").toUtf8(),KUrl::TolerantMode); 
	contents << baseUrl.resolved(relativeUrl).toString();
    }
    
    QWebElementCollection links = mainFrame()->documentElement().findAll("a");
    foreach(QWebElement link, links)
    {
	relativeUrl.setEncodedUrl(link.attribute("href").toUtf8(),KUrl::TolerantMode); 
	contents << baseUrl.resolved(relativeUrl).toString();
    }
    
    if(!QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.kget"))
    {
        KToolInvocation::kdeinitExecWait("kget");
    }
    QDBusInterface kget("org.kde.kget", "/KGet", "org.kde.kget.main");
    kget.call("importLinks", QVariant(contents.toList()));
}
