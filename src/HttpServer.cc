#include <omnetpp.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <algorithm>

using namespace omnetpp;

/**
 * HTTP Server module implementation
 * Handles HTTP requests with caching and predictive capabilities
 */
class HttpServer : public cSimpleModule
{
private:
    // Cache entry structure
    struct CacheEntry {
        int resourceId;
        simtime_t lastAccess;
        int frequency;
        
        CacheEntry(int id) : resourceId(id), lastAccess(simTime()), frequency(1) {}
    };
    
    // Parameters
    double processingTime;
    int cacheSize;
    std::string cachePolicy;
    bool enablePrediction;
    std::string predictionAlgorithm;
    double predictionAccuracy;
    int numResources;
    
    // State variables
    int requestsReceived;
    int responsesGenerated;
    std::list<CacheEntry> cache;
    std::map<int, int> accessFrequency;
    std::map<int, std::vector<int>> clientAccessPattern;
    
    // Statistics
    simsignal_t requestReceivedSignal;
    simsignal_t responseGeneratedSignal;
    simsignal_t cacheHitSignal;
    simsignal_t cacheMissSignal;
    simsignal_t predictionMadeSignal;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // Cache management methods
    virtual bool checkCache(int resourceId);
    virtual void updateCache(int resourceId);
    virtual void evictFromCache();
    
    // Prediction methods
    virtual std::vector<int> predictNextResources(int clientId, int currentResource);
    virtual void preCacheResources(const std::vector<int>& resources);
    
    // Response handling
    virtual void processRequest(cMessage *request);
    virtual void sendResponse(cMessage *request, int resourceId);
};

Define_Module(HttpServer);

void HttpServer::initialize()
{
    // Read parameters
    processingTime = par("processingTime");
    cacheSize = par("cacheSize");
    cachePolicy = par("cachePolicy").str();
    enablePrediction = par("enablePrediction");
    predictionAlgorithm = par("predictionAlgorithm").str();
    predictionAccuracy = par("predictionAccuracy");
    numResources = par("numResources");
    
    // Initialize state
    requestsReceived = 0;
    responsesGenerated = 0;
    
    // Register signals
    requestReceivedSignal = registerSignal("requestReceived");
    responseGeneratedSignal = registerSignal("responseGenerated");
    cacheHitSignal = registerSignal("cacheHit");
    cacheMissSignal = registerSignal("cacheMiss");
    predictionMadeSignal = registerSignal("predictionMade");
    
    EV << "HttpServer initialized with cache size: " << cacheSize << endl;
}

void HttpServer::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // This is a scheduled processing event
        processRequest(msg);
    } else {
        // This is a new incoming request
        requestsReceived++;
        emit(requestReceivedSignal, requestsReceived);
        
        // Store the arrival gate index for response routing
        int gateIndex = msg->getArrivalGate()->getIndex();
        msg->addPar("arrivalGateIndex") = gateIndex;
        
        EV << "Received HTTP request from gate " << gateIndex << ", scheduling processing" << endl;
        
        // Process the request after processing delay
        scheduleAt(simTime() + exponential(processingTime), msg);
    }
}

void HttpServer::processRequest(cMessage *request)
{
    // Safely extract parameters
    if (!request->hasPar("resourceId") || !request->hasPar("clientId") || 
        !request->hasPar("requestId") || !request->hasPar("arrivalGateIndex")) {
        EV << "ERROR: Received message without required parameters" << endl;
        delete request;
        return;
    }
    
    int resourceId = request->par("resourceId");
    int clientId = request->par("clientId");
    
    EV << "Processing request for resource " << resourceId 
       << " from client " << clientId << endl;
    
    // Update access patterns for prediction
    clientAccessPattern[clientId].push_back(resourceId);
    accessFrequency[resourceId]++;
    
    // Check cache
    bool cacheHit = checkCache(resourceId);
    if (cacheHit) {
        emit(cacheHitSignal, 1);
        EV << "Cache hit for resource " << resourceId << endl;
    } else {
        emit(cacheMissSignal, 1);
        updateCache(resourceId);
        EV << "Cache miss for resource " << resourceId << ", added to cache" << endl;
    }
    
    // Predictive pre-caching
    if (enablePrediction) {
        std::vector<int> predictions = predictNextResources(clientId, resourceId);
        preCacheResources(predictions);
        if (!predictions.empty()) {
            emit(predictionMadeSignal, predictions.size());
        }
    }
    
    // Send response
    sendResponse(request, resourceId);
    delete request;
}

bool HttpServer::checkCache(int resourceId)
{
    if (cacheSize == 0) return false;
    
    auto it = std::find_if(cache.begin(), cache.end(),
                          [resourceId](const CacheEntry& entry) {
                              return entry.resourceId == resourceId;
                          });
    
    if (it != cache.end()) {
        // Update access information
        it->lastAccess = simTime();
        it->frequency++;
        return true;
    }
    
    return false;
}

void HttpServer::updateCache(int resourceId)
{
    if (cacheSize == 0) return;
    
    // Check if already cached
    if (checkCache(resourceId)) return;
    
    // Add new entry
    if (cache.size() >= cacheSize) {
        evictFromCache();
    }
    
    cache.emplace_back(resourceId);
}

void HttpServer::evictFromCache()
{
    if (cache.empty()) return;
    
    if (cachePolicy == "LRU") {
        // Remove least recently used
        auto oldest = std::min_element(cache.begin(), cache.end(),
                                     [](const CacheEntry& a, const CacheEntry& b) {
                                         return a.lastAccess < b.lastAccess;
                                     });
        cache.erase(oldest);
    } else if (cachePolicy == "LFU") {
        // Remove least frequently used
        auto leastFreq = std::min_element(cache.begin(), cache.end(),
                                        [](const CacheEntry& a, const CacheEntry& b) {
                                            return a.frequency < b.frequency;
                                        });
        cache.erase(leastFreq);
    } else { // FIFO
        cache.pop_front();
    }
}

std::vector<int> HttpServer::predictNextResources(int clientId, int currentResource)
{
    std::vector<int> predictions;
    
    if (predictionAlgorithm == "frequency") {
        // Predict based on global frequency
        std::vector<std::pair<int, int>> freqPairs;
        for (const auto& pair : accessFrequency) {
            freqPairs.push_back({pair.second, pair.first}); // {frequency, resourceId}
        }
        
        std::sort(freqPairs.rbegin(), freqPairs.rend());
        
        // Take top 3 most frequent resources
        for (int i = 0; i < std::min(3, (int)freqPairs.size()); i++) {
            if (uniform(0, 1) < predictionAccuracy) {
                predictions.push_back(freqPairs[i].second);
            }
        }
    } else if (predictionAlgorithm == "pattern") {
        // Predict based on client access pattern
        auto& pattern = clientAccessPattern[clientId];
        if (pattern.size() >= 2) {
            // Simple pattern: if current resource was accessed, predict next in sequence
            for (int i = 0; i < pattern.size() - 1; i++) {
                if (pattern[i] == currentResource && uniform(0, 1) < predictionAccuracy) {
                    predictions.push_back(pattern[i + 1]);
                    break;
                }
            }
        }
    }
    
    return predictions;
}

void HttpServer::preCacheResources(const std::vector<int>& resources)
{
    for (int resourceId : resources) {
        updateCache(resourceId);
        EV << "Pre-cached resource " << resourceId << " based on prediction" << endl;
    }
}

void HttpServer::sendResponse(cMessage *request, int resourceId)
{
    cMessage *response = new cMessage("HttpResponse");
    response->addPar("resourceId") = resourceId;
    int requestId = request->par("requestId").longValue();
    response->addPar("requestId") = requestId;
    
    // Use the stored gate index
    int gateIndex = request->par("arrivalGateIndex");
    send(response, "out", gateIndex);
    
    responsesGenerated++;
    emit(responseGeneratedSignal, responsesGenerated);
    
    EV << "Sent response for resource " << resourceId 
       << " to gate " << gateIndex << endl;
}

void HttpServer::finish()
{
    EV << "HttpServer finished:" << endl;
    EV << "  Requests received: " << requestsReceived << endl;
    EV << "  Responses generated: " << responsesGenerated << endl;
    EV << "  Cache utilization: " << cache.size() << "/" << cacheSize << endl;
    
    recordScalar("requests_received", requestsReceived);
    recordScalar("responses_generated", responsesGenerated);
    recordScalar("cache_utilization", (double)cache.size() / cacheSize);
    
    // Record access frequency statistics
    if (!accessFrequency.empty()) {
        int maxFreq = 0;
        double avgFreq = 0;
        for (const auto& pair : accessFrequency) {
            maxFreq = std::max(maxFreq, pair.second);
            avgFreq += pair.second;
        }
        avgFreq /= accessFrequency.size();
        
        recordScalar("max_access_frequency", maxFreq);
        recordScalar("avg_access_frequency", avgFreq);
        recordScalar("unique_resources_accessed", accessFrequency.size());
    }
}