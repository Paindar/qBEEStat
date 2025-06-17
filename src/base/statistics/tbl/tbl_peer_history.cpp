#include "tbl_peer_history.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlError>
#include <QDateTime>
#include "base/logger.h"

namespace DB
{
    using namespace Qt::Literals::StringLiterals;

    TblPeerHistory::TblPeerHistory(QSqlDatabase db) : m_db(db)
    {
        if (!createTable())
        {
            LogMsg(u"Failed to create peer_history table"_s, Log::CRITICAL);
        }
    }
    
    bool TblPeerHistory::insertTorrentHistory(const QString& peerIp, const QString& torrentHashId, const QString& eventId, const QString& eventArgs)
    {
        QString error;
        if (!insertTorrentHistoryIntoDatabase(peerIp, torrentHashId, eventId, eventArgs, error))
        {
            LogMsg(u"Failed to insert peer history into the database: peerIp=%1, torrentHashId=%2, eventId=%3, eventArgs=%4, errorContent=%5 "_s.arg(peerIp, torrentHashId, eventId, eventArgs, error), Log::CRITICAL);
            return false;
        }
        return true;
    }

    bool TblPeerHistory::getPeerHistory(const QString& peerIp, const QString& torrentHashId, QList<PeerHistory>& result) const
    {
        QString error;
        if (!getPeerHistoryFromDatabase(peerIp, torrentHashId, result, error))
        {
            LogMsg(u"Failed to get peer history from the database: "_s + error, Log::CRITICAL);
            return false;
        }
        return true;
    }

    bool TblPeerHistory::getPeerHistory(const QString& peerIp, QList<PeerHistory>& result) const
    {
        QString error;
        if (!getPeerHistoryFromDatabase(peerIp, result, error))
        {
            LogMsg(u"Failed to get peer history from the database: "_s + error, Log::CRITICAL);
            return false;
        }
        return true;
    }
    bool TblPeerHistory::createTable()
    {
        if(!m_db.tables().contains(u"peer_history"_s))
        {
            QSqlQuery query(m_db);
            query.prepare(u"CREATE TABLE IF NOT EXISTS peer_history ("
                          u"id INTEGER PRIMARY KEY AUTOINCREMENT, "
                          u"peer_ip VARCHAR(45) NOT NULL, "
                          u"torrent_hash_id VARCHAR(40) NOT NULL, "
                          u"event_id VARCHAR(10) NOT NULL, "
                          u"event_args TEXT, "
                          u"event_time DATETIME DEFAULT CURRENT_TIMESTAMP)"_s);
            if (!query.exec())
            {
                LogMsg(u"Failed to create peer_history table: "_s + query.lastError().text(), Log::CRITICAL);
                return false;
            }
            QSqlQuery indexQuery(m_db);
            // Create index for faster lookups
            indexQuery.prepare(u"CREATE INDEX IF NOT EXISTS idx_peer_history ON peer_history (peer_ip, torrent_hash_id)"_s);
            if (!indexQuery.exec())
            {
                LogMsg(u"Failed to create index on peer_history: "_s + indexQuery.lastError().text(), Log::CRITICAL);
            }
        }
        return true;
    }

    bool TblPeerHistory::insertTorrentHistoryIntoDatabase(const QString& peerIp, const QString& torrentHashId, const QString& eventId, const QString& eventArgs, QString& error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"INSERT INTO peer_history (peer_ip, torrent_hash_id, event_id, event_args) VALUES (:peer_ip, :torrent_hash_id, :event_id, :event_args)"_s);
        query.bindValue(u":peer_ip"_s, peerIp);
        query.bindValue(u":torrent_hash_id"_s, torrentHashId);
        query.bindValue(u":event_id"_s, eventId);
        query.bindValue(u":event_args"_s, eventArgs);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        return true;
    }

    bool TblPeerHistory::getPeerHistoryFromDatabase(const QString& peerIp, const QString& torrentHashId, QList<PeerHistory>& result, QString& error) const
    {
        QSqlQuery query(m_db);
        query.prepare(u"SELECT peer_ip, torrent_hash_id, event_id, event_args, event_time FROM peer_history WHERE peer_ip = :peer_ip AND torrent_hash_id = :torrent_hash_id"_s);
        query.bindValue(u"peer_ip"_s, peerIp);
        query.bindValue(u"torrent_hash_id"_s, torrentHashId);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        
        while (query.next())
        {
            PeerHistory history;
            history.peerIp = query.value(0).toString();
            history.torrentHashId = query.value(1).toString();
            history.eventId = query.value(2).toString();
            history.eventArgs = query.value(3).toString();
            history.eventTime = query.value(4).toDateTime();
            result.append(history);
        }
        return true;
    }

    bool TblPeerHistory::getPeerHistoryFromDatabase(const QString& peerIp, QList<PeerHistory>& result, QString& error) const
    {
        QSqlQuery query(m_db);
        query.prepare(u"SELECT peer_ip, torrent_hash_id, event_id, event_args, event_time FROM peer_history WHERE peer_ip = :peer_ip"_s);
        query.bindValue(u"peer_ip"_s, peerIp);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        
        while (query.next())
        {
            PeerHistory history;
            history.peerIp = query.value(0).toString();
            history.torrentHashId = query.value(1).toString();
            history.eventId = query.value(2).toString();
            history.eventArgs = query.value(3).toString();
            history.eventTime = query.value(4).toDateTime();
            result.append(history);
        }
        return true;
        
    }
}
