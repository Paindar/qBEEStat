#pragma once

#include <QtSql/QSqlDatabase>
#include <QList>
#include <QString>
#include <QDateTime>

namespace DB
{
    //CREATE TABLE IF NOT EXISTS torrent_history (id INTEGER PRIMARY KEY AUTOINCREMENT, hash_id TEXT NOT NULL, event_id TEXT NOT NULL, event_args TEXT, event_time DATETIME DEFAULT CURRENT_TIMESTAMP)
    class TblTorrentHistory
    {
    public:
        struct TorrentHistory
        {
            QString hashId;
            QString eventId;
            QString eventArgs;
            QDateTime eventTime;
        };
        static TblTorrentHistory& instance(QSqlDatabase db)
        {
            static TblTorrentHistory instance(db);
            return instance;
        }
        // Add methods to interact with the database here
        bool getTorrentHistory(const QString& hashId, QList<TorrentHistory>& result) const;

        bool insertTorrentHistory(const QString& hashId, const QString& eventId, const QString& eventArgs);
    private:
        explicit TblTorrentHistory(QSqlDatabase db) : m_db(db)
        {
            createTable();
        }
        ~TblTorrentHistory() = default;
        // Delete copy and move constructors
        TblTorrentHistory(const TblTorrentHistory&) = delete;
        TblTorrentHistory& operator=(const TblTorrentHistory&) = delete;
        TblTorrentHistory(TblTorrentHistory&&) = delete;
        TblTorrentHistory& operator=(TblTorrentHistory&&) = delete;

        void createTable();
        //Deprecated: Will be removed in future versions
        bool getTorrentHistoryFromDatabase(const QString& hashId, QList<TorrentHistory>& history, QString& error) const;
        bool insertTorrentHistoryIntoDatabase(const QString& hashId, const QString& eventId, const QString& eventArgs, QString& error);
    private:
        QSqlDatabase m_db;
    };
}