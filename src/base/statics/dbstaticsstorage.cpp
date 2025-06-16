#include "dbstaticsstorage.h"
#include <QtSql/QSqlError>
#include "base/profile.h"

using namespace Qt::Literals::StringLiterals;
namespace
{
    const QString DB_CONNECTION_NAME = u"TrafficStatics"_s;

    const int DB_VERSION = 8;
}
DbStaticsStorage& DbStaticsStorage::instance()
{
    static DbStaticsStorage instance;
    return instance;
}
const Path& DbStaticsStorage::dbPath()
{
    return instance().m_dbPath;
}
DbStaticsStorage::DbStaticsStorage() : m_dbPath(specialFolderLocation(SpecialFolder::Data) / Path(u"statics.db"_s)), m_db(QSqlDatabase::addDatabase(u"QSQLITE"_s, DB_CONNECTION_NAME))
{
    m_db.setDatabaseName(m_dbPath.toString());
    if (!m_db.open())
    {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return;
    }
}
DbStaticsStorage::~DbStaticsStorage()
{
    if (m_db.isOpen())
    {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(DB_CONNECTION_NAME);
}

DB::TblTorrentInfo& DbStaticsStorage::getTorrentInfoTable()
{
    return DB::TblTorrentInfo::instance(m_db);
}
DB::TblTorrentHistory& DbStaticsStorage::getTorrentHistoryTable()
{
    return DB::TblTorrentHistory::instance(m_db);
}

DB::TblPeerInfo& DbStaticsStorage::getPeerInfoTable()
{
    return DB::TblPeerInfo::instance(m_db);
}

DB::TblPeerHistory& DbStaticsStorage::getPeerHistoryTable()
{
    return DB::TblPeerHistory::instance(m_db);
}

DB::TblContributionHistory& DbStaticsStorage::getContributionHistoryTable()
{
    return DB::TblContributionHistory::instance(m_db);
}
