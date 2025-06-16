#pragma once

#include <QTreeView>
#include <QAbstractItemModel>
#include <QList>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QDateTime>
#include "base/bittorrent/torrent.h"
#include "base/statics/tbl/tbl_contribution_history.h"

// Define PeriodRecord as needed
struct PeriodRecord {
    qlonglong uploadBytes;
    qlonglong downloadBytes;
    QDateTime startTime;
    QDateTime endTime;
};

struct DistinctRecord {
    QString peerIp;
    qlonglong uploadBytes;
    qlonglong downloadBytes;
    QDateTime startTime;
    QDateTime endTime;
    std::vector<PeriodRecord> records;
};

class TorrentContributionModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit TorrentContributionModel(QObject *parent = nullptr);

    void setTorrent(BitTorrent::Torrent *const torrent);
    void refreshContribution();
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
signals:
    void requestTorrentContribution(const BitTorrent::Torrent * torrent);
    void modelWillBeRefreshed();
    void modelRefreshed();
public slots:
    void onGetTorrentContribution(const BitTorrent::Torrent* torrent, const QList<DB::TblContributionHistory::ContributionHistory>& result);
private:
    QList<DistinctRecord> m_records;
    BitTorrent::Torrent *m_lastTorrent = nullptr;
};
//Widget->Model->statics->Model->Widget
class TorrentContributionWidget : public QTreeView
{
    Q_OBJECT
public:
    explicit TorrentContributionWidget(QWidget *parent = nullptr);

    void setTorrent(BitTorrent::Torrent *const torrent);
    void refreshContribution();
public slots:
    void saveExpandedState();
    void loadExpandedState();
private:
    TorrentContributionModel *m_model;
    QSet<QString> m_expandedPeers; // Store expanded peers to restore state
};