#include <omnetpp.h>
#include <string>
#include <vector>
#include <map>

using namespace omnetpp;

/**
 * HTTP Client module implementation
 * Generates HTTP requests and handles responses with optional caching and prediction
 */
class HttpClient : public cSimpleModule
{
private:
    // Parameters
    double requestInterval;
    int numRequests;
    double thinkTime;
    std::string requestPattern;
    int cacheSize;
    bool enablePrediction;
    
    // State variables
    int requestsSent;
    int responsesReceived;
    std::vector<int> cache;
    std::map<int, int> accessFrequency;
    
    // Statistics
    simsignal_t requestSentSignal;
    simsignal_t responseReceivedSignal;
    simsignal_t cacheHitSignal;
    simsignal_t cacheMissSignal;
    
    // Self messages
    cMessage *sendRequestTimer;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // Helper methods
    virtual int generateResourceId();
    virtual bool checkCache(int resourceId);
    virtual void updateCache(int resourceId);
    virtual void sendHttpRequest(int resourceId);
    virtual void handleHttpResponse(cMessage *msg);
};

Define_Module(HttpClient);

void HttpClient::initialize()
{
    // Read parameters
    requestInterval = par("requestInterval");
    numRequests = par("numRequests");
    thinkTime = par("thinkTime");
    requestPattern = par("requestPattern").str();
    cacheSize = par("cacheSize");
    enablePrediction = par("enablePrediction");
    
    // Initialize state
    requestsSent = 0;
    responsesReceived = 0;
    cache.reserve(cacheSize);
    
    // Register signals
    requestSentSignal = registerSignal("requestSent");
    responseReceivedSignal = registerSignal("responseReceived");
    cacheHitSignal = registerSignal("cacheHit");
    cacheMissSignal = registerSignal("cacheMiss");
    
    // Schedule first request
    sendRequestTimer = new cMessage("sendRequest");
    scheduleAt(simTime() + exponential(requestInterval), sendRequestTimer);
    
    EV << "HttpClient initialized: " << getName() << endl;
}

void HttpClient::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (msg == sendRequestTimer) {
            // Time to send a new request
            if (requestsSent < numRequests) {
                int resourceId = generateResourceId();
                
                if (!checkCache(resourceId)) {
                    sendHttpRequest(resourceId);
                    emit(cacheMissSignal, 1);
                } else {
                    emit(cacheHitSignal, 1);
                    EV << "Cache hit for resource " << resourceId << endl;
                }
                
                // Schedule next request
                scheduleAt(simTime() + exponential(requestInterval), sendRequestTimer);
            }
        }
    } else {
        // Handle HTTP response
        handleHttpResponse(msg);
        delete msg;
    }
}

int HttpClient::generateResourceId()
{
    int resourceId = 0;
    
    if (requestPattern == "sequential") {
        resourceId = requestsSent % 100; // Cycle through first 100 resources
    } else if (requestPattern == "hotspot") {
        // 80% requests go to 20% of resources (Pareto distribution)
        if (uniform(0, 1) < 0.8) {
            resourceId = intuniform(0, 19); // Hot resources
        } else {
            resourceId = intuniform(20, 999); // Cold resources
        }
    } else { // random
        resourceId = intuniform(0, 999);
    }
    
    return resourceId;
}

bool HttpClient::checkCache(int resourceId)
{
    if (cacheSize == 0) return false;
    
    auto it = std::find(cache.begin(), cache.end(), resourceId);
    return (it != cache.end());
}

void HttpClient::updateCache(int resourceId)
{
    if (cacheSize == 0) return;
    
    // Check if already in cache
    auto it = std::find(cache.begin(), cache.end(), resourceId);
    if (it != cache.end()) {
        return; // Already cached
    }
    
    // Add to cache
    if (cache.size() < cacheSize) {
        cache.push_back(resourceId);
    } else {
        // Replace first item (FIFO policy for simplicity)
        cache[0] = resourceId;
        std::rotate(cache.begin(), cache.begin() + 1, cache.end());
    }
}

void HttpClient::sendHttpRequest(int resourceId)
{
    cMessage *request = new cMessage("HttpRequest");
    request->addPar("resourceId") = resourceId;
    request->addPar("clientId") = getId();
    request->addPar("requestId") = requestsSent;
    
    send(request, "out");
    requestsSent++;
    
    emit(requestSentSignal, requestsSent);
    EV << "Sent request for resource " << resourceId << " (request #" << requestsSent << ")" << endl;
}

void HttpClient::handleHttpResponse(cMessage *msg)
{
    int resourceId = msg->par("resourceId");
    
    responsesReceived++;
    updateCache(resourceId);
    
    emit(responseReceivedSignal, responsesReceived);
    EV << "Received response for resource " << resourceId << " (response #" << responsesReceived << ")" << endl;
}

void HttpClient::finish()
{
    EV << "HttpClient " << getName() << " finished:" << endl;
    EV << "  Requests sent: " << requestsSent << endl;
    EV << "  Responses received: " << responsesReceived << endl;
    EV << "  Cache size: " << cache.size() << "/" << cacheSize << endl;
    
    recordScalar("requests_sent", requestsSent);
    recordScalar("responses_received", responsesReceived);
    recordScalar("cache_utilization", (double)cache.size() / cacheSize);
    
    cancelAndDelete(sendRequestTimer);
}