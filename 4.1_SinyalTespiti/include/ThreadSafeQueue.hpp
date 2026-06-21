/**
 * @file ThreadSafeQueue.hpp
 * @brief Thread-safe queue implementation for pipeline architecture
 * @details Provides blocking/non-blocking queue operations with condition variables
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#ifndef SIGNAL_DETECTION_THREAD_SAFE_QUEUE_HPP
#define SIGNAL_DETECTION_THREAD_SAFE_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace CognitiveRF {
namespace SignalDetection {

/**
 * @class ThreadSafeQueue
 * @brief Thread-safe FIFO queue with blocking and non-blocking operations
 * @tparam T Type of elements stored in the queue
 * 
 * @details This class provides a thread-safe queue implementation suitable for
 * producer-consumer patterns in multi-threaded signal processing pipelines.
 * Supports both blocking pop (with timeout) and non-blocking try_pop operations.
 */
template<typename T>
class ThreadSafeQueue {
public:
    /**
     * @brief Default constructor
     */
    ThreadSafeQueue() = default;
    
    /**
     * @brief Destructor - automatically shuts down the queue
     */
    ~ThreadSafeQueue() {
        shutdown();
    }
    
    // Delete copy constructor and assignment operator
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    
    /**
     * @brief Push an item to the queue (copy)
     * @param item Item to push
     * @return true if successful, false if queue is shut down
     */
    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return false;
        
        queue_.push(item);
        cond_var_.notify_one();
        return true;
    }
    
    /**
     * @brief Push an item to the queue (move)
     * @param item Item to push
     * @return true if successful, false if queue is shut down
     */
    bool push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return false;
        
        queue_.push(std::move(item));
        cond_var_.notify_one();
        return true;
    }
    
    /**
     * @brief Pop an item from the queue (blocking with timeout)
     * @param item Reference to store the popped item
     * @param timeout_ms Timeout in milliseconds (0 = wait forever)
     * @return true if item was popped, false if timeout or shutdown
     */
    bool pop(T& item, int timeout_ms = 0) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (timeout_ms > 0) {
            // Wait with timeout
            if (!cond_var_.wait_for(lock, 
                std::chrono::milliseconds(timeout_ms),
                [this]() { return !queue_.empty() || shutdown_; })) {
                return false; // Timeout
            }
        } else {
            // Wait indefinitely
            cond_var_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });
        }
        
        if (shutdown_ && queue_.empty()) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief Try to pop an item without blocking
     * @param item Reference to store the popped item
     * @return true if item was popped, false if queue is empty
     */
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty() || shutdown_) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief Get the current size of the queue
     * @return Number of items in the queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    /**
     * @brief Check if the queue is empty
     * @return true if empty, false otherwise
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    /**
     * @brief Shutdown the queue
     * @details Wakes up all waiting threads and prevents further pushes
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        cond_var_.notify_all();
    }
    
    /**
     * @brief Check if queue is shut down
     * @return true if shut down, false otherwise
     */
    bool is_shutdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutdown_;
    }
    
    /**
     * @brief Clear all items from the queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

private:
    std::queue<T> queue_;                        ///< Underlying queue
    mutable std::mutex mutex_;                   ///< Mutex for thread safety
    std::condition_variable cond_var_;           ///< Condition variable for blocking
    bool shutdown_ = false;                      ///< Shutdown flag
};

} // namespace SignalDetection
} // namespace CognitiveRF

#endif // SIGNAL_DETECTION_THREAD_SAFE_QUEUE_HPP
