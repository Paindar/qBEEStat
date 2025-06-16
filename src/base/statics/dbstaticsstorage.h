#pragma once

#include <QtSql/QSqlDatabase>
#include "base/path.h"
#include "base/pathfwd.h"
#include "tbl/tbl_torrent_info.h"
#include "tbl/tbl_torrent_history.h"
#include "tbl/tbl_peer_info.h"
#include "tbl/tbl_peer_history.h"
#include "tbl/tbl_contribution_history.h"

class DbStaticsStorage
{
    Q_DISABLE_COPY_MOVE(DbStaticsStorage)
public:
    static DbStaticsStorage &instance();
    static const Path &dbPath();
public:
    DB::TblTorrentInfo& getTorrentInfoTable();
    DB::TblTorrentHistory& getTorrentHistoryTable();
    DB::TblPeerInfo& getPeerInfoTable();
    DB::TblPeerHistory& getPeerHistoryTable();
    DB::TblContributionHistory& getContributionHistoryTable();
private:
    explicit DbStaticsStorage();
    virtual ~DbStaticsStorage();
    
private:
    const Path m_dbPath;
    QSqlDatabase m_db;
};