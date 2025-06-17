#include "dbstatisticsstorage.h"
#include <QtSql/QSqlError>
#include "base/profile.h"

using namespace Qt::Literals::StringLiterals;
namespace
{
    const QString DB_CONNECTION_NAME = u"TrafficStatistics"_s;

    const int DB_VERSION = 8;
}
DbStatisticsStorage& DbStatisticsStorage::instance()
{
    static DbStatisticsStorage instance;
    return instance;
}
const Path& DbStatisticsStorage::dbPath()
{
    return instance().m_dbPath;
}
DbStatisticsStorage::DbStatisticsStorage() : m_dbPath(specialFolderLocation(SpecialFolder::Data) / Path(u"statistics.db"_s)), m_db(QSqlDatabase::addDatabase(u"QSQLITE"_s, DB_CONNECTION_NAME))
{
    m_db.setDatabaseName(m_dbPath.toString());
    if (!m_db.open())
    {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return;
    }
}
DbStatisticsStorage::~DbStatisticsStorage()
{
    if (m_db.isOpen())
    {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(DB_CONNECTION_NAME);
}

DB::TblTorrentInfo& DbStatisticsStorage::getTorrentInfoTable()
{
    return DB::TblTorrentInfo::instance(m_db);
}
DB::TblTorrentHistory& DbStatisticsStorage::getTorrentHistoryTable()
{
    return DB::TblTorrentHistory::instance(m_db);
}

DB::TblPeerInfo& DbStatisticsStorage::getPeerInfoTable()
{
    return DB::TblPeerInfo::instance(m_db);
}

DB::TblPeerHistory& DbStatisticsStorage::getPeerHistoryTable()
{
    return DB::TblPeerHistory::instance(m_db);
}

DB::TblContributionHistory& DbStatisticsStorage::getContributionHistoryTable()
{
    return DB::TblContributionHistory::instance(m_db);
}
