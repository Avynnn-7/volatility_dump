

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <new>

namespace pool_detail {

#if defined(_MSC_VER)

inline void* aligned_alloc_impl(size_t alignment, size_t size) {
    return _aligned_malloc(size, alignment);
}

inline void aligned_free_impl(void* ptr) {
    _aligned_free(ptr);
}
#else

inline void* aligned_alloc_impl(size_t alignment, size_t size) {
    
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    return std::aligned_alloc(alignment, aligned_size);
}

inline void aligned_free_impl(void* ptr) {
    std::free(ptr);
}
#endif

} 

class MemoryPool {
public:
    static constexpr size_t DEFAULT_BLOCK_SIZE = 1024 * 1024;  
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    explicit MemoryPool(size_t blockSize = DEFAULT_BLOCK_SIZE)
        : blockSize_(blockSize), currentOffset_(0), totalAllocated_(0)
    {
        allocateNewBlock();
    }
    
    ~MemoryPool() {
        for (auto* block : blocks_) {
            pool_detail::aligned_free_impl(block);
        }
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    MemoryPool(MemoryPool&& other) noexcept
        : blocks_(std::move(other.blocks_)),
          blockSize_(other.blockSize_),
          currentOffset_(other.currentOffset_),
          totalAllocated_(other.totalAllocated_)
    {
        other.blocks_.clear();
        other.currentOffset_ = 0;
        other.totalAllocated_ = 0;
    }
    
    MemoryPool& operator=(MemoryPool&& other) noexcept {
        if (this != &other) {
            
            for (auto* block : blocks_) {
                pool_detail::aligned_free_impl(block);
            }
            
            blocks_ = std::move(other.blocks_);
            blockSize_ = other.blockSize_;
            currentOffset_ = other.currentOffset_;
            totalAllocated_ = other.totalAllocated_;
            
            other.blocks_.clear();
            other.currentOffset_ = 0;
            other.totalAllocated_ = 0;
        }
        return *this;
    }

    void* allocateRaw(size_t bytes, size_t alignment = alignof(std::max_align_t)) {
        if (bytes == 0) return nullptr;

        size_t alignedOffset = (currentOffset_ + alignment - 1) & ~(alignment - 1);

        if (alignedOffset + bytes > blockSize_) {
            
            if (bytes > blockSize_) {
                
                void* block = pool_detail::aligned_alloc_impl(CACHE_LINE_SIZE, bytes);
                if (!block) throw std::bad_alloc();
                blocks_.push_back(block);
                totalAllocated_ += bytes;
                return block;
            }
            
            allocateNewBlock();
            alignedOffset = 0;
        }
        
        void* ptr = static_cast<uint8_t*>(blocks_.back()) + alignedOffset;
        currentOffset_ = alignedOffset + bytes;
        totalAllocated_ += bytes;
        
        return ptr;
    }

    template<typename T>
    T* allocate(size_t count = 1) {
        return static_cast<T*>(allocateRaw(count * sizeof(T), alignof(T)));
    }

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        T* ptr = allocate<T>(1);
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    template<typename T>
    T* createArray(size_t count) {
        T* ptr = allocate<T>(count);
        for (size_t i = 0; i < count; ++i) {
            new (&ptr[i]) T();
        }
        return ptr;
    }

    void reset() {
        
        while (blocks_.size() > 1) {
            pool_detail::aligned_free_impl(blocks_.back());
            blocks_.pop_back();
        }
        currentOffset_ = 0;
        totalAllocated_ = 0;
    }

    size_t totalAllocated() const {
        return totalAllocated_;
    }
    
    size_t numBlocks() const {
        return blocks_.size();
    }
    
    size_t blockSize() const {
        return blockSize_;
    }
    
    size_t currentBlockRemaining() const {
        return blockSize_ - currentOffset_;
    }

private:
    void allocateNewBlock() {
        void* block = pool_detail::aligned_alloc_impl(CACHE_LINE_SIZE, blockSize_);
        if (!block) {
            throw std::bad_alloc();
        }
        blocks_.push_back(block);
        currentOffset_ = 0;
    }
    
    std::vector<void*> blocks_;
    size_t blockSize_;
    size_t currentOffset_;
    size_t totalAllocated_;
};

class ThreadLocalPool {
public:
    
    static MemoryPool& get() {
        thread_local MemoryPool pool(512 * 1024);  
        return pool;
    }

    static void reset() {
        get().reset();
    }

    template<typename T>
    static T* allocate(size_t count = 1) {
        return get().allocate<T>(count);
    }

    template<typename T, typename... Args>
    static T* create(Args&&... args) {
        return get().create<T>(std::forward<Args>(args)...);
    }
};

template<typename T>
class PoolAllocator {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::false_type;
    
    explicit PoolAllocator(MemoryPool& pool) noexcept : pool_(&pool) {}
    
    template<typename U>
    PoolAllocator(const PoolAllocator<U>& other) noexcept : pool_(other.pool_) {}
    
    T* allocate(size_t n) {
        return pool_->allocate<T>(n);
    }
    
    void deallocate(T*, size_t) noexcept {
        
    }
    
    template<typename U>
    bool operator==(const PoolAllocator<U>& other) const noexcept {
        return pool_ == other.pool_;
    }
    
    template<typename U>
    bool operator!=(const PoolAllocator<U>& other) const noexcept {
        return !(*this == other);
    }

    template<typename U>
    friend class PoolAllocator;
    
private:
    MemoryPool* pool_;
};

class PoolResetGuard {
public:
    explicit PoolResetGuard(MemoryPool& pool) : pool_(pool) {}
    
    ~PoolResetGuard() {
        pool_.reset();
    }

    PoolResetGuard(const PoolResetGuard&) = delete;
    PoolResetGuard& operator=(const PoolResetGuard&) = delete;
    PoolResetGuard(PoolResetGuard&&) = delete;
    PoolResetGuard& operator=(PoolResetGuard&&) = delete;
    
private:
    MemoryPool& pool_;
};

