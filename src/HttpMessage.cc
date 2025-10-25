#include "HttpMessage.h"
#include <sstream>

// HttpRequest implementation
HttpRequest::HttpRequest(const char* name) : cMessage(name)
{
    requestId = 0;
    clientId = 0;
    resourceId = 0;
    url = "";
    timestamp = SIMTIME_ZERO;
    fromPage = -1;
}

HttpRequest::HttpRequest(const HttpRequest& other) : cMessage(other)
{
    requestId = other.requestId;
    clientId = other.clientId;
    resourceId = other.resourceId;
    url = other.url;
    timestamp = other.timestamp;
    fromPage = other.fromPage;
}

HttpRequest::~HttpRequest()
{
    // No dynamic memory to clean up
}

HttpRequest& HttpRequest::operator=(const HttpRequest& other)
{
    if (this == &other) return *this;
    
    cMessage::operator=(other);
    requestId = other.requestId;
    clientId = other.clientId;
    resourceId = other.resourceId;
    url = other.url;
    timestamp = other.timestamp;
    fromPage = other.fromPage;
    
    return *this;
}

HttpRequest* HttpRequest::dup() const
{
    return new HttpRequest(*this);
}

std::string HttpRequest::toString() const
{
    std::ostringstream oss;
    oss << "HttpRequest{requestId=" << requestId 
        << ", clientId=" << clientId
        << ", resourceId=" << resourceId
        << ", url=" << url
        << ", timestamp=" << timestamp
        << ", fromPage=" << fromPage << "}";
    return oss.str();
}

// HttpResponse implementation
HttpResponse::HttpResponse(const char* name) : cMessage(name)
{
    requestId = 0;
    resourceId = 0;
    content = "";
    contentSize = 0;
    timestamp = SIMTIME_ZERO;
    ttl = 3600; // Default 1 hour
    cacheable = true;
}

HttpResponse::HttpResponse(const HttpResponse& other) : cMessage(other)
{
    requestId = other.requestId;
    resourceId = other.resourceId;
    content = other.content;
    contentSize = other.contentSize;
    timestamp = other.timestamp;
    ttl = other.ttl;
    cacheable = other.cacheable;
}

HttpResponse::~HttpResponse()
{
    // No dynamic memory to clean up
}

HttpResponse& HttpResponse::operator=(const HttpResponse& other)
{
    if (this == &other) return *this;
    
    cMessage::operator=(other);
    requestId = other.requestId;
    resourceId = other.resourceId;
    content = other.content;
    contentSize = other.contentSize;
    timestamp = other.timestamp;
    ttl = other.ttl;
    cacheable = other.cacheable;
    
    return *this;
}

HttpResponse* HttpResponse::dup() const
{
    return new HttpResponse(*this);
}

std::string HttpResponse::toString() const
{
    std::ostringstream oss;
    oss << "HttpResponse{requestId=" << requestId
        << ", resourceId=" << resourceId
        << ", contentSize=" << contentSize
        << ", timestamp=" << timestamp
        << ", ttl=" << ttl
        << ", cacheable=" << (cacheable ? "true" : "false") << "}";
    return oss.str();
}

bool HttpResponse::isExpired() const
{
    if (!cacheable) return true;
    
    simtime_t currentTime = simTime();
    simtime_t expiryTime = timestamp + ttl;
    
    return currentTime >= expiryTime;
}