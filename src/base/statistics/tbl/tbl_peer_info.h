#pragma once

#include <QtSql/QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <optional>

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
            std::optional<QDateTime> endingTime; // Time when the peer was last seen
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
        bool getPeer(const PrimaryKey& key, PeerInfo& result) const;
        bool getPeers(const QString& torrentHashId, QList<PeerInfo>& result) const;
        bool getDeadPeers(int interval, QList<PeerInfo>& result) const;
        bool getAllPeers(QList<PeerInfo>& peers) const;
        bool HasPeerInfo(const PrimaryKey& key) const;
        bool insertPeerInfo(const PrimaryKey& key, const QString& peerId, const qint64 uploadBytes, const qint64 downloadBytes, const QDateTime& startTime);
        bool updateTraffic(const PrimaryKey& key, const qint64 uploadBytes, const qint64 downloadBytes);
        bool setPeerEndingTime(const PrimaryKey& key, const QDateTime& endingTime);
        bool removePeerEndingTime(const PrimaryKey& key);
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
        bool setPeerEndingTimeInDatabase(const PrimaryKey& key, const QDateTime& endingTime, QString& error);
        bool removePeerEndingTimeFromDatabase(const PrimaryKey& key, QString& error);
        bool deletePeerInfoFromDatabase(const PrimaryKey& key, QString& error);

    private:
        QSqlDatabase m_db;
        QHash<PrimaryKey, PeerInfo> m_cache; // Key: peerIp and torrentHashId, Value: PeerInfo
    };
    bool operator==(const TblPeerInfo::PrimaryKey& a, const TblPeerInfo::PrimaryKey& b);
    uint qHash(const TblPeerInfo::PrimaryKey& key, uint seed = 0);
}
