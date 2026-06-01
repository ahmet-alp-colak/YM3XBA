/**
 * @file PersistenceLogger.cpp
 * @brief Implementation of persistence logger
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "PersistenceLogger.hpp"
#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace CognitiveRF {

// ============================================================================
// PersistenceLogger Implementation
// ============================================================================

PersistenceLogger::PersistenceLogger(const LoggerConfig& config)
    : config_(config)
    , db_(nullptr)
    , running_(false)
    , flush_requested_(false)
    , total_logged_(0)
    , error_count_(0)
{
}

PersistenceLogger::~PersistenceLogger() {
    stop();
    
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool PersistenceLogger::start() {
    if (running_) {
        return true;  // Already running
    }
    
    // Initialize database
    if (!initializeDatabase()) {
        return false;
    }
    
    // Start logging thread
    running_ = true;
    logging_thread_ = std::make_unique<std::thread>(
        &PersistenceLogger::loggingLoop,
        this
    );
    
    return true;
}

void PersistenceLogger::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    queue_cv_.notify_all();
    
    if (logging_thread_ && logging_thread_->joinable()) {
        logging_thread_->join();
    }
}

bool PersistenceLogger::logSignal(const DecodedSignal& signal) {
    if (!running_) {
        return false;
    }
    
    SignalRecord record = convertToRecord(signal);
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        record_queue_.push(record);
    }
    
    queue_cv_.notify_one();
    return true;
}

void PersistenceLogger::flush() {
    flush_requested_ = true;
    queue_cv_.notify_one();
}

bool PersistenceLogger::initializeDatabase() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    // Open database
    int rc = sqlite3_open(config_.database_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    // Enable WAL mode for better concurrency
    if (config_.enable_wal_mode) {
        executeSql("PRAGMA journal_mode=WAL;");
    }
    
    // Create table (Updated for Pipeline V1.1.0 with DF and Geolocation)
    const char* create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS signal_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp_us INTEGER NOT NULL,
            frequency_hz REAL NOT NULL,
            bandwidth_hz REAL,
            rssi_dbm REAL,
            snr_db REAL,
            modulation TEXT,
            protocol TEXT,
            multiplexing TEXT,
            signal_class INTEGER,
            confidence REAL,
            fhss_suspected INTEGER,
            jamming_suspected INTEGER,
            aoa REAL,
            target_lat REAL,
            target_lon REAL,
            payload_hex TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";
    
    if (!executeSql(create_table_sql)) {
        return false;
    }
    
    // Create indices for common queries
    executeSql("CREATE INDEX IF NOT EXISTS idx_timestamp ON signal_records(timestamp_us);");
    executeSql("CREATE INDEX IF NOT EXISTS idx_frequency ON signal_records(frequency_hz);");
    executeSql("CREATE INDEX IF NOT EXISTS idx_protocol ON signal_records(protocol);");
    executeSql("CREATE INDEX IF NOT EXISTS idx_geolocation ON signal_records(target_lat, target_lon);");
    
    return true;
}

void PersistenceLogger::loggingLoop() {
    std::vector<SignalRecord> batch;
    batch.reserve(config_.batch_size);
    
    auto last_flush = std::chrono::steady_clock::now();
    
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // Wait for data or timeout
        queue_cv_.wait_for(
            lock,
            std::chrono::milliseconds(config_.flush_interval_ms),
            [this] { return !record_queue_.empty() || !running_ || flush_requested_; }
        );
        
        // Collect batch
        while (!record_queue_.empty() && batch.size() < config_.batch_size) {
            batch.push_back(std::move(record_queue_.front()));
            record_queue_.pop();
        }
        
        lock.unlock();
        
        // Write batch if we have data
        if (!batch.empty()) {
            size_t written = writeBatch(batch);
            total_logged_ += written;
            batch.clear();
        }
        
        flush_requested_ = false;
    }
    
    // Final flush on shutdown
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!record_queue_.empty()) {
        batch.push_back(std::move(record_queue_.front()));
        record_queue_.pop();
        
        if (batch.size() >= config_.batch_size) {
            total_logged_ += writeBatch(batch);
            batch.clear();
        }
    }
    
    if (!batch.empty()) {
        total_logged_ += writeBatch(batch);
    }
}

size_t PersistenceLogger::writeBatch(const std::vector<SignalRecord>& records) {
    if (records.empty()) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    // Begin transaction
    executeSql("BEGIN TRANSACTION;");
    
    const char* insert_sql = R"(
        INSERT INTO signal_records (
            timestamp_us, frequency_hz, bandwidth_hz, rssi_dbm, snr_db,
            modulation, protocol, multiplexing, signal_class, confidence,
            fhss_suspected, jamming_suspected, aoa, target_lat, target_lon, payload_hex
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        executeSql("ROLLBACK;");
        error_count_++;
        return 0;
    }
    
    size_t written = 0;
    for (const auto& record : records) {
        sqlite3_reset(stmt);
        
        sqlite3_bind_int64(stmt, 1, record.timestamp_us);
        sqlite3_bind_double(stmt, 2, record.frequency_hz);
        sqlite3_bind_double(stmt, 3, record.bandwidth_hz);
        sqlite3_bind_double(stmt, 4, record.rssi_dbm);
        sqlite3_bind_double(stmt, 5, record.snr_db);
        sqlite3_bind_text(stmt, 6, record.protocol_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, record.protocol_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, "", -1, SQLITE_TRANSIENT);  // multiplexing
        sqlite3_bind_int(stmt, 9, record.activity_class);
        sqlite3_bind_double(stmt, 10, record.confidence);
        sqlite3_bind_int(stmt, 11, 0);  // fhss_suspected
        sqlite3_bind_int(stmt, 12, 0);  // jamming_suspected
        sqlite3_bind_double(stmt, 13, record.aoa);  // Angle of Arrival [NEW]
        sqlite3_bind_double(stmt, 14, record.target_lat);  // Target latitude [NEW]
        sqlite3_bind_double(stmt, 15, record.target_lon);  // Target longitude [NEW]
        sqlite3_bind_text(stmt, 16, record.payload_hex.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            written++;
        }
    }
    
    sqlite3_finalize(stmt);
    
    // Commit transaction
    executeSql("COMMIT;");
    
    return written;
}

SignalRecord PersistenceLogger::convertToRecord(const DecodedSignal& signal) const {
    SignalRecord record;
    
    record.timestamp_us = signal.timestamp_us;
    record.frequency_hz = signal.carrier_frequency_hz;
    record.bandwidth_hz = signal.bandwidth_hz;
    record.activity_class = static_cast<int>(signal.signal_class);
    record.modulation_type = static_cast<int>(signal.modulation);
    record.protocol_name = ProtocolTypeToString(signal.protocol);
    record.confidence = signal.confidence;
    record.rssi_dbm = signal.rssi_dbm;
    record.snr_db = signal.snr_db;
    
    // Initialize new fields with default values (will be populated by DF/Geolocation modules)
    record.aoa = 0.0f;           // Angle of Arrival (0 = not available)
    record.target_lat = 0.0;     // Target latitude (0 = not available)
    record.target_lon = 0.0;     // Target longitude (0 = not available)
    
    // Convert payload to hex string if present
    if (signal.payload.has_value()) {
        std::ostringstream oss;
        for (uint8_t byte : signal.payload.value()) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        record.payload_hex = oss.str();
    }
    
    return record;
}

bool PersistenceLogger::executeSql(const std::string& sql) {
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);
    
    if (rc != SQLITE_OK) {
        if (error_msg) {
            sqlite3_free(error_msg);
        }
        return false;
    }
    
    return true;
}

// ============================================================================
// CSVLogger Implementation
// ============================================================================

CSVLogger::CSVLogger(const std::string& filename)
    : filename_(filename)
    , header_written_(false)
{
    file_.open(filename_, std::ios::out | std::ios::app);
}

CSVLogger::~CSVLogger() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool CSVLogger::logSignal(const DecodedSignal& signal) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    
    if (!file_.is_open()) {
        return false;
    }
    
    if (!header_written_) {
        writeHeader();
        header_written_ = true;
    }
    
    // Write CSV row (Updated for Pipeline V1.1.0)
    file_ << signal.timestamp_us << ","
          << signal.carrier_frequency_hz << ","
          << signal.bandwidth_hz << ","
          << signal.rssi_dbm << ","
          << signal.snr_db << ","
          << ModulationTypeToString(signal.modulation) << ","
          << ProtocolTypeToString(signal.protocol) << ","
          << MultiplexingTypeToString(signal.multiplexing) << ","
          << static_cast<int>(signal.signal_class) << ","
          << signal.confidence << ","
          << signal.fhss_suspected << ","
          << signal.jamming_suspected << ","
          << "0.0" << ","  // aoa (placeholder - populated by DF module)
          << "0.0" << ","  // target_lat (placeholder - populated by Geolocation module)
          << "0.0" << "\n";  // target_lon (placeholder - populated by Geolocation module)
    
    return true;
}

void CSVLogger::flush() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

void CSVLogger::writeHeader() {
    file_ << "timestamp_us,frequency_hz,bandwidth_hz,rssi_dbm,snr_db,"
          << "modulation,protocol,multiplexing,signal_class,confidence,"
          << "fhss_suspected,jamming_suspected,aoa,target_lat,target_lon\n";
}

} // namespace CognitiveRF
