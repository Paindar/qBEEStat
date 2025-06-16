#include "tbl_contribution_history.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlError>
#include <QDateTime>
#include "base/logger.h"

namespace DB
{
    using namespace Qt::Literals::StringLiterals;

    TblContributionHistory::TblContributionHistory(QSqlDatabase db) : m_db(db)
    {
        createTable();
    }

    bool TblContributionHistory::getContributionHistory(const QString& torrentHashId, QList<ContributionHistory>& result) const
    {
        QString error;
        if (!getContributionHistoryFromDatabase(torrentHashId, result, error))
        {
            LogMsg(u"Failed to get contribution history from the database: "_s + error, Log::CRITICAL);
            return false;
        }
        return true;
    }

    bool TblContributionHistory::getContributionHistory(const QString& torrentHashId, const QString& peerIp, QList<ContributionHistory>& result) const
    {
        QString error;
        if (!getContributionHistoryFromDatabase(torrentHashId, peerIp, result, error))
        {
            LogMsg(u"Failed to get contribution history from the database: "_s + error, Log::CRITICAL);
            return false;
        }
        return true;
    }

    bool TblContributionHistory::insertContributionHistory(const QString& torrentHashId, const QString& peerIp, qint64 uploadBytes, qint64 downloadBytes, const QDateTime& startTime, const QDateTime& endTime)
    {
        QString error;
        if (!insertContributionHistoryIntoDatabase(torrentHashId, peerIp, uploadBytes, downloadBytes, startTime, endTime, error))
        {
            LogMsg(u"Failed to insert contribution history into the database: "_s + error, Log::CRITICAL);
            return false;
        }
        return true;
    }

    bool TblContributionHistory::createTable() const
    {
        if (!m_db.tables().contains(u"contribution_history"_s))
        {
            QSqlQuery query(m_db);
            query.prepare(u"CREATE TABLE IF NOT EXISTS contribution_history ("
                          u"id INTEGER PRIMARY KEY AUTOINCREMENT, "
                          u"torrent_hash_id VARCHAR(40) NOT NULL, "
                          u"peer_ip VARCHAR(45) NOT NULL, "
                          u"upload_bytes INTEGER, "
                          u"download_bytes INTEGER, "
                          u"start_time DATETIME, "
                          u"end_time DATETIME)"_s);
            if (!query.exec())
            {
                LogMsg(u"Failed to create contribution_history table: "_s + query.lastError().text(), Log::CRITICAL);
                return false;
            }

            QSqlQuery indexQuery(m_db);
            indexQuery.prepare(u"CREATE INDEX IF NOT EXISTS idx_contribution_history ON contribution_history(peer_ip, torrent_hash_id)"_s);
            if (!indexQuery.exec()) {
                LogMsg(u"Failed to create index on contribution_history: "_s + indexQuery.lastError().text(), Log::CRITICAL);
                return false;
            }
        }
        return true;
    }

    bool TblContributionHistory::getContributionHistoryFromDatabase(const QString& torrentHashId, QList<ContributionHistory>& result, QString& error) const
    {
        QSqlQuery query(m_db);
        query.prepare(u"SELECT torrent_hash_id, peer_ip, upload_bytes, download_bytes, start_time, end_time FROM contribution_history WHERE torrent_hash_id = :torrent_hash_id"_s);
        query.bindValue(u":torrent_hash_id"_s, torrentHashId);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        while (query.next())
        {
            ContributionHistory history;
            history.torrentHashId = query.value(0).toString();
            history.peerIp = query.value(1).toString();
            history.uploadBytes = query.value(2).toLongLong();
            history.downloadBytes = query.value(3).toLongLong();
            history.startTime = query.value(4).toDateTime();
            history.endTime = query.value(5).toDateTime();
            result.append(history);
        }
        return true;
    }

    bool TblContributionHistory::getContributionHistoryFromDatabase(const QString& torrentHashId, const QString& peerIp, QList<ContributionHistory>& result, QString& error) const
    {
        QSqlQuery query(m_db);
        query.prepare(u"SELECT torrent_hash_id, peer_ip, upload_bytes, download_bytes, start_time, end_time FROM contribution_history WHERE torrent_hash_id = :torrent_hash_id AND peer_ip = :peer_ip"_s);
        query.bindValue(u":torrent_hash_id"_s, torrentHashId);
        query.bindValue(u":peer_ip"_s, peerIp);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        
        while (query.next())
        {
            ContributionHistory history;
            history.torrentHashId = query.value(0).toString();
            history.peerIp = query.value(1).toString();
            history.uploadBytes = query.value(2).toLongLong();
            history.downloadBytes = query.value(3).toLongLong();
            history.startTime = query.value(4).toDateTime();
            history.endTime = query.value(5).toDateTime();
            result.append(history);
        }
        return true;
    }

    bool TblContributionHistory::insertContributionHistoryIntoDatabase(const QString& torrentHashId, const QString& peerIp, qint64 uploadBytes, qint64 downloadBytes, const QDateTime& startTime, const QDateTime& endTime, QString& error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"INSERT INTO contribution_history (torrent_hash_id, peer_ip, upload_bytes, download_bytes, start_time, end_time) "
                      u"VALUES (:torrent_hash_id, :peer_ip, :upload_bytes, :download_bytes, :start_time, :end_time)"_s);
        query.bindValue(u":torrent_hash_id"_s, torrentHashId);
        query.bindValue(u":peer_ip"_s, peerIp);
        query.bindValue(u":upload_bytes"_s, uploadBytes);
        query.bindValue(u":download_bytes"_s, downloadBytes);
        query.bindValue(u":start_time"_s, startTime);
        query.bindValue(u":end_time"_s, endTime);
        
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        
        return true;
    }
}