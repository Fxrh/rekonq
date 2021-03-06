/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2007-2008 Trolltech ASA. All rights reserved
* Copyright (C) 2008-2010 by Andrea Diamantini <adjam7 at gmail dot com>
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
#include "networkaccessmanager.h"
#include "networkaccessmanager.moc"

// Local Includes
#include "adblockmanager.h"
#include "application.h"
#include "webpage.h"

// KDE Includes
#include <KLocale>
#include <KProtocolManager>


NetworkAccessManager::NetworkAccessManager(QObject *parent)
        : AccessManager(parent)
{
    QString c = KGlobal::locale()->country();
    if (c == QL1S("C"))
        c = QL1S("en_US");
    if (c != QL1S("en_US"))
        c.append(QL1S(", en_US"));

    _acceptLanguage = c.toLatin1();
}


QNetworkReply *NetworkAccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    WebPage *parentPage = qobject_cast<WebPage *>(parent());

    QNetworkReply *reply = 0;

    QNetworkRequest req = request;
    req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    req.setRawHeader("Accept-Language", _acceptLanguage);

    KIO::CacheControl cc = KProtocolManager::cacheControl();
    switch (cc)
    {
    case KIO::CC_CacheOnly:      // Fail request if not in cache.
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysCache);
        break;

    case KIO::CC_Refresh:        // Always validate cached entry with remote site.
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
        break;

    case KIO::CC_Reload:         // Always fetch from remote site
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        break;

    case KIO::CC_Cache:          // Use cached entry if available.
    case KIO::CC_Verify:         // Validate cached entry with remote site if expired.
    default:
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        break;
    }

    // WARNING
    // There are actually 2 exceptions here handled with QNAM
    // instead of KIO that need fixes upstream before removing. They are:
    // 1) DeleteOperation
    // 2) CustomOperation

    switch(op)
    {
    case QNetworkAccessManager::HeadOperation:
        break;

    case QNetworkAccessManager::GetOperation:
        reply = Application::adblockManager()->block(req, parentPage);
        break;

    case QNetworkAccessManager::PutOperation:
        break;

    case QNetworkAccessManager::PostOperation:
        break;

    case QNetworkAccessManager::DeleteOperation:
        kDebug() << "DELETE OPERATION...";
        reply = QNetworkAccessManager::createRequest(op, req, outgoingData);
        if(!reply)
            kDebug() << "OOOOOOOOOOOOOOOOOOO DELETE REPLY NULL";
        break;

    case QNetworkAccessManager::CustomOperation:
        kDebug() << "CUSTOM OPERATION...";
        reply = QNetworkAccessManager::createRequest(op, req, outgoingData);
        if(!reply)
            kDebug() << "OOOOOOOOOOOOOOOOOOO CUSTOM REPLY NULL";
        break;

    default:
        kDebug() << "NON EXTANT CASE...";
        break;
    }

    if(!reply)
        reply = AccessManager::createRequest(op, req, outgoingData);

    if(parentPage && parentPage->hasNetworkAnalyzerEnabled())
        emit networkData( op, req, reply );

    return reply;
}
