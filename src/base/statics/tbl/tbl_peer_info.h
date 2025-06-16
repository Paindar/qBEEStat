#pragma once

#include <QtSql/QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <QList>

namespace DB
{
    class TblPeerInfo
    {
    public:
        struct PeerInfo
        {
            QString peerIp;
            QString torrentHashId;
            QString peerId;
            qint64 uploadBytes;
            qint64 downloadBytes;
            QDateTime startTime;
        };

        static TblPeerInfo& instance(QSqlDatabase db)
        {
            static TblPeerInfo instance(db);
            return instance;
        }
        struct PrimaryKey
        {
            QString torrentHashId;
            QString peerIp;
        };

    public:
        bool getPeers(const QString& torrentHashId, QList<PeerInfo>& result) const;
        bool getAllPeers(QList<PeerInfo>& peers) const;
        bool insertPeerInfo(const PrimaryKey& key, const QString& peerId, const qint64 uploadBytes, const qint64 downloadBytes, const QDateTime& startTime);
        bool updateTraffic(const PrimaryKey& key, const qint64 uploadBytes, const qint64 downloadBytes);
        bool deletePeerInfo(const PrimaryKey& key);

    private:
        explicit TblPeerInfo(QSqlDatabase db);
        ~TblPeerInfo() = default;
        // Disable copy and move
        TblPeerInfo(const TblPeerInfo&) = delete;
        TblPeerInfo& operator=(const TblPeerInfo&) = delete;
        TblPeerInfo(TblPeerInfo&&) = delete;
        TblPeerInfo& operator=(TblPeerInfo&&) = delete;

        // Database operations
        bool createTable();
        QList<PeerInfo> fetchAllPeersFromDatabase(QString& error) const;
        bool insertPeerInfoToDatabase(const PrimaryKey& key, const QString& peerId, const qint64 uploadBytes, const qint64 downloadBytes, const QDateTime& startTime, QString& error);
        bool updatePeerTrafficInDatabase(const PrimaryKey& key, const qint64 uploadBytes, const qint64 downloadBytes, QString& error);
        bool deletePeerInfoFromDatabase(const PrimaryKey& key, QString& error);

    private:
        QSqlDatabase m_db;
        QHash<PrimaryKey, PeerInfo> m_cache; // Key: peerIp and torrentHashId, Value: PeerInfo
    };
    bool operator==(const TblPeerInfo::PrimaryKey& a, const TblPeerInfo::PrimaryKey& b);
    uint qHash(const TblPeerInfo::PrimaryKey& key, uint seed = 0);
}

