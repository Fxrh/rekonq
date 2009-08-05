/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2007-2008 Trolltech ASA. All rights reserved
* Copyright (C) 2008-2009 by Andrea Diamantini <adjam7 at gmail dot com>*
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


#ifndef MODELMENU_H
#define MODELMENU_H


// KDE Includes
#include <KIcon>
#include <KAction>
#include <KMenu>

// Qt Includes
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

// Forward Declarations
class QString;
class QWidget;
class QObject;
class QPersistentModelIndex;

class KIcon;

/** 
 * A KMenu that is dynamically populated 
 * from a QAbstractItemModel
 *
 */

class ModelMenu : public KMenu
{
    Q_OBJECT

signals:
    void activated(const QModelIndex &index);
    void hovered(const QString &text);

public:
    ModelMenu(QWidget *parent = 0);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    void setMaxRows(int max);
    int maxRows() const;

    void setFirstSeparator(int offset);
    int firstSeparator() const;

    void setRootIndex(const QModelIndex &index);
    QModelIndex rootIndex() const;

    void setHoverRole(int role);
    int hoverRole() const;

    void setSeparatorRole(int role);
    int separatorRole() const;

    KAction *makeAction(const KIcon &icon, const QString &text, QObject *parent);

protected:
    // add any actions before the tree, return true if any actions are added.
    virtual bool prePopulated();
    // add any actions after the tree
    virtual void postPopulated();
    // put all of the children of parent into menu up to max
    void createMenu(const QModelIndex &parent, int max, QMenu *parentMenu = 0, QMenu *menu = 0);

private slots:
    void aboutToShow();
    void triggered(QAction *action);
    void hovered(QAction *action);

private:
    KAction *makeAction(const QModelIndex &index);
    int m_maxRows;
    int m_firstSeparator;
    int m_maxWidth;
    int m_hoverRole;
    int m_separatorRole;
    QAbstractItemModel *m_model;
    QPersistentModelIndex m_root;
};

#endif // MODELMENU_H
