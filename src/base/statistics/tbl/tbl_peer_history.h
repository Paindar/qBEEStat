#pragma once
#include <QtSql/QSqlDatabase>
#include <QList>
#include <QDateTime>
#include <QString>

namespace DB
{
    class TblPeerHistory
    {
    public:
        struct PeerHistory
        {
            QString peerIp;
            QString torrentHashId;
            QString eventId;
            QString eventArgs;
            QDateTime eventTime;
        };
        static TblPeerHistory& instance(QSqlDatabase db)
        {
            static TblPeerHistory instance(db);
            return instance;
        }
        
    public:
        bool insertTorrentHistory(const QString& peerIp, const QString& torrentHashId, const QString& eventId, const QString& eventArgs);
        bool getPeerHistory(const QString& peerIp, const QString& torrentHashId, QList<PeerHistory>& result) const;
        bool getPeerHistory(const QString& peerIp, QList<PeerHistory>& result) const;
    private:
        explicit TblPeerHistory(QSqlDatabase db);
        ~TblPeerHistory() = default;
        // Delete copy and move constructors
        TblPeerHistory(const TblPeerHistory&) = delete;
        TblPeerHistory& operator=(const TblPeerHistory&) = delete;
        TblPeerHistory(TblPeerHistory&&) = delete;
        TblPeerHistory& operator=(TblPeerHistory&&) = delete;

        bool createTable();
        bool insertTorrentHistoryIntoDatabase(const QString& peerId, const QString& torrentHashId, const QString& eventId, const QString& eventArgs, QString& error);
        bool getPeerHistoryFromDatabase(const QString& peerIp, const QString& torrentHashId, QList<PeerHistory>& result, QString& error) const;
        bool getPeerHistoryFromDatabase(const QString& peerIp, QList<PeerHistory>& result, QString& error) const;
    private:
        QSqlDatabase m_db;
    };
}
