#include "tbl_torrent_info.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlError>
#include <QDebug>
#include "base/logger.h"

namespace DB
{
    using namespace Qt::Literals::StringLiterals;

    TblTorrentInfo::TblTorrentInfo(QSqlDatabase db) : m_db(db)
    {
        createTable();
    }

    bool TblTorrentInfo::createTable()
    {
        if(!m_db.tables().contains(u"torrent_info"_s))
        {
            QSqlQuery query(m_db);
            query.prepare(u"CREATE TABLE IF NOT EXISTS torrent_info ("
                u"hash_id VARCHAR(40) PRIMARY KEY, "
                u"size INTEGER, "
                u"create_time DATETIME, "
                u"finish_time DATETIME)"_s);
            if (!query.exec())
            {
                LogMsg(u"Failed to create table torrent_info: "_s + query.lastError().text(), Log::CRITICAL);
                return false;
            }
        }
        QString error;
        QList<TblTorrentInfo::TorrentInfo> torrents = fetchAllTorrentsFromDatabase(error);
        if (!error.isEmpty())
        {
            LogMsg(u"Failed to fetch torrents from database: "_s + error, Log::CRITICAL);
            return false;
        }
        m_cache.clear(); // Clear the cache before populating it
        for (const auto& torrent : torrents)
        {
            m_cache.insert(torrent.hashId, torrent);
        }
        return true;
    }
    bool TblTorrentInfo::getTorrentInfo(const QString &hashId, TorrentInfo &info) const
    {
        if (m_cache.contains(hashId))
        {
            info = m_cache.value(hashId);
            return true;
        }
        return false;
    }
    bool TblTorrentInfo::getAllTorrents(QList<TorrentInfo> &torrents) const
    {
        if (!m_cache.isEmpty())
        {
            torrents = m_cache.values();
        }
        return true;
    }
    bool TblTorrentInfo::insertTorrentInfo(const QString &hashId, const qint64 size, const QDateTime &createTime, const QDateTime &finishTime)
    {
        if (m_cache.contains(hashId))
        {
            LogMsg(u"Torrent info with hash id already exists: "_s + hashId, Log::WARNING);
            return false; // Torrent info already exists
        }
        QString error;
        if (!insertTorrentInfoToDatabase(hashId, size, createTime, finishTime, error))
        {
            LogMsg(u"Failed to insert torrent info: "_s + error, Log::CRITICAL);
            return false;
        }
        m_cache.emplace(hashId, hashId, size, createTime, finishTime);
        return true;
    }
    bool TblTorrentInfo::updateTorrentInfo(const QString &hashId, const QDateTime &finishTime)
    {
        if (!m_cache.contains(hashId))
        {
            LogMsg(u"Torrent info with hash id does not exist: "_s + hashId, Log::WARNING);
            return false; // Torrent info does not exist
        }
        QString error;
        if (!updateTorrentInfoInDatabase(hashId, finishTime, error))
        {
            LogMsg(u"Failed to update torrent info: "_s + error, Log::CRITICAL);
            return false;
        }
        TorrentInfo &info = m_cache[hashId];
        info.finishTime = finishTime; // Update the cached info
        return true;
    }
    bool TblTorrentInfo::deleteTorrentInfo(const QString &hashId)
    {
        if (!m_cache.contains(hashId))
        {
            LogMsg(u"Torrent info with hash id does not exist: "_s + hashId, Log::WARNING);
            return false; // Torrent info does not exist
        }
        QString error;
        if (!deleteTorrentInfoFromDatabase(hashId, error))
        {
            LogMsg(u"Failed to delete torrent info: "_s + error, Log::CRITICAL);
            return false;
        }
        m_cache.remove(hashId); // Remove from cache
        return true;
    }

    QList<TblTorrentInfo::TorrentInfo> TblTorrentInfo::fetchAllTorrentsFromDatabase(QString & error) const
    {
        QSqlQuery query(m_db);
        query.prepare(u"SELECT hash_id, size, create_time, finish_time FROM torrent_info"_s);
        QList<TblTorrentInfo::TorrentInfo> result;
        if (!query.exec())
        {
            error = query.lastError().text();
            return result;
        }
        while (query.next())
        {
            TblTorrentInfo::TorrentInfo row;
            row.hashId = query.value(0).toString();
            row.size = query.value(1).toLongLong();
            row.createTime = query.value(2).toDateTime();
            row.finishTime = query.value(3).toDateTime();
            result.emplace_back(std::move(row));
        }
        if (query.lastError().isValid())
        {
            error = query.lastError().text();
            return QList<TorrentInfo>();
        }
        return result;
    }
    bool TblTorrentInfo::insertTorrentInfoToDatabase(const QString &hashId, const qint64 size, const QDateTime &createTime, const QDateTime &finishTime, QString &error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"INSERT INTO torrent_info (hash_id, size, create_time, finish_time) VALUES (:hash_id, :size, :create_time, :finish_time)"_s);
        query.bindValue(u":hash_id"_s, hashId);
        query.bindValue(u":size"_s, size);
        query.bindValue(u":create_time"_s, createTime);
        query.bindValue(u":finish_time"_s, finishTime);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        return true;
    }
    bool TblTorrentInfo::updateTorrentInfoInDatabase(const QString &hashId, const QDateTime &finishTime, QString &error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"UPDATE torrent_info SET finish_time = :finish_time WHERE hash_id = :hash_id"_s);
        query.bindValue(u":finish_time"_s, finishTime);
        query.bindValue(u":hash_id"_s, hashId);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        return true;
    }
    bool TblTorrentInfo::deleteTorrentInfoFromDatabase(const QString &hashId, QString &error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"DELETE FROM torrent_info WHERE hash_id = :hash_id"_s);
        query.bindValue(u":hash_id"_s, hashId);
        if (!query.exec())
        {
            error = query.lastError().text();
            return false;
        }
        return true;
    }
}