#include "statistics.h"

#include <QString>
#include <QVector>
#include <ctime>
#include <chrono>
#include <unordered_map>
#include "base/bittorrent/sessionimpl.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/peeraddress.h"
#include "base/logger.h"
#include "tbl/tbl_peer_history.h"
#include "tbl/tbl_peer_info.h"
#include "tbl/tbl_torrent_history.h"
#include "tbl/tbl_torrent_info.h"
#include "tbl/tbl_contribution_history.h"

#include <QElapsedTimer>

using namespace Qt::Literals::StringLiterals;
void StatisticsController::start()
{
    if (m_enStatus.load()==State::Running) return;
    subscribeAlert();
    m_enStatus.store(State::Running);
    LogMsg(u"StatisticsController started"_s);
    m_workThread = std::thread(&StatisticsController::runAction, this);
}
void StatisticsController::stop()
{
    if (m_enStatus.load()==State::Stopped) return;
    unsubscribeAlert();
    m_enStatus.store(State::Stopped);
    handleStatisticsStop();
    m_workThread.join();
}
bool StatisticsController::isRunning() const
{
    return m_enStatus.load() == State::Running;
}
void StatisticsController::setInterval(int interval)
{
    m_interval = interval;
}
bool StatisticsController::subscribeAlert()
{
    BitTorrent::Session *session = BitTorrent::SessionImpl::instance();
    if (!session) return false; // Check if session is valid
    if (!connect(session, &BitTorrent::Session::torrentAdded, this, &StatisticsController::onTorrentAdded))
    {
        return false; // Check if connection was successful
    }
    if (!connect(session, &BitTorrent::Session::torrentFinished, this, &StatisticsController::onTorrentFinished))
    {
        return false; // Check if connection was successful
    }
    if(!connect(session, &BitTorrent::Session::torrentRemoved, this, &StatisticsController::onTorrentRemove))
    {
        return false; // Check if connection was successful
    }
    if (!connect(session, &BitTorrent::Session::torrentStarted, this, &StatisticsController::onTorrentStart))
    {
        return false; // Check if connection was successful
    }
    if (!connect(session, &BitTorrent::Session::torrentStopped, this, &StatisticsController::onTorrentStop))
    {
        return false; // Check if connection was successful
    }
    return true; // Return true if subscription was successful, false otherwise
}
bool StatisticsController::unsubscribeAlert()
{
    // Unsubscribe from alerts from the session
    BitTorrent::Session *session = BitTorrent::SessionImpl::instance();
    if (!session) return false; // Check if session is valid
    bool bRet = true;
    if (!disconnect(session, &BitTorrent::Session::torrentAdded, this, &StatisticsController::onTorrentAdded))
    {
        bRet = false; // Check if disconnection was successful
    }
    if (!disconnect(session, &BitTorrent::Session::torrentFinished, this, &StatisticsController::onTorrentFinished))
    {
        bRet = false; // Check if disconnection was successful
    }
    if (!disconnect(session, &BitTorrent::Session::torrentRemoved, this, &StatisticsController::onTorrentRemove))
    {
        bRet = false; // Check if disconnection was successful
    }
    if (!disconnect(session, &BitTorrent::Session::torrentStarted, this, &StatisticsController::onTorrentStart))
    {
        bRet = false; // Check if disconnection was successful
    }
    if (!disconnect(session, &BitTorrent::Session::torrentStopped, this, &StatisticsController::onTorrentStop))
    {
        bRet = false; // Check if disconnection was successful
    }

    return bRet; // Return true if unsubscription was successful, false otherwise
}

void StatisticsController::runAction()
{
    auto startTime = std::chrono::system_clock::now();
    auto endTime = startTime + std::chrono::milliseconds(m_interval);
    // Start the work thread
    LogMsg(u"StatisticsController work thread started"_s, Log::INFO);

    while (m_enStatus.load() == State::Running)
    {
        auto currentTime = std::chrono::system_clock::now();
        while(m_enStatus.load() == State::Running && currentTime < endTime)
        {
            std::unique_lock lock(this->m_mutex);
            if (this->m_alertQueue.empty())
                break; // Exit the loop if there are no alerts to process
            Event evt = this->m_alertQueue.front();
            this->m_alertQueue.pop();
            lock.unlock();
            switch (evt.type)
            {
                case AlertType::TorrentAdded:
                    handleTorrentAdded(evt.torrent);
                    break;
                case AlertType::TorrentFinished:
                    handleTorrentFinished(evt.torrent);
                    break;
                case AlertType::TorrentRemoved:
                    handleTorrentRemove(evt.infoHash);
                    break;
                case AlertType::TorrentStarted:
                    handleTorrentStart(evt.torrent);
                    break;
                case AlertType::TorrentStopped:
                    handleTorrentStop(evt.torrent);
                    break;
                case AlertType::TorrentCollectData:
                    handleCollectData();
                    break;
                case AlertType::TorrentContributionRequested:
                    handleTorrentContributionRequested(evt.torrent);
                    break;
            }
            currentTime = std::chrono::system_clock::now();
        }
        // Check if the current time is less than the end time
        currentTime = std::chrono::system_clock::now();
        if (currentTime < endTime)
        {
            // Sleep for a short duration to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - currentTime));
        }
        else
        {
            // Push event
            {
                Event evt
                {
                    AlertType::TorrentCollectData,
                    nullptr, // No torrent associated with this event
                    nullptr // Empty info hash
                };
                std::scoped_lock lock(this->m_mutex);
                this->m_alertQueue.push(evt);
            }
            // Reset the start time and end time for the next iteration
            startTime = currentTime;
            endTime = startTime + std::chrono::milliseconds(m_interval);
        }
    }
    qDebug()<<u"StatisticsController work thread stopped"_s;
}
QString StatisticsController::getHashId(const BitTorrent::InfoHash &infoHash) const
{
#ifdef QBT_USES_LIBTORRENT2
    if (infoHash.v2().isValid())
    {
        return infoHash.v2().toString();
    }
#else
    if (infoHash.v1().isValid())
    {
        return infoHash.v1().toString();
    }
#endif
    return infoHash.toTorrentID().toString(); // Return the default hash ID as a string with 160 bits
}
void StatisticsController::handleStatisticsStop()
{
    LogMsg(u"StatisticsController stopped"_s, Log::INFO);
    // Archive all torrents info into history
    DB::TblTorrentInfo& tblTorrentInfo = DbStatisticsStorage::instance().getTorrentInfoTable();
    DB::TblTorrentHistory& tblTorrentHistory = DbStatisticsStorage::instance().getTorrentHistoryTable();
    QList<DB::TblTorrentInfo::TorrentInfo> torrents;
    if (!tblTorrentInfo.getAllTorrents(torrents))
    {
        LogMsg(u"Failed to get all torrents from the database."_s);
        return; // Return false if there was an error
    }
    for (const DB::TblTorrentInfo::TorrentInfo& torrent : torrents)
    {
        const QString& torrentHash = torrent.hashId;
        if (torrentHash.isEmpty()) continue; // Skip if torrent hash is empty
        // Archive torrent info
        if (!tblTorrentHistory.insertTorrentHistory(torrentHash, u"stopped"_s,
                QString::number(torrent.size) + u"||"_s + torrent.createTime.toString() + u"||"_s + torrent.finishTime.toString()))
        {
            LogMsg(u"Failed to add torrent history to the database, hash: %1, size: %2, create time: %3, finish time: %4."_s
                   .arg(torrentHash, QString::number(torrent.size), torrent.createTime.toString(), torrent.finishTime.toString()), Log::CRITICAL);
            return;
        }
    }

    // Archive all peer info into history
    DB::TblPeerInfo& tblPeerInfo = DbStatisticsStorage::instance().getPeerInfoTable();
    DB::TblPeerHistory& tblPeerHistory = DbStatisticsStorage::instance().getPeerHistoryTable();
    DB::TblContributionHistory& tblContributionHistory = DbStatisticsStorage::instance().getContributionHistoryTable();
    QList<DB::TblPeerInfo::PeerInfo> peerInfos;
    if (!tblPeerInfo.getAllPeers(peerInfos))
    {
        LogMsg(u"Failed to get all peers from the database."_s, Log::CRITICAL);
        return; // Return false if there was an error
    }
    for (const DB::TblPeerInfo::PeerInfo& peerInfo : peerInfos)
    {
        if (peerInfo.peerIp.isEmpty()) continue; // Skip if peer ID is empty
        if (peerInfo.torrentHashId.isEmpty()) continue; // Skip if torrent hash is empty
        // Archive peer info and history
        if (!tblContributionHistory.insertContributionHistory(
                peerInfo.torrentHashId,
                peerInfo.peerIp,
                peerInfo.uploadBytes,
                peerInfo.downloadBytes,
                peerInfo.startTime,
                QDateTime::currentDateTime()))
        {
            LogMsg(u"Failed to add contribution history to the database, peer IP: %1, torrent hash: %2, upload bytes: %3, download bytes: %4, start time: %5."_s
                   .arg(peerInfo.peerIp, peerInfo.torrentHashId, QString::number(peerInfo.uploadBytes), QString::number(peerInfo.downloadBytes), peerInfo.startTime.toString()), Log::CRITICAL);
            return;
        }
        QList<QString> args;
        // Args: hashid name
        {
            QString addr = peerInfo.peerIp;
            addr.replace(u"|"_s, u"\\|"_s);
            args.append(addr);
        }
        if (!tblPeerHistory.insertTorrentHistory(peerInfo.peerIp, peerInfo.torrentHashId, u"stopped"_s, args.join(u"||"_s)))
        {
            LogMsg(u"Failed to add peer history to the database, peer IP: %1, torrent hash: %2, event: stopped."_s
                   .arg(peerInfo.peerIp, peerInfo.torrentHashId), Log::CRITICAL);
            return;
        }
    }


}
void StatisticsController::handleTorrentAdded(const BitTorrent::Torrent *torrent)
{
    if (!torrent) return; // Check if torrent is valid
    LogMsg(u"Torrent added:"_s + torrent->name());
    QString error;
    
    QString torrentHash = getHashId(torrent->infoHash());
    // Add torrent info to the database
    DB::TblTorrentInfo& tblTorrentInfo = DbStatisticsStorage::instance().getTorrentInfoTable();
    if (!tblTorrentInfo.insertTorrentInfo(torrentHash, torrent->totalSize(), torrent->addedTime(), QDateTime()))
    {
        LogMsg(u"Failed to add torrent info to the database, hash: %1, size: %2, added time: %3."_s
               .arg(torrentHash, QString::number(torrent->totalSize()), torrent->addedTime().toString()), Log::CRITICAL);
        return;
    }
    // Add torrent history to the database
    DB::TblTorrentHistory& tblTorrentHistory = DbStatisticsStorage::instance().getTorrentHistoryTable();
    QList<QString> args;
    {
        // Args: hashid name savepath size
        args.append(torrent->name().replace(u"|"_s, u"\\|"_s));
        args.append(torrent->savePath().toString().replace(u"|"_s, u"\\|"_s));
        args.append(QString::number(torrent->totalSize()));
    }
    if (!tblTorrentHistory.insertTorrentHistory(torrentHash, u"added"_s, args.join(u"||"_s)))
    {
        LogMsg(u"Failed to add torrent history to the database, hash: %1, name: %2, save path: %3, size: %4."_s
               .arg(torrentHash, torrent->name(), torrent->savePath().toString(), QString::number(torrent->totalSize())), Log::CRITICAL);
        return;
    }
    
}
void StatisticsController::handleTorrentFinished(const BitTorrent::Torrent* torrent)
{
    if (!torrent) return; // Check if torrent is valid
    qDebug() << "Torrent finished:" << torrent->name();
    QString torrentHash = getHashId(torrent->infoHash());
    // Update torrent info in the database
    DB::TblTorrentInfo& tblTorrentInfo = DbStatisticsStorage::instance().getTorrentInfoTable();
    if(!tblTorrentInfo.updateTorrentInfo(torrentHash, torrent->completedTime()))
    {
        LogMsg(u"Failed to update torrent info in the database, hash: %1, completed time: %2."_s
               .arg(torrentHash, torrent->completedTime().toString()), Log::CRITICAL);
        return;
    }
    // Add torrent history to the database
    DB::TblTorrentHistory& tblTorrentHistory = DbStatisticsStorage::instance().getTorrentHistoryTable();
    QList<QString> args;
    // Args: hashid name
    args.append(torrentHash.replace(u"|"_s, u"\\|"_s));
    args.append(torrent->name().replace(u"|"_s, u"\\|"_s));
    if(!tblTorrentHistory.insertTorrentHistory(torrentHash, u"finished"_s, args.join(u"||"_s)))
    {
        LogMsg(u"Failed to add torrent history to the database, hash: %1, name: %2."_s
               .arg(torrentHash, torrent->name()), Log::CRITICAL);
        return;
    }
}
void StatisticsController::handleTorrentRemove(const std::shared_ptr<BitTorrent::InfoHash::WrappedType> infoHash)
{
    if (!infoHash) return; // Check if torrent is valid
    QString torrentHash = getHashId(*infoHash);
    //qDebug() << "Torrent removed:" << infoHash->
    // Remove torrent info from the database
    DB::TblTorrentInfo& tblTorrentInfo = DbStatisticsStorage::instance().getTorrentInfoTable();
    if(!tblTorrentInfo.deleteTorrentInfo(torrentHash))
    {
        LogMsg(u"Failed to delete torrent info from the database, hash: %1."_s
               .arg(torrentHash), Log::CRITICAL);
        return;
    }
    // Add torrent history to the database
    DB::TblTorrentHistory& tblTorrentHistory = DbStatisticsStorage::instance().getTorrentHistoryTable();
    QList<QString> args;
    // Args: hashid name
    args.append(torrentHash.replace(u"|"_s, u"\\|"_s));
    args.append(u""_s);
    if(!tblTorrentHistory.insertTorrentHistory(torrentHash, u"removed"_s, args.join(u"||"_s)))
    {
        LogMsg(u"Failed to add torrent history to the database, hash: %1."_s
               .arg(torrentHash), Log::CRITICAL);
        return;
    }
    // Archive peer info and history
    DB::TblPeerInfo& tblPeerInfo = DbStatisticsStorage::instance().getPeerInfoTable();
    QList<DB::TblPeerInfo::PeerInfo> peerInfos;
    if (!tblPeerInfo.getPeers(torrentHash, peerInfos))
    {
        LogMsg(u"Failed to get peer info from the database, hash: %1."_s
               .arg(torrentHash), Log::CRITICAL);
        return;
    }
    // Peer is no longer alive, update peer info
    DB::TblContributionHistory& tblContributionHistory = DbStatisticsStorage::instance().getContributionHistoryTable();
    DB::TblPeerHistory& tblPeerHistory = DbStatisticsStorage::instance().getPeerHistoryTable();
    for(const DB::TblPeerInfo::PeerInfo& peerInfo : peerInfos)
    {
        // Archive peer info and history
        //handlePeerLeaving(torrentHash, peerId, peerInfo);
        if (!tblContributionHistory.insertContributionHistory(torrentHash, peerInfo.peerIp,
                peerInfo.uploadBytes,
                peerInfo.downloadBytes,
                peerInfo.startTime,
                QDateTime::currentDateTime()))
        {
            LogMsg(u"Failed to add contribution history to the database, peer IP: %1, torrent hash: %2, upload bytes: %3, download bytes: %4, start time: %5."_s
                   .arg(peerInfo.peerIp, torrentHash, QString::number(peerInfo.uploadBytes), QString::number(peerInfo.downloadBytes), peerInfo.startTime.toString()), Log::CRITICAL);
            continue;
        }
        if(!tblPeerInfo.deletePeerInfo({torrentHash, peerInfo.peerIp}))
        {
            LogMsg(u"Failed to delete peer info from the database, peer IP: %1, torrent hash: %2."_s
                   .arg(peerInfo.peerIp, torrentHash), Log::CRITICAL);
            continue;
        }
        QList<QString> args;
        // Args: hashid name
        {
            QString pid = peerInfo.peerIp;
            pid.replace(u"|"_s, u"\\|"_s);
            args.append(pid);
        }
        if (!tblPeerHistory.insertTorrentHistory(peerInfo.peerIp, peerInfo.torrentHashId, u"left"_s, args.join(u"||"_s)))
        {
            LogMsg(u"Failed to add peer history to the database, peer IP: %1, torrent hash: %2, event: left."_s
                   .arg(peerInfo.peerIp, torrentHash), Log::CRITICAL);
            continue;
        }

    }
}
void StatisticsController::handleTorrentStart(const BitTorrent::Torrent* torrent)
{
    if (!torrent) return; // Check if torrent is valid
    qDebug() << "Torrent started:" << torrent->name();
    QString error;
    QString torrentHash = getHashId(torrent->infoHash());
    // Add torrent history to the database
    DB::TblTorrentHistory& tblTorrentHistory = DbStatisticsStorage::instance().getTorrentHistoryTable();
    QList<QString> args;
    // Args: hashid name
    args.append(torrent->name().replace(u"|"_s, u"\\|"_s));
    if(!tblTorrentHistory.insertTorrentHistory(torrentHash, u"started"_s, args.join(u"||"_s)))
    {
        LogMsg(u"Failed to add torrent history to the database, hash: %1, name: %2."_s
               .arg(torrentHash, torrent->name()), Log::CRITICAL);
        return;
    }
}

void StatisticsController::handleTorrentStop(const BitTorrent::Torrent* torrent)
{
    if (!torrent) return; // Check if torrent is valid
    qDebug() << "Torrent stopped:" << torrent->name();
    QString error;
    QString torrentHash = getHashId(torrent->infoHash());
    // Add torrent history to the database
    DB::TblTorrentHistory& tblTorrentHistory = DbStatisticsStorage::instance().getTorrentHistoryTable();
    QList<QString> args;
    // Args: hashid name
    args.append(torrent->name().replace(u"|"_s, u"\\|"_s));
    if(!tblTorrentHistory.insertTorrentHistory(torrentHash, u"stopped"_s, args.join(u"||"_s)))
    {
        LogMsg(u"Failed to add torrent history to the database, hash: %1, name: %2."_s
               .arg(torrentHash, torrent->name()), Log::CRITICAL);
        return;
    }
}
bool StatisticsController::handleCollectData()
{
    // Collect data from the database and perform any necessary operations
    // For example, you can query the database and process the results
    // ...
    DbStatisticsStorage &db = DbStatisticsStorage::instance();
    BitTorrent::SessionImpl *session = static_cast<BitTorrent::SessionImpl*>(BitTorrent::SessionImpl::instance());
    if (!session) return false; // Check if session is valid
    DB::TblPeerHistory& tblPeerHistory = db.getPeerHistoryTable();
    DB::TblPeerInfo& tblPeerInfo = db.getPeerInfoTable();
    DB::TblContributionHistory& tblContributionHistory = db.getContributionHistoryTable();
    QString error;
    for(const BitTorrent::Torrent* torrent : session->torrents())
    {
        if (!torrent) continue; // Check if torrent is valid
        QString torrentHash = getHashId(torrent->infoHash());
        QList<DB::TblPeerInfo::PeerInfo> peerInfos;
        if (!tblPeerInfo.getPeers(torrentHash, peerInfos))
        {
            LogMsg(u"Failed to get peer info from the database for torrent hash: "_s + torrentHash, Log::CRITICAL);
            continue;
        }
        std::unordered_map<QString, int> peer2Index;
        std::unordered_map<QString, bool> mapPeerAlive;
        for (qsizetype i = 0; i < peerInfos.size(); i++)
        {
            const QString& peerIp = peerInfos[i].peerIp;
            if (peerIp.isEmpty()) continue; // Skip if peer ID is empty
            peer2Index[peerIp] = i; // peerId is at index 0
            mapPeerAlive[peerIp] = false; // Initialize alive status for each peer
        }
        const QList<BitTorrent::PeerInfo> peerInfosList = torrent->peers();
        for(const BitTorrent::PeerInfo &peerInfo : peerInfosList)
        {
            const QString& peerId = peerInfo.peerId();
            if (peerId.isEmpty()) continue; // Skip if peer ID is empty
            const QString& peerIp = peerInfo.address().toString();
            if (peerIp.isEmpty()) continue; // Skip if peer ID is empty
            if (peerInfo.totalUpload() == 0 && peerInfo.totalDownload() == 0)
            {
                // Skip if total upload and download are both zero
                //LogMsg(u"Peer info has zero upload and download, skipping: "_s + peerId + u" from "_s + peerInfo.address().toString());
                continue;
            }
            auto it = peer2Index.find(peerIp);
            if (it == peer2Index.end())
            {
                // New peer joined
                if (!tblPeerInfo.insertPeerInfo({torrentHash, peerIp}, peerId , peerInfo.totalUpload(), peerInfo.totalDownload(), QDateTime::currentDateTime()))
                {
                    LogMsg(u"Failed to insert peer info into the database, torrent hash: %1, peerIp: %2, peerId: %3, upload: %4, download: %5."_s
                           .arg(torrentHash, peerIp, peerId, QString::number(peerInfo.totalUpload()), QString::number(peerInfo.totalDownload())), Log::CRITICAL);
                    continue;
                }
                QList<QString> args;
                // Args: hashid name
                {
                    QString pid = peerId;
                    pid.replace(u"|"_s, u"\\|"_s);
                    args.append(pid);
                }
                if (!tblPeerHistory.insertTorrentHistory(peerIp, torrentHash, u"joined"_s, args.join(u"||"_s)))
                {
                    LogMsg(u"Failed to insert peer history into the database, peer IP: %1, torrent hash: %2, event: joined."_s
                           .arg(peerIp, torrentHash), Log::CRITICAL);
                    continue;
                }
            }
            else
            {
                // Update existing peer info
                mapPeerAlive[peerIp] = true; // Mark peer as alive
                if (!tblPeerInfo.updateTraffic({torrentHash, peerIp}, peerInfo.totalUpload(), peerInfo.totalDownload()))
                {
                    qDebug() << "Failed to update peer info in the database:" << error;
                    continue;
                }
            }
        }
        // Check for peers that are no longer alive
        for (const auto& [peerIp, alive] : mapPeerAlive)
        {
            if (!alive)
            {
                const int index = peer2Index[peerIp];
                const DB::TblPeerInfo::PeerInfo &peerInfo = peerInfos[index];
                // Peer is no longer alive, update peer info
                if (!tblContributionHistory.insertContributionHistory(torrentHash, peerIp,
                    //peerInfo.peerId,
                    peerInfo.uploadBytes,
                    peerInfo.downloadBytes,
                    peerInfo.startTime,
                    QDateTime::currentDateTime()))
                {
                    LogMsg(u"Failed to insert contribution history into the database, peer IP: %1, torrent hash: %2, upload bytes: %3, download bytes: %4, start time: %5."_s
                           .arg(peerIp, torrentHash, QString::number(peerInfo.uploadBytes), QString::number(peerInfo.downloadBytes), peerInfo.startTime.toString()), Log::CRITICAL);
                    continue;
                }
                if(!tblPeerInfo.deletePeerInfo({torrentHash, peerIp}))
                {
                    LogMsg(u"Failed to delete peer info from the database, peer IP: %1, torrent hash: %2."_s
                           .arg(peerIp, torrentHash), Log::CRITICAL);
                    continue;
                }
                QList<QString> args;
                // Args: hashid name
                {
                    QString pid = peerInfo.peerId;
                    pid.replace(u"|"_s, u"\\|"_s);
                    args.append(pid);
                }
                if (!tblPeerHistory.insertTorrentHistory(peerIp, torrentHash, u"left"_s, args.join(u"||"_s)))
                {
                    LogMsg(u"Failed to insert peer history into the database, peer IP: %1, torrent hash: %2, event: left."_s
                           .arg(peerIp, torrentHash), Log::CRITICAL);
                    continue;
                }
            }
        }
    }
    
    return true; // Return true if data collection was successful, false otherwise
}

bool StatisticsController::handleTorrentContributionRequested(const BitTorrent::Torrent* torrent)
{
    if (!torrent) return false; // Check if torrent is valid
    DbStatisticsStorage &db = DbStatisticsStorage::instance();
    QString torrentHash = getHashId(torrent->infoHash());
    DB::TblContributionHistory& tblContributionHistory = db.getContributionHistoryTable();
    QList<DB::TblContributionHistory::ContributionHistory> result;

    // QElapsedTimer timer;
    // timer.start();

    if (!tblContributionHistory.getContributionHistory(torrentHash, result))
    {
        LogMsg(u"Failed to get contribution history from the database for torrent hash: "_s + torrentHash, Log::CRITICAL);
        return false; // Return false if there was an error
    }

    // qint64 elapsedMs = timer.elapsed();
    // LogMsg(u"getContributionHistory time cost: "_s + QString::number(elapsedMs) + u" ms", Log::INFO);

    emit getTorrentContribution(torrent, result);
    return true; // Return true if contribution data was successfully retrieved
}

void StatisticsController::onTorrentAdded(BitTorrent::Torrent *torrent)
{
    if (!torrent) return; // Check if torrent is valid
    std::scoped_lock lock(this->m_mutex);
    this->m_alertQueue.push({
        AlertType::TorrentAdded,
        torrent,
        nullptr
    });
}
void StatisticsController::onTorrentFinished(BitTorrent::Torrent *torrent)
{
    if (!torrent) return; // Check if torrent is valid
    std::scoped_lock lock(this->m_mutex);
    this->m_alertQueue.push({
        AlertType::TorrentAdded,
        torrent,
        nullptr
    });
}
void StatisticsController::onTorrentRemove(const BitTorrent::InfoHash::WrappedType &torrentID)
{
    std::scoped_lock lock(this->m_mutex);
    this->m_alertQueue.push({
        AlertType::TorrentRemoved,
        nullptr,
        std::make_shared<BitTorrent::InfoHash::WrappedType>(torrentID)
    });
}

void StatisticsController::onTorrentStart(BitTorrent::Torrent *torrent)
{
    if (!torrent) return; // Check if torrent is valid
    std::scoped_lock lock(this->m_mutex);
    this->m_alertQueue.push({
        AlertType::TorrentStarted,
        torrent,
        nullptr
    });
}
void StatisticsController::onTorrentStop(BitTorrent::Torrent *torrent)
{
    if (!torrent) return; // Check if torrent is valid
    std::scoped_lock lock(this->m_mutex);
    this->m_alertQueue.push({
        AlertType::TorrentStopped,
        torrent,
        nullptr
    });
}
void StatisticsController::onRequestTorrentContribution(const BitTorrent::Torrent *torrent)
{
    if (!torrent) return; // Check if torrent is valid
    std::scoped_lock lock(this->m_mutex);
    this->m_alertQueue.push({
        AlertType::TorrentContributionRequested,
        torrent,
        nullptr
    });
}
