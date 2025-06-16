#include "tbl_torrent_history.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlError>
#include <QVariant>
#include <QDebug>
#include "base/logger.h"

namespace DB
{
    using namespace Qt::Literals::StringLiterals;

    // Add methods to interact with the database here
    bool TblTorrentHistory::getTorrentHistory(const QString& hashId, QList<TblTorrentHistory::TorrentHistory>& result) const
    {
        QString error;
        if (!getTorrentHistoryFromDatabase(hashId, result, error))
        {
            LogMsg(u"Failed to get torrent history from the database: "_s + error, Log::CRITICAL);
            return false;
        }
        return true;
    }
    bool TblTorrentHistory::insertTorrentHistory(const QString& hashId, const QString& eventId, const QString& eventArgs)
    {
        QString error;
        if (!insertTorrentHistoryIntoDatabase(hashId, eventId, eventArgs, error))
        {
            LogMsg(u"Failed to insert torrent history into the database: "_s + error, Log::CRITICAL);
            return false;
        }
        return true;
    }

    void TblTorrentHistory::createTable()
    {
        if(!m_db.tables().contains(u"torrent_history"_s))
        {
            // Create the torrent_history table if it does not exist
            QSqlQuery query(m_db);
            query.prepare(u"CREATE TABLE IF NOT EXISTS torrent_history ("
                u"id INTEGER PRIMARY KEY AUTOINCREMENT, "
                u"hash_id VARCHAR(40) NOT NULL, "
                u"event_id VARCHAR(10) NOT NULL, "
                u"event_args TEXT, "
                u"event_time DATETIME DEFAULT CURRENT_TIMESTAMP"
                u")"_s);
            if (!query.exec())
            {
                LogMsg(u"Failed to create torrent_history table: "_s + query.lastError().text(), Log::CRITICAL);
            }
            // Create an index on hash_id for faster lookups
            QSqlQuery indexQuery(m_db);
            indexQuery.prepare(u"CREATE INDEX IF NOT EXISTS idx_hash_id ON torrent_history (hash_id)"_s);
            if (!indexQuery.exec())
            {
                LogMsg(u"Failed to create index on torrent_history: "_s + indexQuery.lastError().text(), Log::CRITICAL);
            }
        }
    }
    bool TblTorrentHistory::getTorrentHistoryFromDatabase(const QString &hashId, QList<TorrentHistory> &history, QString &error) const
    {
        QSqlQuery query(m_db);
        query.prepare(u"SELECT hash_id, event_id, event_args, event_time FROM torrent_history WHERE hash_id = :hash_id"_s);
        query.bindValue(u":hash_id"_s, hashId);
        if (query.exec())
        {
            while (query.next())
            {
                TorrentHistory th;
                th.hashId = query.value(0).toString();
                th.eventId = query.value(1).toString();
                th.eventArgs = query.value(2).toString();
                th.eventTime = query.value(3).toDateTime();
                history.append(th);
            }
            return true;
        }
        else
        {
            error = query.lastError().text();
            return false;
        }
        return true;
    }
    bool TblTorrentHistory::insertTorrentHistoryIntoDatabase(const QString &hashId, const QString &eventId, const QString &eventArgs, QString &error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"INSERT INTO torrent_history (hash_id, event_id, event_args) VALUES (:hash_id, :event_id, :event_args)"_s);
        query.bindValue(u":hash_id"_s, hashId);
        query.bindValue(u":event_id"_s, eventId);
        query.bindValue(u":event_args"_s, eventArgs);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        return true;
    }
}