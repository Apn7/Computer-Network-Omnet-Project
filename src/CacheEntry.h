#ifndef CACHEENTRY_H
#define CACHEENTRY_H

#include <omnetpp.h>
#include <string>

using namespace omnetpp;

/**
 * Cache Entry structure for storing cached page content
 * Contains page content, metadata, and TTL information
 */
class CacheEntry
{
private:
    int resourceId;
    std::string content;
    int contentSize;
    simtime_t timestamp;
    int ttl;  // Time to live in seconds
    int accessCount;
    simtime_t lastAccess;
    bool dirty;  // Indicates if entry needs to be written back
    
public:
    // Constructors
    CacheEntry();
    CacheEntry(int resId, const std::string& pageContent, int ttlSeconds = 3600);
    CacheEntry(const CacheEntry& other);
    
    // Destructor
    ~CacheEntry();
    
    // Assignment operator
    CacheEntry& operator=(const CacheEntry& other);
    
    // Getters
    int getResourceId() const { return resourceId; }
    const std::string& getContent() const { return content; }
    int getContentSize() const { return contentSize; }
    simtime_t getTimestamp() const { return timestamp; }
    int getTtl() const { return ttl; }
    int getAccessCount() const { return accessCount; }
    simtime_t getLastAccess() const { return lastAccess; }
    bool isDirty() const { return dirty; }
    
    // Setters
    void setResourceId(int id) { resourceId = id; }
    void setContent(const std::string& pageContent);
    void setTimestamp(simtime_t t) { timestamp = t; }
    void setTtl(int ttlSeconds) { ttl = ttlSeconds; }
    void setDirty(bool d) { dirty = d; }
    
    // Cache operations
    void updateAccess();  // Update access count and last access time
    bool isExpired() const;  // Check if entry has expired
    bool isValid() const;  // Check if entry is valid (not expired and has content)
    void refresh(const std::string& newContent, int newTtl = -1);  // Refresh content
    
    // Utility methods
    std::string toString() const;
    size_t getMemorySize() const;  // Get estimated memory usage
    
    // Comparison operators for sorting/searching
    bool operator==(const CacheEntry& other) const;
    bool operator!=(const CacheEntry& other) const;
    bool operator<(const CacheEntry& other) const;  // For ordered containers
    
    // Static utility methods
    static bool compareByLastAccess(const CacheEntry& a, const CacheEntry& b);
    static bool compareByAccessCount(const CacheEntry& a, const CacheEntry& b);
    static bool compareByTimestamp(const CacheEntry& a, const CacheEntry& b);
};

#endif // CACHEENTRY_H