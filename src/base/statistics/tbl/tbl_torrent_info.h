#pragma once
#include <QtSql/QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <QHash>

namespace DB
{
    class TblTorrentInfo /*: public TableTemplate<TblTorrentInfo::Column>*/
    {
    public:
        struct TorrentInfo
        {
            QString hashId;
            qint64 size;
            QDateTime createTime;
            QDateTime finishTime;
        };
        static TblTorrentInfo& instance(QSqlDatabase db)
        {
            static TblTorrentInfo instance(db);
            return instance;
        }
    public:
        bool getTorrentInfo(const QString& hashId, TorrentInfo& info) const;
        bool getAllTorrents(QList<TorrentInfo>& torrents) const;
        bool insertTorrentInfo(const QString& hashId, const qint64 size, const QDateTime& createTime, const QDateTime& finishTime);
        bool updateTorrentInfo(const QString& hashId, const QDateTime& finishTime);
        bool deleteTorrentInfo(const QString& hashId);
    private:
        explicit TblTorrentInfo(QSqlDatabase db);
        ~TblTorrentInfo() = default;
        // Disable copy and move
        TblTorrentInfo(const TblTorrentInfo&) = delete;
        TblTorrentInfo& operator=(const TblTorrentInfo&) = delete;
        TblTorrentInfo(TblTorrentInfo&&) = delete;
        TblTorrentInfo& operator=(TblTorrentInfo&&) = delete;
        // Database operations
        bool createTable();
        QList<TorrentInfo> fetchAllTorrentsFromDatabase(QString& error) const;
        bool insertTorrentInfoToDatabase(const QString& hashId, const qint64 size, const QDateTime& createTime, const QDateTime& finishTime, QString& error);
        bool updateTorrentInfoInDatabase(const QString& hashId, const QDateTime& finishTime, QString& error);
        bool deleteTorrentInfoFromDatabase(const QString& hashId, QString& error);
    private:
        QSqlDatabase m_db;
        QHash<QString, TorrentInfo> m_cache; // Cache for quick access
    };
}
