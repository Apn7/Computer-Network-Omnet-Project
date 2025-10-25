#include "CacheEntry.h"
#include <sstream>

// Constructors
CacheEntry::CacheEntry()
{
    resourceId = -1;
    content = "";
    contentSize = 0;
    timestamp = SIMTIME_ZERO;
    ttl = 3600;  // Default 1 hour
    accessCount = 0;
    lastAccess = SIMTIME_ZERO;
    dirty = false;
}

CacheEntry::CacheEntry(int resId, const std::string& pageContent, int ttlSeconds)
{
    resourceId = resId;
    setContent(pageContent);
    timestamp = simTime();
    ttl = ttlSeconds;
    accessCount = 1;
    lastAccess = timestamp;
    dirty = false;
}

CacheEntry::CacheEntry(const CacheEntry& other)
{
    resourceId = other.resourceId;
    content = other.content;
    contentSize = other.contentSize;
    timestamp = other.timestamp;
    ttl = other.ttl;
    accessCount = other.accessCount;
    lastAccess = other.lastAccess;
    dirty = other.dirty;
}

// Destructor
CacheEntry::~CacheEntry()
{
    // No dynamic memory to clean up
}

// Assignment operator
CacheEntry& CacheEntry::operator=(const CacheEntry& other)
{
    if (this == &other) return *this;
    
    resourceId = other.resourceId;
    content = other.content;
    contentSize = other.contentSize;
    timestamp = other.timestamp;
    ttl = other.ttl;
    accessCount = other.accessCount;
    lastAccess = other.lastAccess;
    dirty = other.dirty;
    
    return *this;
}

// Setters
void CacheEntry::setContent(const std::string& pageContent)
{
    content = pageContent;
    contentSize = pageContent.length();
    dirty = true;  // Mark as dirty when content changes
}

// Cache operations
void CacheEntry::updateAccess()
{
    accessCount++;
    lastAccess = simTime();
}

bool CacheEntry::isExpired() const
{
    if (ttl <= 0) return false;  // Never expires if TTL is 0 or negative
    
    simtime_t currentTime = simTime();
    simtime_t expiryTime = timestamp + ttl;
    
    return currentTime >= expiryTime;
}

bool CacheEntry::isValid() const
{
    return (resourceId >= 0 && !content.empty() && !isExpired());
}

void CacheEntry::refresh(const std::string& newContent, int newTtl)
{
    setContent(newContent);
    timestamp = simTime();
    if (newTtl >= 0) {
        ttl = newTtl;
    }
    dirty = false;  // Fresh content is not dirty
}

// Utility methods
std::string CacheEntry::toString() const
{
    std::ostringstream oss;
    oss << "CacheEntry{resourceId=" << resourceId
        << ", contentSize=" << contentSize
        << ", timestamp=" << timestamp
        << ", ttl=" << ttl
        << ", accessCount=" << accessCount
        << ", lastAccess=" << lastAccess
        << ", expired=" << (isExpired() ? "true" : "false")
        << ", dirty=" << (dirty ? "true" : "false") << "}";
    return oss.str();
}

size_t CacheEntry::getMemorySize() const
{
    return sizeof(CacheEntry) + content.capacity();
}

// Comparison operators
bool CacheEntry::operator==(const CacheEntry& other) const
{
    return resourceId == other.resourceId;
}

bool CacheEntry::operator!=(const CacheEntry& other) const
{
    return !(*this == other);
}

bool CacheEntry::operator<(const CacheEntry& other) const
{
    return resourceId < other.resourceId;
}

// Static utility methods for sorting
bool CacheEntry::compareByLastAccess(const CacheEntry& a, const CacheEntry& b)
{
    return a.lastAccess < b.lastAccess;  // Older first (for LRU)
}

bool CacheEntry::compareByAccessCount(const CacheEntry& a, const CacheEntry& b)
{
    return a.accessCount < b.accessCount;  // Less frequent first (for LFU)
}

bool CacheEntry::compareByTimestamp(const CacheEntry& a, const CacheEntry& b)
{
    return a.timestamp < b.timestamp;  // Older first (for FIFO)
}