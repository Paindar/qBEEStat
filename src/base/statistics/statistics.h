#pragma once

#include <QObject>
#include <thread>
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include "dbstatisticsstorage.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/infohash.h"

class StatisticsController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StatisticsController)
public:
    // Enum to represent the state of the controller
    enum class State
    {
        Running,
        Stopped
    };
public:
    // Static methods to access the controller instance
    static StatisticsController& instance()
    {
        static StatisticsController instance;
        return instance;
    }
    void start(); // Start the controller
    void stop(); // Stop the controller
    bool isRunning() const; // Check if the controller is running
    void setInterval(int interval); // Set the interval for the controller
    void setDecayInterval(int decayInterval); // Set the decay interval for peers
private:
    explicit StatisticsController() = default; // Constructor
    virtual ~StatisticsController() = default;
    bool subscribeAlert(); // Subscribe to alerts
    bool unsubscribeAlert(); // Unsubscribe from alerts
    void runAction(); // Run method for the thread
private:
    enum AlertType
    {
        TorrentAdded,
        TorrentFinished,
        TorrentRemoved,
        TorrentStarted,
        TorrentStopped,


        TorrentCollectData, // Collect data for the torrent
        TorrentContributionRequested, // Request contribution data for the torrent
    };
    struct Event
    {
        const AlertType type;
        const BitTorrent::Torrent* torrent;
        std::shared_ptr<BitTorrent::InfoHash::WrappedType> infoHash;
    };
    std::thread m_workThread; // Thread for the controller
    std::atomic<State> m_enStatus = State::Stopped; // Flag to indicate if the controller is running
    int m_interval = 1000; // Interval for the controller in milliseconds
    int m_decayInterval = 10; // Decay interval for peers in seconds
    std::shared_ptr<DbStatisticsStorage> m_pDbStatisticsStorage; // Pointer to the database storage
    std::mutex m_mutex; // Mutex for thread safety
    std::queue<Event> m_alertQueue; // Queue for alerts
private:
    // private tool methods
    QString getHashId(const BitTorrent::InfoHash &torrentID) const;
    // Private methods to handle different events
    void handleStatisticsStop();
    void handleTorrentAdded(const BitTorrent::Torrent* torrent);
    void handleTorrentFinished(const BitTorrent::Torrent* torrent);
    void handleTorrentRemove(const std::shared_ptr<BitTorrent::InfoHash::WrappedType> torrentID);
    void handleTorrentStart(const BitTorrent::Torrent* torrent);
    void handleTorrentStop(const BitTorrent::Torrent* torrent);
    bool handleCollectData(); // Collect data for the controller
    bool handleTorrentContributionRequested(const BitTorrent::Torrent* torrent);
    bool handlePeerJoined(const QString& torrentHash, const BitTorrent::PeerInfo&peerInfo);
    bool handlePeerUpdated(const QString& torrentHash, const BitTorrent::PeerInfo&peerInfo);
    bool handlePeerDecay(const QString& torrentHash, const QString& peerIp);
    bool handlePeerLeaving(const QString& torrentHash,const QString& peerIp);
signals:
    void getTorrentContribution(const BitTorrent::Torrent* torrent, const QList<DB::TblContributionHistory::ContributionHistory>& result); // Signal to request contribution data for the torrent
public slots:
    void onTorrentAdded(BitTorrent::Torrent* torrent);
    void onTorrentFinished(BitTorrent::Torrent* torrent);
    void onTorrentRemove(const BitTorrent::InfoHash::WrappedType& infoHash);
    void onTorrentStart(BitTorrent::Torrent* torrent);
    void onTorrentStop(BitTorrent::Torrent* torrent);

    void onRequestTorrentContribution(const BitTorrent::Torrent* torrent);
};
