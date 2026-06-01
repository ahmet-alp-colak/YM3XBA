/**
 * @file PersistenceLogger.hpp
 * @brief Asynchronous database logging interface for signal records
 * @details Thread-safe persistence layer for SQLite/PostgreSQL
 * 
 * Features:
 * - Asynchronous logging (non-blocking)
 * - Thread-safe queue
 * - Batch insertion for performance
 * - SQLite support (easily extensible to PostgreSQL)
 * 
 * References:
 * - Section 4.2.5: Veri Kalıcılığı
 * - Pipeline Stage [18]: Database/Logging Katmanı
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef PERSISTENCE_LOGGER_HPP
#define PERSISTENCE_LOGGER_HPP

#include "DataStructures.hpp"
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>

// Forward declaration for SQLite (avoid including sqlite3.h in header)
struct sqlite3;

namespace CognitiveRF {

/**
 * @struct LoggerConfig
 * @brief Configuration for persistence logger
 */
struct LoggerConfig {
    std::string database_path = "signal_records.db";  ///< SQLite database file
    size_t batch_size = 100;                          ///< Records per batch insert
    int flush_interval_ms = 1000;                     ///< Auto-flush interval
    bool enable_wal_mode = true;                      ///< Write-Ahead Logging for performance
};

/**
 * @class PersistenceLogger
 * @brief Asynchronous logger for signal detection records
 *
 * Architecture:
 * - Producer: Main pipeline pushes DecodedSignal records
 * - Consumer: Background thread batches and writes to database
 * - Thread-safe queue for inter-thread communication
 *
 * Database Schema (Updated for Pipeline V1.1.0):
 * CREATE TABLE signal_records (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     timestamp_us INTEGER,
 *     frequency_hz REAL,
 *     bandwidth_hz REAL,
 *     rssi_dbm REAL,
 *     snr_db REAL,
 *     modulation TEXT,
 *     protocol TEXT,
 *     multiplexing TEXT,
 *     signal_class INTEGER,
 *     confidence REAL,
 *     fhss_suspected INTEGER,
 *     jamming_suspected INTEGER,
 *     aoa REAL,              -- Angle of Arrival (degrees) [NEW]
 *     target_lat REAL,       -- Target latitude [NEW]
 *     target_lon REAL,       -- Target longitude [NEW]
 *     payload_hex TEXT
 * );
 */
class PersistenceLogger {
public:
    /**
     * @brief Constructor
     * @param config Logger configuration
     */
    explicit PersistenceLogger(const LoggerConfig& config = LoggerConfig{});
    
    /**
     * @brief Destructor - flushes pending records and closes database
     */
    ~PersistenceLogger();
    
    /**
     * @brief Start logging thread
     * @return true if started successfully
     */
    bool start();
    
    /**
     * @brief Stop logging thread (flushes pending records)
     */
    void stop();
    
    /**
     * @brief Log signal detection record (non-blocking)
     * @param signal Decoded signal to log
     * @return true if queued successfully
     */
    bool logSignal(const DecodedSignal& signal);
    
    /**
     * @brief Flush pending records immediately
     */
    void flush();
    
    /**
     * @brief Get number of pending records in queue
     */
    size_t getPendingCount() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return record_queue_.size();
    }
    
    /**
     * @brief Get total number of records logged
     */
    uint64_t getTotalLogged() const { return total_logged_; }
    
    /**
     * @brief Check if logger is running
     */
    bool isRunning() const { return running_; }

private:
    // Configuration
    LoggerConfig config_;
    
    // Database handle
    sqlite3* db_;
    std::mutex db_mutex_;
    
    // Record queue
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<SignalRecord> record_queue_;
    
    // Threading
    std::unique_ptr<std::thread> logging_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> flush_requested_;
    
    // Statistics
    std::atomic<uint64_t> total_logged_;
    std::atomic<uint64_t> error_count_;
    
    /**
     * @brief Initialize database and create tables
     * @return true if successful
     */
    bool initializeDatabase();
    
    /**
     * @brief Main logging loop (runs in separate thread)
     */
    void loggingLoop();
    
    /**
     * @brief Write batch of records to database
     * @param records Records to write
     * @return Number of records successfully written
     */
    size_t writeBatch(const std::vector<SignalRecord>& records);
    
    /**
     * @brief Convert DecodedSignal to SignalRecord
     * @param signal Decoded signal
     * @return Database record
     */
    SignalRecord convertToRecord(const DecodedSignal& signal) const;
    
    /**
     * @brief Execute SQL statement
     * @param sql SQL statement
     * @return true if successful
     */
    bool executeSql(const std::string& sql);
};

/**
 * @class CSVLogger
 * @brief Simple CSV file logger (alternative to database)
 * 
 * Useful for:
 * - Quick debugging
 * - Data export
 * - Systems without SQLite
 */
class CSVLogger {
public:
    /**
     * @brief Constructor
     * @param filename CSV file path
     */
    explicit CSVLogger(const std::string& filename);
    
    /**
     * @brief Destructor - closes file
     */
    ~CSVLogger();
    
    /**
     * @brief Log signal to CSV
     * @param signal Decoded signal
     * @return true if successful
     */
    bool logSignal(const DecodedSignal& signal);
    
    /**
     * @brief Flush file buffer
     */
    void flush();

private:
    std::string filename_;
    std::ofstream file_;
    std::mutex file_mutex_;
    bool header_written_;
    
    /**
     * @brief Write CSV header
     */
    void writeHeader();
};

} // namespace CognitiveRF

#endif // PERSISTENCE_LOGGER_HPP
