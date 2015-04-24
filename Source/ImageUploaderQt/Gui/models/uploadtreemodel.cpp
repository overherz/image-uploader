/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

/*
	 UploadTreeModel.cpp

    Provides a simple tree model to show how to create and use hierarchical
    models.
*/

#include <QtGui>
#include "uploadtreemodel.h"
#include <QStringList>

UploadTreeModel::UploadTreeModel(QObject *parent,UploadManager *uploadManager)
    : QAbstractItemModel(parent)
{
	setupModelData(uploadManager);
}

UploadTreeModel::~UploadTreeModel()
{

}

int UploadTreeModel::columnCount(const QModelIndex &parent) const
{
	return 4;
}

QVariant UploadTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

      InternalItem * obj = reinterpret_cast<InternalItem*>(index.internalPointer());

    if(role == Qt::DecorationRole && index.column()==0 && obj->task)
	{
        QIcon ico;
        if(obj->task->status()== UploadTask::StatusFinished)
			ico = QIcon(":/icons/res/transfer_success.png");
        else if(obj->task->status() ==  UploadTask::StatusInQueue)
			 ico = QIcon(":/icons/res/transfer_inactive.png");
        else if(obj->task->status() == UploadTask::StatusRunning)
			 ico = QIcon(":/icons/res/transfer.png");
		else return QVariant();
		return ico;
	 }
	 if (role != Qt::DisplayRole)
        return QVariant();




	if(!obj) return QVariant();

	QString result;
	if(index.column() == 0)
	{
        result = U2Q(obj->task->title());
	}
	else if (index.column() == 1)
	{
        result = U2Q(obj->task->progress()->statusText);
	}
	else if (index.column() == 2)
	{
/*		ZInfoProgress *progress = obj->uploadProgress();
		if(progress->Uploaded && progress->Total)
		{

			QString speedText = QString("%1 of %2").arg(progress->Uploaded).arg(progress->Total);
			if(progress->Total)
			{
				QString perc;
				perc = QString::number((long)(100*(double)progress->Uploaded / progress->Total)) + "%";
				speedText += "(" + perc + ")";
			}
			if( progress->speed)
			{
				speedText += " ["+ QString::number((int)progress->speed) + " bytes/sec]";
			}
			result =speedText;
        }*/
	}
	else if (index.column() == 3)
	{
        result = U2Q(obj->task->serverName());
	}
	return result;
}

Qt::ItemFlags UploadTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant UploadTreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
	QStringList headerLabels ;
	headerLabels <<tr("Fil1e")<<tr("Status")<<tr("Progress")<<tr("Server");

    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		  return headerLabels[section];
	  }

    return QVariant();
}

QModelIndex UploadTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    InternalItem * internalItem = 0;

    if (!parent.isValid()) {
        UploadSession* session = m_uploadManager->session(row).get();
        auto it = sessionsMap_.find(session);
        if ( it == sessionsMap_.end() ) {
            return QModelIndex();
        }
        internalItem = it->second;
    }
	else
	{
        InternalItem * internalParentItem = reinterpret_cast<InternalItem*> (parent.internalPointer());
        if ( internalParentItem->task ) {
            UploadTask* task = internalParentItem->task->child(row).get();
            auto it = m_objMap.find(task);
            if ( it == m_objMap.end() ) {
                return QModelIndex();
            }
            internalItem = it->second;

        } else {
              auto& it = sessionsMap_.find(internalParentItem->session.get());
              if ( it == sessionsMap_.end() ) {
                  return QModelIndex();
              }
              internalItem = it->second;
        }

	}

	/*TreeItem *parentItem = getItem(parent);

		 TreeItem *childItem = parentItem->child(row);
		 if (childItem)
			  return createIndex(row, column, childItem);
		 else
			  return QModelIndex();*/


    return createIndex(row, column, internalItem);
}

QModelIndex UploadTreeModel::parent(const QModelIndex &index) const
{
	 if (!index.isValid())
        return QModelIndex();

    if ( index.column() == 0 ) {
       InternalItem * internalParentItem = reinterpret_cast<InternalItem*>(index.internalPointer());

       if ( internalParentItem->task ) {
           UploadTask* parent = internalParentItem->task->parentTask();
           if(!parent) {
               UploadSession* session = internalParentItem->task->session();
               auto it = sessionsMap_.find(session);
               if ( it == sessionsMap_.end() ) {
                   return QModelIndex();
               }
               return createIndex( it->second->index, 0, it->second);
           }
           UploadTask* parentTask = internalParentItem->task->parentTask();
           auto it = m_objMap.find(parentTask);
           if ( it == m_objMap.end() ) {
               return QModelIndex();
           }
           return createIndex( it->second->index, 0, it->second);
       } else {
           //UploadSession* has no parent
            return QModelIndex();
        }
      // return createIndex(upload->childCount(), 0, parent);
    }
    return QModelIndex();

}

int UploadTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
	{
         if ( parent.column() != 0) {
             return 0;
         }
         InternalItem * internalParentItem = reinterpret_cast<InternalItem*>(parent.internalPointer());

         if ( internalParentItem->task ) {
            return internalParentItem->task->childCount();
         } else {
              return internalParentItem->session->taskCount();
         }
		 //return 0;
	  }

     return  m_uploadManager->sessionCount();
}

void UploadTreeModel::setupModelData(UploadManager *uploadManager)
{
	m_uploadManager = uploadManager;
    m_uploadManager->OnTaskAdded.bind(this, &UploadTreeModel::data_OnChildAdded);
    m_uploadManager->OnTaskAdded.bind(this,  &UploadTreeModel::data_OnUploadProgress);
    //m_uploadManager->OnStatusChanged.bind(this, &UploadTreeModel::data_OnStatusChanged);

    /*connect(uploadManager, SIGNAL(OnChildAdded(UploadTask*,UploadTask*)), this, SLOT(data_OnChildAdded(UploadTask*,UploadTask*))/, Qt::BlockingQueuedConnection*);
	connect(uploadManager, SIGNAL(OnUploadProgress(UploadTask*,InfoProgress)), this, SLOT(data_OnUploadProgress(UploadTask*,InfoProgress)));
	connect(uploadManager, SIGNAL(OnStatusChanged(UploadTask*,QString)), this, SLOT(data_OnStatusChanged(UploadTask*,QString)));
    */
//emit dataChanged();
}

void UploadTreeModel::data_OnChildAdded(UploadTask* child)
{
    recalcObjMap();
    if(!child->parentTask())
	{
		insertRow(QModelIndex());

	}
	else
	{
			//insertRow(QModelIndex());
        UploadTask * parent = child->parentTask();
        insertRow(createIndex(byUploadTask(parent), 0, parent));
	}
}

 void UploadTreeModel::data_OnUploadProgress(UploadTask* it)
 {
    int row = byUploadTask(it);
	emit dataChanged(createIndex(row,1,it), createIndex(row,3,it));
 }

 int UploadTreeModel::byUploadTask(UploadTask* obj) const
 {
     auto it = m_objMap.find(obj);
     if ( it != m_objMap.end() ) {
         return it->second->index;
     }
    return -1;
 }

 int UploadTreeModel::byUploadSession(UploadSession *obj) const
 {
     for(int i=0; i<m_uploadManager->sessionCount(); i++)
     {
         if(m_uploadManager->session(i).get()==obj)
             return i;
     }
     return -1;
 }

 void UploadTreeModel::data_OnStatusChanged(UploadTask* it)
 {
     int row = byUploadTask(it);
	 emit dataChanged(createIndex(row,0,it), createIndex(row,3,it));
 }

  bool UploadTreeModel::insertRow( const QModelIndex &parent)
  {
	  UploadTask* parentItem = reinterpret_cast<UploadTask*>(parent.internalPointer());

      int pos = parentItem? parentItem->childCount(): m_uploadManager->sessionCount();
	  beginInsertRows(parent,pos, pos);
	  endInsertRows();
      return true;
  }

  void UploadTreeModel::recalcObjMap()
  {
      int k = 0;
      int sessionCount = m_uploadManager->sessionCount();
      for( int i = 0; i < sessionCount; i++ ) {
          std::shared_ptr<UploadSession> session = m_uploadManager->session(i);
          InternalItem * item = new InternalItem();
          item->session = session;
          item->index = i;
          sessionsMap_[session.get()] = item;

          int childCount = session->taskCount();
          for ( int j = 0; j < childCount; j++ ) {
              InternalItem * item = new InternalItem();
              item->index = j;
              item->task = session->getTask(j);
              m_objMap[item->task.get()] = item;
          }

      }
  }
