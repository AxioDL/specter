#ifndef SPECTER_VERTEXBUFFERPOOL_HPP
#define SPECTER_VERTEXBUFFERPOOL_HPP

#include <boo/boo.hpp>
#include <hecl/BitVector.hpp>
#include <vector>
#include <cstdlib>

namespace specter
{

#define SPECTER_VBUFPOOL_ALLOCATION_BLOCK 262144

/** This class provides a uniform structure for packing instanced vertex-buffer
 *  data with consistent stride into a vector of 256K 'Buckets'.
 *
 *  This results in a space-efficient way of managing GPU data of things like UI
 *  widgets. These can potentially have numerous binding instances, so this avoids
 *  allocating a full GPU buffer object for each. */
template <typename VertStruct>
class VertexBufferPool
{
    /* Resolve div_t type using ssize_t as basis */
#if _WIN32
    using IndexTp = SSIZE_T;
#else
    using IndexTp = ssize_t;
#endif
    struct InvalidTp {};
    using DivTp = std::conditional_t<std::is_same<IndexTp, long long>::value, std::lldiv_t,
                  std::conditional_t<std::is_same<IndexTp, long>::value, std::ldiv_t,
                  std::conditional_t<std::is_same<IndexTp, int>::value, std::div_t, InvalidTp>>>;
    static_assert(!std::is_same<DivTp, InvalidTp>::value, "unsupported IndexTp for DivTp resolution");

    /** Size of single element */
    static constexpr IndexTp m_stride = sizeof(VertStruct);
    static_assert(m_stride <= SPECTER_VBUFPOOL_ALLOCATION_BLOCK, "Stride too large for vertex pool");

    /** Number of elements per 256K bucket */
    static constexpr IndexTp m_countPerBucket = SPECTER_VBUFPOOL_ALLOCATION_BLOCK / m_stride;

    /** Buffer size per bucket (ideally 256K) */
    static constexpr IndexTp m_sizePerBucket = m_stride * m_countPerBucket;

    /** BitVector indicating free allocation elements */
    hecl::llvm::BitVector m_freeElements;

    /** Efficient way to get bucket and element simultaneously */
    DivTp getBucketDiv(IndexTp idx) const { return std::div(idx, m_countPerBucket); }

    /** Buffer pool token */
    boo::GraphicsBufferPoolToken m_token;

    /** Private bucket info */
    struct Bucket
    {
        boo::IGraphicsBufferD* buffer;
        uint8_t* cpuBuffer = nullptr;
        bool dirty = false;
        Bucket(const Bucket& other) = delete;
        Bucket& operator=(const Bucket& other) = delete;
        Bucket(Bucket&& other) = default;
        Bucket& operator=(Bucket&& other) = default;
        Bucket(VertexBufferPool& pool)
        {
            buffer = pool.m_token.newPoolBuffer(boo::BufferUse::Vertex, pool.m_stride, pool.m_countPerBucket);
        }
        void updateBuffer()
        {
            buffer->unmap();
            cpuBuffer = nullptr;
            dirty = false;
        }
    };
    std::vector<Bucket> m_buckets;

public:
    /** User element-owning token */
    class Token
    {
        friend class VertexBufferPool;
        VertexBufferPool& m_pool;
        IndexTp m_index;
        IndexTp m_count;
        DivTp m_div;
        Token(VertexBufferPool& pool, IndexTp count)
        : m_pool(pool), m_count(count)
        {
            assert(count <= pool.m_countPerBucket && "unable to fit in bucket");
            auto& freeSpaces = pool.m_freeElements;
            auto& buckets = pool.m_buckets;
            int idx = freeSpaces.find_first_contiguous(count);
            if (idx == -1)
            {
                buckets.emplace_back(pool);
                m_index = freeSpaces.size();
                freeSpaces.resize(freeSpaces.size() + pool.m_countPerBucket, true);
            }
            else
            {
                m_index = idx;
            }
            freeSpaces.reset(m_index, m_index + count);
            m_div = pool.getBucketDiv(m_index);
        }
    public:
        Token(const Token& other) = delete;
        Token& operator=(const Token& other) = delete;
        Token& operator=(Token&& other) = delete;
        Token(Token&& other)
        : m_pool(other.m_pool), m_index(other.m_index),
          m_count(other.m_count), m_div(other.m_div)
        {
            other.m_index = -1;
        }

        ~Token()
        {
            if (m_index != -1)
                m_pool.m_freeElements.set(m_index, m_index + m_count);
        }

        VertStruct* access()
        {
            Bucket& bucket = m_pool.m_buckets[m_div.quot];
            if (!bucket.cpuBuffer)
                bucket.cpuBuffer = reinterpret_cast<uint8_t*>(bucket.buffer->map(m_sizePerBucket));
            bucket.dirty = true;
            return reinterpret_cast<VertStruct*>(&bucket.cpuBuffer[m_div.rem * m_pool.m_stride]);
        }

        std::pair<boo::IGraphicsBufferD*, IndexTp> getBufferInfo() const
        {
            Bucket& bucket = m_pool.m_buckets[m_div.quot];
            return {bucket.buffer, m_div.rem};
        }
    };

    VertexBufferPool() = default;
    VertexBufferPool(const VertexBufferPool& other) = delete;
    VertexBufferPool& operator=(const VertexBufferPool& other) = delete;

    /** Load dirty buffer data into GPU */
    void updateBuffers()
    {
        for (Bucket& bucket : m_buckets)
            if (bucket.dirty)
                bucket.updateBuffer();
    }

    /** Allocate free block into client-owned Token */
    Token allocateBlock(boo::IGraphicsDataFactory* factory, IndexTp count)
    {
        if (!m_token)
            m_token = factory->newBufferPool();
        return Token(*this, count);
    }

    static constexpr IndexTp bucketCapacity() { return m_countPerBucket; }
};

}

#endif // SPECTER_VERTEXBUFFERPOOL_HPP