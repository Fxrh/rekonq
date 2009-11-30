/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2009 by Andrea Diamantini <adjam7 at gmail dot com>
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



#ifndef ADBLOCK_MANAGER_H
#define ADBLOCK_MANAGER_H


// Local Includes
#include "khtml_filter_p.h"

// Qt Includes
#include <QObject>
#include <QStringList>
#include <QNetworkReply>

// Forward Includes
class QNetworkRequest;


class AdBlockManager : public QObject
{
Q_OBJECT
    
public:
    AdBlockManager(QObject *parent = 0);
    ~AdBlockManager();

    void loadSettings();
    QNetworkReply *block(const QNetworkRequest &request);
    
private:
    bool _isAdblockEnabled;
    bool _isHideAdsEnabled;

    khtml::FilterSet _adBlackList;
    khtml::FilterSet _adWhiteList;
};

#endif
