#include "tbl_peer_info.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlError>
#include <QVariant>
#include <QDebug>
#include "base/logger.h"

namespace DB
{
    using namespace Qt::Literals::StringLiterals;

    // Constructor
    TblPeerInfo::TblPeerInfo(QSqlDatabase db)
        : m_db(db)
    {
        createTable();
    }

    // Create the peer_info table if it does not exist
    bool TblPeerInfo::createTable()
    {
        if (!m_db.tables().contains(u"peer_info"_s)) {
            QSqlQuery query(m_db);
            query.prepare(u"CREATE TABLE IF NOT EXISTS peer_info ("
                          u"peer_ip varchar(45), "
                          u"torrent_hash_id varchar(40), "
                          u"peer_id varchar(40) NOT NULL, "
                          u"upload_bytes INTEGER, "
                          u"download_bytes INTEGER, "
                          u"start_time DATETIME, "
                          u"PRIMARY KEY (peer_ip, torrent_hash_id)"
                          u")"_s
            );
            if (!query.exec()) {
                LogMsg(u"Failed to create table peer_info:"_s + query.lastError().text(), Log::CRITICAL);
                return false;
            }
            LogMsg(u"Table peer_info created successfully."_s, Log::INFO);
        }
        QString error;
        QList<PeerInfo> peers = fetchAllPeersFromDatabase(error);
        if (!error.isEmpty()) {
            LogMsg(u"Failed to fetch peers from database: "_s + error, Log::CRITICAL);
            return false;
        }
        m_cache.clear(); // Clear the cache before inserting new peers
        for (const PeerInfo& peer : peers) {
            m_cache.insert({peer.torrentHashId, peer.peerIp}, peer); // Cache the peer info
        }
        return true;
    }

    // Get a single peer by peerIp
    bool TblPeerInfo::getPeers(const QString& torrentHashId, QList<PeerInfo>& result) const
    {
        result.clear();
        for (const PeerInfo& peer : m_cache)
        {
            if (peer.torrentHashId == torrentHashId)
            {
                result.append(peer);
            }
        }
        return true;
    }

    // Get all peers
    bool TblPeerInfo::getAllPeers(QList<PeerInfo>& peers) const
    {
        if (!m_cache.isEmpty())
            peers = m_cache.values();
        return true;
    }

    // Insert a new peer
    bool TblPeerInfo::insertPeerInfo(const PrimaryKey& key, const QString& peerId, const qint64 uploadBytes, const qint64 downloadBytes, const QDateTime& startTime)
    {
        if (m_cache.contains(key)) {
            LogMsg(u"Peer info with peer IP already exists: (%1, %2)"_s.arg(key.torrentHashId, key.peerIp), Log::WARNING);
            return false; // Peer info already exists
        }
        QString error;
        if (!insertPeerInfoToDatabase(key, peerId, uploadBytes, downloadBytes, startTime, error)) {
            LogMsg(u"Failed to insert peer info: "_s + error, Log::CRITICAL);
            return false;
        }
        m_cache.emplace(key, key.peerIp, key.torrentHashId, peerId, uploadBytes, downloadBytes, startTime); // Cache the new peer info
        return true;
    }

    // Update an existing peer
    bool TblPeerInfo::updateTraffic(const PrimaryKey& key, const qint64 uploadBytes, const qint64 downloadBytes)
    {
        if (!m_cache.contains(key)) {
            LogMsg(u"Peer info with peer IP does not exist: (%1, %2)"_s.arg(key.torrentHashId, key.peerIp), Log::WARNING);
            return false; // Peer info does not exist
        }
        QString error;
        if (!updatePeerTrafficInDatabase(key, uploadBytes, downloadBytes, error)) {
            LogMsg(u"Failed to update peer info: "_s + error, Log::CRITICAL);
            return false;
        }
        PeerInfo& info = m_cache[key];
        info.uploadBytes = uploadBytes; // Update the cached info
        info.downloadBytes = downloadBytes; // Update the cached info
        return true;
    }

    // Delete a peer by peerIp
    bool TblPeerInfo::deletePeerInfo(const PrimaryKey& key)
    {
        if (!m_cache.contains(key)) {
            LogMsg(u"Peer info with peer IP does not exist: (%1, %2)"_s.arg(key.torrentHashId, key.peerIp), Log::WARNING);
            return false; // Peer info does not exist
        }
        QString error;
        if (!deletePeerInfoFromDatabase(key, error)) {
            LogMsg(u"Failed to delete peer info: "_s + error, Log::CRITICAL);
            return false;
        }
        m_cache.remove(key); // Remove from cache
        return true;
    }

    // Fetch all peers from the database (for internal use)
    QList<TblPeerInfo::PeerInfo> TblPeerInfo::fetchAllPeersFromDatabase(QString& error) const
    {
        QList<PeerInfo> peers;
        QSqlQuery query(m_db);
        query.prepare(u"SELECT peer_ip, peer_id, torrent_hash_id, upload_bytes, download_bytes, start_time FROM peer_info"_s);
        if (query.exec())
        {
            while (query.next())
            {
                PeerInfo info;
                info.peerIp = query.value(0).toString();
                info.peerId = query.value(1).toString();
                info.torrentHashId = query.value(2).toString();
                info.uploadBytes = query.value(3).toLongLong();
                info.downloadBytes = query.value(4).toLongLong();
                info.startTime = query.value(5).toDateTime();
                peers.emplace_back(std::move(info));
            }
        } else {
            error = query.lastError().text();
        }
        return peers;
    }

    bool TblPeerInfo::insertPeerInfoToDatabase(const PrimaryKey& key, const QString &peerId, const qint64 uploadBytes, const qint64 downloadBytes, const QDateTime &startTime, QString& error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"INSERT INTO peer_info (torrent_hash_id, peer_ip, peer_id, upload_bytes, download_bytes, start_time) "
                      "VALUES (:torrent_hash_id, :peer_ip, :peer_id, :upload_bytes, :download_bytes, :start_time)"_s);
        query.bindValue(u":torrent_hash_id"_s, key.torrentHashId);
        query.bindValue(u":peer_ip"_s, key.peerIp);
        query.bindValue(u":peer_id"_s, peerId);
        query.bindValue(u":upload_bytes"_s, uploadBytes);
        query.bindValue(u":download_bytes"_s, downloadBytes);
        query.bindValue(u":start_time"_s, startTime);
        if (!query.exec()) {
            error = query.lastError().text();
            return false;
        }
        return true;
    }

    // Update peer info in database (for internal use)
    bool TblPeerInfo::updatePeerTrafficInDatabase(const PrimaryKey& key, const qint64 uploadBytes, const qint64 downloadBytes, QString& error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"UPDATE peer_info SET upload_bytes = :upload_bytes, download_bytes = :download_bytes"
            " WHERE torrent_hash_id = :torrent_hash_id and peer_ip = :peer_ip"_s);
        query.bindValue(u":torrent_hash_id"_s, key.torrentHashId);
        query.bindValue(u":peer_ip"_s, key.peerIp);
        query.bindValue(u":upload_bytes"_s, uploadBytes);
        query.bindValue(u":download_bytes"_s, downloadBytes);
        if (!query.exec()) {
            error = query.lastError().text();
            return false;
        }
        return true;
    }

    // Delete peer info from database (for internal use)
    bool TblPeerInfo::deletePeerInfoFromDatabase(const PrimaryKey& key, QString& error)
    {
        QSqlQuery query(m_db);
        query.prepare(u"DELETE FROM peer_info "
            u"WHERE torrent_hash_id = :torrent_hash_id "
            u"and peer_ip = :peer_ip"_s);
        query.bindValue(u":torrent_hash_id"_s, key.torrentHashId);
        query.bindValue(u":peer_ip"_s, key.peerIp);
        if (!query.exec()) {
            error = query.lastError().text();
            return false;
        }
        return true;
    }

    bool operator==(const TblPeerInfo::PrimaryKey& a, const TblPeerInfo::PrimaryKey& b)
    {
        return a.peerIp == b.peerIp && a.torrentHashId == b.torrentHashId;
    }

    uint qHash(const TblPeerInfo::PrimaryKey& key, uint seed)
    {
        seed = qHash(key.peerIp, seed);
        seed = qHash(key.torrentHashId, seed);
        return seed;
    }
}
