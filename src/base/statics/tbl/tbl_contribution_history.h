#pragma once
#include <QtSql/QSqlDatabase>
#include <QDateTime>
#include <QList>
#include <QString>

namespace DB
{
    class TblContributionHistory
    {
    public:
        struct ContributionHistory
        {
            QString torrentHashId;
            QString peerIp;
            qint64 uploadBytes;
            qint64 downloadBytes;
            QDateTime startTime;
            QDateTime endTime;
        };
        static TblContributionHistory& instance(QSqlDatabase db)
        {
            static TblContributionHistory instance(db);
            return instance;
        }
    public:
        // Add methods to interact with the database here
        bool getContributionHistory(const QString& torrentHashId, QList<ContributionHistory>& result) const;
        bool getContributionHistory(const QString& torrentHashId, const QString& peerIp, QList<ContributionHistory>& result) const;
        bool insertContributionHistory(const QString& torrentHashId, const QString& peerIp, qint64 uploadBytes, qint64 downloadBytes, const QDateTime& startTime, const QDateTime& endTime);
    private:
        explicit TblContributionHistory(QSqlDatabase db);
        ~TblContributionHistory() = default;
        // Delete copy and move constructors
        TblContributionHistory(const TblContributionHistory&) = delete;
        TblContributionHistory& operator=(const TblContributionHistory&) = delete;
        TblContributionHistory(TblContributionHistory&&) = delete;
        TblContributionHistory& operator=(TblContributionHistory&&) = delete;
        bool createTable() const;

        bool getContributionHistoryFromDatabase(const QString& torrentHashId, QList<ContributionHistory>& result, QString& error) const;
        bool getContributionHistoryFromDatabase(const QString& torrentHashId, const QString& peerIp, QList<ContributionHistory>& result, QString& error) const;

        bool insertContributionHistoryIntoDatabase(const QString& torrentHashId, const QString& peerIp, qint64 uploadBytes, qint64 downloadBytes, const QDateTime& startTime, const QDateTime& endTime, QString& error);
    private:
        QSqlDatabase m_db;
    };
}

        