#ifndef HTTPMESSAGE_H
#define HTTPMESSAGE_H

#include <omnetpp.h>
#include <string>

using namespace omnetpp;

/**
 * HTTP Request message class
 * Represents an HTTP request with necessary fields for predictive caching
 */
class HttpRequest : public cMessage
{
private:
    int requestId;
    int clientId;
    int resourceId;
    std::string url;
    simtime_t timestamp;
    int fromPage;  // For pattern tracking
    
public:
    // Constructors
    HttpRequest(const char* name = "HttpRequest");
    HttpRequest(const HttpRequest& other);
    virtual ~HttpRequest();
    
    // Assignment operator
    HttpRequest& operator=(const HttpRequest& other);
    
    // Clone method for OMNeT++ message handling
    virtual HttpRequest* dup() const override;
    
    // Getters
    int getRequestId() const { return requestId; }
    int getClientId() const { return clientId; }
    int getResourceId() const { return resourceId; }
    const std::string& getUrl() const { return url; }
    simtime_t getTimestamp() const { return timestamp; }
    int getFromPage() const { return fromPage; }
    
    // Setters
    void setRequestId(int id) { requestId = id; }
    void setClientId(int id) { clientId = id; }
    void setResourceId(int id) { resourceId = id; }
    void setUrl(const std::string& u) { url = u; }
    void setTimestamp(simtime_t t) { timestamp = t; }
    void setFromPage(int page) { fromPage = page; }
    
    // Utility methods
    std::string toString() const;
};

/**
 * HTTP Response message class  
 * Represents an HTTP response with cache-relevant information
 */
class HttpResponse : public cMessage
{
private:
    int requestId;
    int resourceId;
    std::string content;
    int contentSize;
    simtime_t timestamp;
    int ttl;  // Time to live in seconds
    bool cacheable;
    
public:
    // Constructors
    HttpResponse(const char* name = "HttpResponse");
    HttpResponse(const HttpResponse& other);
    virtual ~HttpResponse();
    
    // Assignment operator
    HttpResponse& operator=(const HttpResponse& other);
    
    // Clone method for OMNeT++ message handling
    virtual HttpResponse* dup() const override;
    
    // Getters
    int getRequestId() const { return requestId; }
    int getResourceId() const { return resourceId; }
    const std::string& getContent() const { return content; }
    int getContentSize() const { return contentSize; }
    simtime_t getTimestamp() const { return timestamp; }
    int getTtl() const { return ttl; }
    bool isCacheable() const { return cacheable; }
    
    // Setters
    void setRequestId(int id) { requestId = id; }
    void setResourceId(int id) { resourceId = id; }
    void setContent(const std::string& c) { content = c; contentSize = c.length(); }
    void setContentSize(int size) { contentSize = size; }
    void setTimestamp(simtime_t t) { timestamp = t; }
    void setTtl(int t) { ttl = t; }
    void setCacheable(bool c) { cacheable = c; }
    
    // Utility methods
    std::string toString() const;
    bool isExpired() const;
};

#endif // HTTPMESSAGE_H