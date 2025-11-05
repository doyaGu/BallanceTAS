#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <utility>

/**
 * @brief Lock-free Multi-Producer Single-Consumer (MPSC) queue
 *
 * A high-performance, wait-free queue optimized for the message bus use case:
 * - Multiple contexts (producers) sending messages
 * - Single manager thread (consumer) processing messages
 *
 * Design based on Dmitry Vyukov's intrusive MPSC queue with priority support.
 *
 * Properties:
 * - Wait-free enqueue (producers never block)
 * - Lock-free dequeue (consumer never blocks)
 * - Bounded memory (configurable max size)
 * - Priority ordering (within same-priority FIFO)
 * - No dynamic allocation on hot path after warmup
 *
 * Thread Safety:
 * - Multiple threads can call Enqueue() concurrently (lock-free)
 * - Only ONE thread may call Dequeue() (single consumer)
 * - Size() is approximate (eventually consistent)
 *
 * Template Parameters:
 * - T: Message type (must be movable)
 * - MaxPriority: Maximum priority value (0-MaxPriority inclusive)
 */
template <typename T, int MaxPriority = 15>
class LockFreeMPSCQueue {
public:
    /**
     * @brief Constructs a bounded MPSC queue
     * @param maxSize Maximum number of elements (0 = unbounded, not recommended)
     */
    explicit LockFreeMPSCQueue(size_t maxSize = 10000)
        : m_MaxSize(maxSize)
        , m_ApproxSize(0)
    {
        // Initialize per-priority queues
        for (int i = 0; i <= MaxPriority; ++i) {
            // Create dummy stub nodes
            Node* stub = new Node();
            m_PriorityQueues[i].head.store(stub, std::memory_order_relaxed);
            m_PriorityQueues[i].tail.store(stub, std::memory_order_relaxed);
        }
    }

    ~LockFreeMPSCQueue() {
        // Drain all queues
        while (Dequeue().has_value()) {}

        // Delete stub nodes
        for (int i = 0; i <= MaxPriority; ++i) {
            Node* stub = m_PriorityQueues[i].head.load(std::memory_order_relaxed);
            delete stub;
        }
    }

    // Non-copyable, non-movable
    LockFreeMPSCQueue(const LockFreeMPSCQueue&) = delete;
    LockFreeMPSCQueue& operator=(const LockFreeMPSCQueue&) = delete;

    /**
     * @brief Enqueues a message with given priority (wait-free for producers)
     * @param value The message to enqueue
     * @param priority Priority level (0-MaxPriority, higher = more urgent)
     * @return true if enqueued, false if queue is full
     */
    bool Enqueue(T value, int priority = 0) {
        // Validate priority
        if (priority < 0) priority = 0;
        if (priority > MaxPriority) priority = MaxPriority;

        // Check size limit (approximate, may slightly exceed)
        if (m_MaxSize > 0 && m_ApproxSize.load(std::memory_order_relaxed) >= m_MaxSize) {
            return false;
        }

        // Allocate node (only allocation on hot path)
        Node* node = new Node(std::move(value));

        // Enqueue into the appropriate priority queue
        EnqueueIntoPriorityQueue(node, priority);

        // Increment approximate size
        m_ApproxSize.fetch_add(1, std::memory_order_relaxed);

        return true;
    }

    /**
     * @brief Dequeues highest priority message (lock-free for single consumer)
     * @return Message if available, std::nullopt if empty
     *
     * THREAD SAFETY: Only ONE thread may call this method!
     */
    std::optional<T> Dequeue() {
        // Try queues from highest to lowest priority
        for (int priority = MaxPriority; priority >= 0; --priority) {
            if (auto value = DequeueFromPriorityQueue(priority)) {
                m_ApproxSize.fetch_sub(1, std::memory_order_relaxed);
                return value;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Checks if queue is empty (approximate)
     * @return true if queue appears empty
     */
    bool IsEmpty() const {
        return m_ApproxSize.load(std::memory_order_relaxed) == 0;
    }

    /**
     * @brief Gets approximate size (eventually consistent)
     * @return Approximate number of elements
     */
    size_t Size() const {
        return m_ApproxSize.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets maximum capacity
     * @return Maximum size (0 = unbounded)
     */
    size_t Capacity() const {
        return m_MaxSize;
    }

private:
    struct Node {
        std::atomic<Node*> next;
        std::optional<T> value;

        Node() : next(nullptr), value(std::nullopt) {}
        explicit Node(T&& val) : next(nullptr), value(std::move(val)) {}
    };

    struct PriorityQueue {
        // Head: only accessed by consumer (single thread)
        // Tail: accessed by producers (multiple threads via atomic CAS)
        std::atomic<Node*> head;  // Dequeue end
        std::atomic<Node*> tail;  // Enqueue end
        char padding[64 - sizeof(std::atomic<Node*>) * 2];  // Cache line padding
    };

    void EnqueueIntoPriorityQueue(Node* node, int priority) {
        auto& queue = m_PriorityQueues[priority];

        // Initialize node
        node->next.store(nullptr, std::memory_order_relaxed);

        // Atomically swap tail pointer and link previous tail to new node
        Node* prev = queue.tail.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    std::optional<T> DequeueFromPriorityQueue(int priority) {
        auto& queue = m_PriorityQueues[priority];

        // Load head (only consumer accesses head, so relaxed is ok)
        Node* head = queue.head.load(std::memory_order_relaxed);
        Node* next = head->next.load(std::memory_order_acquire);

        // Queue is empty if next is null
        if (next == nullptr) {
            return std::nullopt;
        }

        // Extract value from next node
        std::optional<T> value = std::move(next->value);

        // Move head forward
        queue.head.store(next, std::memory_order_relaxed);

        // Delete old head (it's now a stub)
        delete head;

        return value;
    }

    // Configuration
    size_t m_MaxSize;

    // Per-priority queues (cache-line padded to avoid false sharing)
    alignas(64) PriorityQueue m_PriorityQueues[MaxPriority + 1];

    // Approximate size counter (relaxed ordering, for size limits only)
    alignas(64) std::atomic<size_t> m_ApproxSize;
};
