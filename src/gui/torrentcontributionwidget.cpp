#include "torrentContributionwidget.h"
#include <QStringList>
#include <QHash>
#include "base/statics/statics.h"
#include "base/logger.h"
#include "base/utils/misc.h"

using namespace Qt::Literals::StringLiterals;

TorrentContributionModel::TorrentContributionModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    StaticsController& controller = StaticsController::instance();
    connect(this, &TorrentContributionModel::requestTorrentContribution, &controller, &StaticsController::onRequestTorrentContribution);
    connect(&controller, &StaticsController::getTorrentContribution, this, &TorrentContributionModel::onGetTorrentContribution);
    m_lastTorrent = nullptr;
}
void TorrentContributionModel::setTorrent(BitTorrent::Torrent *const torrent)
{
    if (torrent != m_lastTorrent)
    {
        m_records.clear();
        m_lastTorrent = torrent;
    }
    //ChangeTorrent(torrent);
}
void TorrentContributionModel::refreshContribution()
{
    if (!m_lastTorrent) return;
    emit requestTorrentContribution(m_lastTorrent);
}

QModelIndex TorrentContributionModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        // Top-level: DistinctRecord
        if (row >= 0 && row < static_cast<int>(m_records.size()))
            return createIndex(row, column, -1);
    } else if (parent.internalId() == static_cast<quintptr>(-1)) {
        // Child: PeriodRecord
        int parentRow = parent.row();
        if (parentRow >= 0 && parentRow < static_cast<int>(m_records.size())) {
            const auto &rec = m_records[parentRow];
            if (row >= 0 && row < static_cast<int>(rec.records.size()))
                return createIndex(row, column, parentRow);
        }
    }
    return QModelIndex();
}

QModelIndex TorrentContributionModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return QModelIndex();
    int parentRow = static_cast<int>(child.internalId());
    if (parentRow == -1)
        return QModelIndex();
    return createIndex(parentRow, 0, -1);
}

int TorrentContributionModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return static_cast<int>(m_records.size());
    if (parent.internalId() == static_cast<quintptr>(-1)) {
        int parentRow = parent.row();
        if (parentRow >= 0 && parentRow < static_cast<int>(m_records.size()))
            return static_cast<int>(m_records[parentRow].records.size());
    }
    return 0;
}

int TorrentContributionModel::columnCount(const QModelIndex &) const
{
    return 5; // peerId, uploadBytes, downloadBytes, startTime, endTime
}

QVariant TorrentContributionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    if (index.internalId() == static_cast<quintptr>(-1)) {
        // DistinctRecord
        const auto &rec = m_records[index.row()];
        switch (index.column()) {
        case 0: return rec.peerIp;
        case 1: return Utils::Misc::friendlyUnit(rec.uploadBytes);
        case 2: return Utils::Misc::friendlyUnit(rec.downloadBytes);
        case 3: return rec.startTime.toString(u"yyyy-MM-dd HH:mm:ss"_s);
        case 4: return rec.endTime.toString(u"yyyy-MM-dd HH:mm:ss"_s);
        }
    } else {
        // PeriodRecord
        const auto &rec = m_records[index.internalId()];
        const auto &period = rec.records[index.row()];
        switch (index.column()) {
        case 0: return QStringLiteral("");
        case 1: return Utils::Misc::friendlyUnit(period.uploadBytes);
        case 2: return Utils::Misc::friendlyUnit(period.downloadBytes);
        case 3: return period.startTime.toString(u"yyyy-MM-dd HH:mm:ss"_s);
        case 4: return period.endTime.toString(u"yyyy-MM-dd HH:mm:ss"_s);
        }
    }
    return QVariant();
}

QVariant TorrentContributionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        static QStringList headers = { tr("Peer IP"), tr("Upload Bytes"), tr("Download Bytes"), tr("Start Time"), tr("End Time") };
        if (section >= 0 && section < headers.size())
            return headers[section];
    }
    return QVariant();
}

// Add this to enable sorting in the view
Qt::ItemFlags TorrentContributionModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    if (index.isValid())
        return defaultFlags | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return defaultFlags;
}

void TorrentContributionModel::sort(int column, Qt::SortOrder order)
{
    // Only sort top-level (DistinctRecord) rows
    std::sort(m_records.begin(), m_records.end(), [column, order](const DistinctRecord &a, const DistinctRecord &b) {
        switch (column) {
        case 0: // Peer IP
            return order == Qt::AscendingOrder ? a.peerIp < b.peerIp : a.peerIp > b.peerIp;
        case 1: // Upload Bytes
            return order == Qt::AscendingOrder ? a.uploadBytes < b.uploadBytes : a.uploadBytes > b.uploadBytes;
        case 2: // Download Bytes
            return order == Qt::AscendingOrder ? a.downloadBytes < b.downloadBytes : a.downloadBytes > b.downloadBytes;
        case 3: // Start Time
            return order == Qt::AscendingOrder ? a.startTime < b.startTime : a.startTime > b.startTime;
        case 4: // End Time
            return order == Qt::AscendingOrder ? a.endTime < b.endTime : a.endTime > b.endTime;
        default:
            return false;
        }
    });
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void TorrentContributionModel::onGetTorrentContribution(const BitTorrent::Torrent* torrent, const QList<DB::TblContributionHistory::ContributionHistory>& result)
{
    if (!torrent || torrent != m_lastTorrent) return;
    std::unordered_map<QString, DistinctRecord> distinctMap;
    for (const auto &row : result) {

        if (!distinctMap.contains(row.peerIp)) {
            DistinctRecord distinctRecord{row.peerIp, row.uploadBytes, row.downloadBytes, row.startTime, row.endTime, {{row.uploadBytes, row.downloadBytes, row.startTime, row.endTime}}};
            // Initialize the distinct record with the first entry
            distinctMap.insert({row.peerIp, distinctRecord});
        } else {
            DistinctRecord &distinctRecord = distinctMap[row.peerIp];
            distinctRecord.uploadBytes += row.uploadBytes;
            distinctRecord.downloadBytes += row.downloadBytes;
            distinctRecord.startTime = std::min(distinctRecord.startTime, row.startTime);
            distinctRecord.endTime = std::max(distinctRecord.endTime, row.endTime);
            distinctRecord.records.emplace_back(row.uploadBytes, row.downloadBytes, row.startTime, row.endTime);
        }
    }
    emit modelWillBeRefreshed();
    // Clear existing records and update with distinct records
    beginResetModel();
    QHash<QString, bool> alive;
    for(auto it = m_records.begin(); it != m_records.end();)
    {
        const QString &peerIp = it->peerIp;
        if (distinctMap.contains(peerIp)) {
            // Peer is still alive, update its record
            it->uploadBytes = distinctMap[peerIp].uploadBytes;
            it->downloadBytes = distinctMap[peerIp].downloadBytes;
            it->startTime = distinctMap[peerIp].startTime;
            it->endTime = distinctMap[peerIp].endTime;
            it->records = std::move(distinctMap[peerIp].records);
            alive.insert(peerIp, true);
            ++it; // Move to the next record
        }
        else {
            // Peer is no longer alive, remove it
            it = m_records.erase(it);
        }
    }
    for(auto & [peerIp, distinctRecord] : distinctMap)
    {
        if (!alive.contains(peerIp)) {
            // Peer is new or has no previous record, add it
            m_records.append(distinctRecord);
        }
    }
    endResetModel();
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    emit modelRefreshed();
}

TorrentContributionWidget::TorrentContributionWidget(QWidget *parent)
    : QTreeView(parent)
{
    m_model = new TorrentContributionModel(this);
    setModel(m_model);
    setSortingEnabled(true); // <-- Add this line
    connect(m_model, &TorrentContributionModel::modelWillBeRefreshed, this, &TorrentContributionWidget::saveExpandedState);
    connect(m_model, &TorrentContributionModel::modelRefreshed, this, &TorrentContributionWidget::loadExpandedState);
}

void TorrentContributionWidget::setTorrent(BitTorrent::Torrent *const torrent)
{
    if (m_model) {
        m_model->setTorrent(torrent);
    }
}

void TorrentContributionWidget::refreshContribution()
{
    if (m_model) {
        m_model->refreshContribution();
    }
}

void TorrentContributionWidget::saveExpandedState()
{
    // Save expanded state of peers
    m_expandedPeers.clear();
    for (int row = 0; row < model()->rowCount(); ++row) 
    {
        QModelIndex idx = model()->index(row, 0);
        if (isExpanded(idx))
        {
            m_expandedPeers.insert(model()->data(idx, Qt::DisplayRole).toString());
        }
    }
    // Store expandedPeers in a suitable place, e.g., settings or member variable
}

void TorrentContributionWidget::loadExpandedState()
{
    // Load expanded state of peers
    for (const QString &peerIp : m_expandedPeers) {
        for (int row = 0; row < model()->rowCount(); ++row) {
            QModelIndex idx = model()->index(row, 0);
            if (model()->data(idx, Qt::DisplayRole).toString() == peerIp) {
                setExpanded(idx, true);
                break;
            }
        }
    }
}