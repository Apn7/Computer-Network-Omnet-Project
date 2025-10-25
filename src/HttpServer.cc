#include <omnetpp.h>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <algorithm>
#include <iomanip>
#include "HttpMessage.h"
#include "CacheEntry.h"

using namespace omnetpp;

/**
 * HTTP Server module implementation
 * Handles HTTP requests for 6 web pages with random processing delay
 */
class HttpServer : public cSimpleModule
{
public:
    // Web page definitions
    enum WebPage {
        HOME = 0,
        LOGIN = 1,
        DASHBOARD = 2,
        PROFILE = 3,
        SETTINGS = 4,
        LOGOUT = 5
    };

private:
    // Web page information structure
    struct PageInfo {
        int pageId;
        std::string pageName;
        std::string content;
        int contentSize;
        int ttl;  // Time to live in seconds
        
        // Default constructor for map operations
        PageInfo() : pageId(-1), pageName(""), content(""), contentSize(0), ttl(3600) {}
        
        PageInfo(int id, const std::string& name, const std::string& pageContent, int ttlSeconds = 3600)
            : pageId(id), pageName(name), content(pageContent), ttl(ttlSeconds) {
            contentSize = pageContent.length();
        }
    };
    
    // Server state variables
    std::map<int, PageInfo> webPages;  // pageId -> PageInfo
    int requestsReceived;
    int responsesGenerated;
    
    // Pattern learning variables
    std::map<std::pair<std::string, std::string>, int> patternTable;  // (fromPage, toPage) -> count
    std::map<int, std::string> clientLastPage;  // clientId -> last page visited
    
    // Predictive caching variables
    std::map<std::string, CacheEntry> responseCache;  // pageName -> cached response
    double predictionThreshold;  // Minimum probability for pre-caching (60%)
    
    // Cache management variables
    std::map<std::string, cMessage*> cacheExpiryMessages;  // pageName -> expiry message
    int maxCacheSize;  // Maximum number of cached entries
    int currentCacheSize;  // Current number of cached entries
    double cacheCleanupInterval;  // Periodic cleanup interval in seconds
    cMessage* cacheCleanupTimer;  // Timer for periodic cleanup
    
    // Metrics tracking variables
    std::map<int, simtime_t> requestStartTimes;  // requestId -> start time
    int totalCacheHits;
    int totalCacheMisses;
    double totalTimeSaved;
    
    // Random number generation for processing delay
    std::mt19937 rng;
    std::uniform_real_distribution<double> delayDistribution;
    std::uniform_real_distribution<double> cacheHitDelayDistribution;
    
    // Statistics signals
    simsignal_t requestReceivedSignal;
    simsignal_t responseGeneratedSignal;
    simsignal_t processingTimeSignal;
    simsignal_t patternLearnedSignal;
    simsignal_t cacheHitSignal;
    simsignal_t cacheMissSignal;
    simsignal_t cachePreGeneratedSignal;
    simsignal_t cacheExpiredSignal;
    simsignal_t cacheEvictedSignal;
    simsignal_t cacheSizeSignal;
    simsignal_t responseTimeSignal;
    simsignal_t cacheHitRateSignal;
    simsignal_t timeSavingsSignal;
    simsignal_t requestCompleteSignal;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // Helper methods
    virtual void initializeWebPages();
    virtual void handleHttpRequest(HttpRequest *request);
    virtual void processDelayedRequest(cMessage *delayedMsg);
    virtual PageInfo* getPageInfo(int pageId);
    virtual std::string generatePageContent(const std::string& pageName);
    
    // Pattern learning methods
    virtual void updatePatternTable(int clientId, const std::string& fromPage, const std::string& toPage);
    virtual double calculateTransitionProbability(const std::string& fromPage, const std::string& toPage);
    virtual std::string getPageName(int pageId);
    virtual void printPatternStatistics();
    
    // Predictive caching methods
    virtual bool checkResponseCache(const std::string& page, std::string& cachedResponse);
    virtual void predictivePreCache(const std::string& currentPage);
    
    // Cache management methods
    virtual void scheduleCacheExpiry(const std::string& pageName, double ttlSeconds);
    virtual void handleCacheExpiry(const std::string& pageName);
    virtual void cleanupExpiredCache();
    virtual void evictLeastRecentlyUsed();
    virtual void updateCacheSize();
    virtual bool addToCacheWithManagement(const std::string& pageName, const CacheEntry& entry);
};

Define_Module(HttpServer);

void HttpServer::initialize()
{
    // Initialize state variables
    requestsReceived = 0;
    responsesGenerated = 0;
    
    // Initialize random number generator
    rng.seed(intuniform(0, 100000));
    delayDistribution = std::uniform_real_distribution<double>(0.1, 0.2);  // 100-200ms
    
    // Initialize predictive caching
    cacheHitDelayDistribution = std::uniform_real_distribution<double>(0.01, 0.02); // 10-20ms
    predictionThreshold = 0.6; // 60%
    
    // Initialize cache management
    maxCacheSize = 20;  // Maximum 20 cached pages
    currentCacheSize = 0;
    cacheCleanupInterval = 10.0;  // Cleanup every 10 seconds
    cacheCleanupTimer = new cMessage("CacheCleanup");
    scheduleAt(simTime() + cacheCleanupInterval, cacheCleanupTimer);
    
    // Initialize web pages
    initializeWebPages();
    
    // Register signals for statistics
    requestReceivedSignal = registerSignal("requestReceived");
    responseGeneratedSignal = registerSignal("responseGenerated");
    processingTimeSignal = registerSignal("processingTime");
    patternLearnedSignal = registerSignal("patternLearned");
    cacheHitSignal = registerSignal("cacheHit");
    cacheMissSignal = registerSignal("cacheMiss");
    cachePreGeneratedSignal = registerSignal("cachePreGenerated");
    cacheExpiredSignal = registerSignal("cacheExpired");
    cacheEvictedSignal = registerSignal("cacheEvicted");
    cacheSizeSignal = registerSignal("cacheSize");
    responseTimeSignal = registerSignal("responseTime");
    cacheHitRateSignal = registerSignal("cacheHitRate");
    timeSavingsSignal = registerSignal("timeSavings");
    requestCompleteSignal = registerSignal("requestComplete");
    
    // Initialize metrics tracking
    totalCacheHits = 0;
    totalCacheMisses = 0;
    totalTimeSaved = 0.0;
    
    EV << "HttpServer initialized with " << webPages.size() << " web pages" << endl;
    EV << "Pages available: ";
    for (const auto& page : webPages) {
        EV << page.second.pageName << " ";
    }
    EV << endl;
}

void HttpServer::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // Check message type for different self-messages
        if (strcmp(msg->getName(), "CachedResponse") == 0) {
            // Handle cached response
            int requestId = msg->par("requestId");
            int clientId = msg->par("clientId");
            int resourceId = msg->par("resourceId");
            int fromPage = msg->par("fromPage");
            int arrivalGate = msg->par("arrivalGate");
            const char* content = msg->par("content");
            
            // Create and send cached response
            HttpResponse *response = new HttpResponse("HttpResponse");
            response->setRequestId(requestId);
            response->setResourceId(resourceId);
            response->setContent(content);
            response->setTimestamp(simTime());
            response->setTtl(3600);
            response->setCacheable(true);
            
            send(response, "out", arrivalGate);
            
            responsesGenerated++;
            emit(responseGeneratedSignal, responsesGenerated);
            
            // Calculate and emit response time
            auto startTimeIt = requestStartTimes.find(requestId);
            if (startTimeIt != requestStartTimes.end()) {
                double responseTime = SIMTIME_DBL(simTime() - startTimeIt->second);
                emit(responseTimeSignal, responseTime);
                requestStartTimes.erase(startTimeIt);
                emit(requestCompleteSignal, 1);
                
                EV << "Response time for cached request " << requestId << ": " << responseTime << "s" << endl;
            }
            
            // Pattern learning for cached requests too
            std::string currentPageName = getPageName(resourceId);
            std::string fromPageName = getPageName(fromPage);
            
            if (fromPage >= 0) {  // Valid fromPage
                updatePatternTable(clientId, fromPageName, currentPageName);
            }
            
            // Trigger predictive pre-caching
            predictivePreCache(currentPageName);
            
            EV << "Sent cached response for page '" << currentPageName 
               << "' to client " << clientId << endl;
               
            delete msg;
        } else if (strcmp(msg->getName(), "CacheCleanup") == 0) {
            // Periodic cache cleanup
            cleanupExpiredCache();
            // Reschedule next cleanup
            scheduleAt(simTime() + cacheCleanupInterval, cacheCleanupTimer);
        } else if (strncmp(msg->getName(), "CacheExpiry_", 12) == 0) {
            // Individual cache entry expiry
            std::string pageName = msg->getName() + 12;  // Extract page name after "CacheExpiry_"
            handleCacheExpiry(pageName);
            delete msg;
        } else {
            // This is a delayed processing message
            processDelayedRequest(msg);
        }
    } else {
        // This is an incoming HTTP request
        HttpRequest *httpRequest = dynamic_cast<HttpRequest*>(msg);
        if (httpRequest) {
            handleHttpRequest(httpRequest);
        } else {
            EV << "ERROR: Received non-HttpRequest message: " << msg->getClassName() << endl;
            delete msg;
        }
    }
}

void HttpServer::initializeWebPages()
{
    // Initialize the 6 web pages with realistic content
    webPages[HOME] = PageInfo(HOME, "home", 
        generatePageContent("Home"), 3600);
        
    webPages[LOGIN] = PageInfo(LOGIN, "login", 
        generatePageContent("Login"), 1800);
        
    webPages[DASHBOARD] = PageInfo(DASHBOARD, "dashboard", 
        generatePageContent("Dashboard"), 900);
        
    webPages[PROFILE] = PageInfo(PROFILE, "profile", 
        generatePageContent("Profile"), 1800);
        
    webPages[SETTINGS] = PageInfo(SETTINGS, "settings", 
        generatePageContent("Settings"), 1200);
        
    webPages[LOGOUT] = PageInfo(LOGOUT, "logout", 
        generatePageContent("Logout"), 300);
        
    EV << "Initialized " << webPages.size() << " web pages" << endl;
}

void HttpServer::handleHttpRequest(HttpRequest *request)
{
    requestsReceived++;
    emit(requestReceivedSignal, requestsReceived);
    
    EV << "Received HTTP request from client " << request->getClientId() 
       << " for resource " << request->getResourceId() 
       << " (request ID: " << request->getRequestId() << ")" << endl;
    
    // Record request start time for response time calculation
    requestStartTimes[request->getRequestId()] = simTime();
    
    // Check cache first
    std::string pageName = getPageName(request->getResourceId());
    std::string cachedResponse;
    
    if (checkResponseCache(pageName, cachedResponse)) {
        // Cache hit - serve from cache with reduced delay
        double cacheDelay = cacheHitDelayDistribution(rng);
        emit(processingTimeSignal, cacheDelay);
        
        EV << "Cache HIT for page '" << pageName << "' - serving with " << cacheDelay << "s delay" << endl;
        
        // Update cache hit metrics
        totalCacheHits++;
        emit(cacheHitSignal, 1);
        
        // Calculate time savings (normal delay - cache delay)
        double normalDelay = (delayDistribution.min() + delayDistribution.max()) / 2.0; // Average normal delay
        double timeSaved = normalDelay - cacheDelay;
        totalTimeSaved += timeSaved;
        emit(timeSavingsSignal, timeSaved);
        
        // Update cache hit rate
        double hitRate = (double)totalCacheHits / (totalCacheHits + totalCacheMisses) * 100.0;
        emit(cacheHitRateSignal, hitRate);
        
        // Create response from cache
        HttpResponse *response = new HttpResponse("HttpResponse");
        response->setRequestId(request->getRequestId());
        response->setResourceId(request->getResourceId());
        response->setContent(cachedResponse);
        response->setTimestamp(simTime() + cacheDelay);
        response->setTtl(3600);
        response->setCacheable(true);
        
        // Schedule sending the cached response
        cMessage *cachedMsg = new cMessage("CachedResponse");
        cachedMsg->addPar("requestId") = request->getRequestId();
        cachedMsg->addPar("clientId") = request->getClientId();
        cachedMsg->addPar("resourceId") = request->getResourceId();
        cachedMsg->addPar("fromPage") = request->getFromPage();
        cachedMsg->addPar("arrivalGate") = request->getArrivalGate()->getIndex();
        cachedMsg->addPar("content") = cachedResponse.c_str();
        
        scheduleAt(simTime() + cacheDelay, cachedMsg);
        delete request;
        return;
    }
    
    // Cache miss - generate normal processing delay (100-200ms)
    EV << "Cache MISS for page '" << pageName << "' - processing normally" << endl;
    
    // Update cache miss metrics
    totalCacheMisses++;
    emit(cacheMissSignal, 1);
    
    // Update cache hit rate
    double hitRate = (totalCacheHits + totalCacheMisses > 0) ? 
                     (double)totalCacheHits / (totalCacheHits + totalCacheMisses) * 100.0 : 0.0;
    emit(cacheHitRateSignal, hitRate);
    
    double delay = delayDistribution(rng);
    emit(processingTimeSignal, delay);
    
    // Store the request information and gate for delayed processing
    cMessage *delayedMsg = new cMessage("DelayedProcessing");
    delayedMsg->addPar("originalRequestId") = request->getRequestId();
    delayedMsg->addPar("clientId") = request->getClientId();
    delayedMsg->addPar("resourceId") = request->getResourceId();
    delayedMsg->addPar("fromPage") = request->getFromPage();
    delayedMsg->addPar("arrivalGate") = request->getArrivalGate()->getIndex();
    
    // Schedule the delayed processing
    scheduleAt(simTime() + delay, delayedMsg);
    
    EV << "Scheduled processing with delay " << delay << "s" << endl;
    
    // Delete the original request as we've extracted all needed information
    delete request;
}

void HttpServer::processDelayedRequest(cMessage *delayedMsg)
{
    // Extract request information from the delayed message
    int requestId = delayedMsg->par("originalRequestId");
    int clientId = delayedMsg->par("clientId");
    int resourceId = delayedMsg->par("resourceId");
    int fromPage = delayedMsg->par("fromPage");
    int arrivalGate = delayedMsg->par("arrivalGate");
    
    EV << "Processing delayed request ID " << requestId 
       << " for resource " << resourceId << endl;
    
    // Get the page information
    PageInfo* pageInfo = getPageInfo(resourceId);
    
    if (pageInfo) {
        // Create HTTP response
        HttpResponse *response = new HttpResponse("HttpResponse");
        response->setRequestId(requestId);
        response->setResourceId(resourceId);
        response->setContent(pageInfo->content);
        response->setTimestamp(simTime());
        response->setTtl(pageInfo->ttl);
        response->setCacheable(true);
        
        // Send response back to the client through the same gate
        send(response, "out", arrivalGate);
        
        responsesGenerated++;
        emit(responseGeneratedSignal, responsesGenerated);
        
        // Calculate and emit response time
        auto startTimeIt = requestStartTimes.find(requestId);
        if (startTimeIt != requestStartTimes.end()) {
            double responseTime = SIMTIME_DBL(simTime() - startTimeIt->second);
            emit(responseTimeSignal, responseTime);
            requestStartTimes.erase(startTimeIt);
            emit(requestCompleteSignal, 1);
            
            EV << "Response time for request " << requestId << ": " << responseTime << "s" << endl;
        }
        
        // Pattern learning: Update pattern table if we have previous page information
        std::string currentPageName = getPageName(resourceId);
        std::string fromPageName = getPageName(fromPage);
        
        if (fromPage >= 0) {  // Valid fromPage
            updatePatternTable(clientId, fromPageName, currentPageName);
        }
        
        // Trigger predictive pre-caching after serving the response
        predictivePreCache(currentPageName);
        
        EV << "Sent HttpResponse for page '" << pageInfo->pageName 
           << "' (size: " << pageInfo->contentSize << " bytes) "
           << "to client " << clientId 
           << " through gate " << arrivalGate << endl;
    } else {
        EV << "ERROR: Requested resource " << resourceId << " not found!" << endl;
        
        // Send error response
        HttpResponse *errorResponse = new HttpResponse("HttpResponse");
        errorResponse->setRequestId(requestId);
        errorResponse->setResourceId(resourceId);
        errorResponse->setContent("ERROR 404: Page not found");
        errorResponse->setTimestamp(simTime());
        errorResponse->setTtl(300);  // Short TTL for error pages
        errorResponse->setCacheable(false);
        
        send(errorResponse, "out", arrivalGate);
        responsesGenerated++;
        emit(responseGeneratedSignal, responsesGenerated);
        
        // Calculate response time for error responses too
        auto startTimeIt = requestStartTimes.find(requestId);
        if (startTimeIt != requestStartTimes.end()) {
            double responseTime = SIMTIME_DBL(simTime() - startTimeIt->second);
            emit(responseTimeSignal, responseTime);
            requestStartTimes.erase(startTimeIt);
            emit(requestCompleteSignal, 1);
            
            EV << "Response time for error request " << requestId << ": " << responseTime << "s" << endl;
        }
    }
    
    delete delayedMsg;
}

HttpServer::PageInfo* HttpServer::getPageInfo(int pageId)
{
    auto it = webPages.find(pageId);
    if (it != webPages.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::string HttpServer::generatePageContent(const std::string& pageName)
{
    // Generate realistic page content based on page name
    std::string content = "<!DOCTYPE html>\n<html>\n<head>\n";
    content += "<title>" + pageName + " - Web Application</title>\n";
    content += "<meta charset='UTF-8'>\n";
    content += "<style>body{font-family:Arial,sans-serif;margin:20px;}</style>\n";
    content += "</head>\n<body>\n";
    
    if (pageName == "Home") {
        content += "<h1>Welcome to Our Web Application</h1>\n";
        content += "<p>This is the home page with navigation and main content.</p>\n";
        content += "<nav><ul><li><a href='login'>Login</a></li><li><a href='dashboard'>Dashboard</a></li></ul></nav>\n";
        content += "<div>Main content area with featured items and news.</div>\n";
    } else if (pageName == "Login") {
        content += "<h1>User Login</h1>\n";
        content += "<form><input type='text' placeholder='Username'><input type='password' placeholder='Password'><button>Login</button></form>\n";
        content += "<p>Please enter your credentials to access the dashboard.</p>\n";
    } else if (pageName == "Dashboard") {
        content += "<h1>User Dashboard</h1>\n";
        content += "<div>Welcome back! Here's your personal dashboard with statistics and quick actions.</div>\n";
        content += "<section>Recent activity, charts, and data visualization components.</section>\n";
        content += "<nav><ul><li><a href='profile'>Profile</a></li><li><a href='settings'>Settings</a></li><li><a href='logout'>Logout</a></li></ul></nav>\n";
    } else if (pageName == "Profile") {
        content += "<h1>User Profile</h1>\n";
        content += "<div>Personal information, avatar, and account details.</div>\n";
        content += "<form>Profile editing form with various input fields and preferences.</form>\n";
    } else if (pageName == "Settings") {
        content += "<h1>Application Settings</h1>\n";
        content += "<div>Configuration options, preferences, and system settings.</div>\n";
        content += "<form>Various settings controls, checkboxes, dropdowns, and configuration options.</form>\n";
    } else if (pageName == "Logout") {
        content += "<h1>Logout Confirmation</h1>\n";
        content += "<p>You have been successfully logged out. Thank you for using our application.</p>\n";
        content += "<a href='home'>Return to Home</a> | <a href='login'>Login Again</a>\n";
    } else {
        content += "<h1>" + pageName + "</h1>\n";
        content += "<p>Generic page content for " + pageName + ".</p>\n";
    }
    
    content += "</body>\n</html>";
    
    return content;
}

void HttpServer::finish()
{
    EV << "HttpServer statistics:" << endl;
    EV << "  Total requests received: " << requestsReceived << endl;
    EV << "  Total responses generated: " << responsesGenerated << endl;
    EV << "  Average processing efficiency: " 
       << (requestsReceived > 0 ? (double)responsesGenerated / requestsReceived * 100 : 0) << "%" << endl;
    
    // Record scalar statistics
    recordScalar("requestsReceived", requestsReceived);
    recordScalar("responsesGenerated", responsesGenerated);
    recordScalar("processingEfficiency", 
                requestsReceived > 0 ? (double)responsesGenerated / requestsReceived : 0);
    recordScalar("webPagesCount", webPages.size());
    
    // Record page-specific statistics
    for (const auto& page : webPages) {
        std::string statName = "page_" + page.second.pageName + "_size";
        recordScalar(statName.c_str(), page.second.contentSize);
    }
    
    // Print pattern learning statistics
    printPatternStatistics();
    
    // Clean up cache management
    if (cacheCleanupTimer && cacheCleanupTimer->isScheduled()) {
        cancelAndDelete(cacheCleanupTimer);
    }
    
    // Cancel all pending cache expiry messages
    for (auto& pair : cacheExpiryMessages) {
        if (pair.second && pair.second->isScheduled()) {
            cancelAndDelete(pair.second);
        }
    }
    cacheExpiryMessages.clear();
    
    // Record cache statistics
    recordScalar("maxCacheSize", maxCacheSize);
    recordScalar("finalCacheSize", currentCacheSize);
    recordScalar("cacheUtilization", currentCacheSize > 0 ? (double)currentCacheSize / maxCacheSize : 0.0);
    
    // Record comprehensive metrics
    int totalRequests = totalCacheHits + totalCacheMisses;
    double finalHitRate = (totalRequests > 0) ? (double)totalCacheHits / totalRequests * 100.0 : 0.0;
    
    recordScalar("totalCacheHits", totalCacheHits);
    recordScalar("totalCacheMisses", totalCacheMisses);
    recordScalar("totalRequests", totalRequests);
    recordScalar("finalCacheHitRate", finalHitRate);
    recordScalar("totalTimeSaved", totalTimeSaved);
    recordScalar("averageTimeSaved", totalCacheHits > 0 ? totalTimeSaved / totalCacheHits : 0.0);
    
    // Emit final cache hit rate
    if (totalRequests > 0) {
        emit(cacheHitRateSignal, finalHitRate);
    }
    
    EV << "=== Performance Metrics ===" << endl;
    EV << "Total requests processed: " << totalRequests << endl;
    EV << "Cache hits: " << totalCacheHits << " (" << finalHitRate << "%)" << endl;
    EV << "Cache misses: " << totalCacheMisses << endl;
    EV << "Total time saved: " << totalTimeSaved << "s" << endl;
    EV << "Average time saved per cache hit: " << (totalCacheHits > 0 ? totalTimeSaved / totalCacheHits : 0.0) << "s" << endl;
}

// Pattern learning method implementations
void HttpServer::updatePatternTable(int clientId, const std::string& fromPage, const std::string& toPage)
{
    if (fromPage.empty() || toPage.empty() || fromPage == toPage) {
        return;  // Skip invalid transitions
    }
    
    // Create transition key
    std::pair<std::string, std::string> transition(fromPage, toPage);
    
    // Update pattern table
    patternTable[transition]++;
    
    // Emit pattern learning signal
    emit(patternLearnedSignal, patternTable[transition]);
    
    // Update client's last page for next transition
    clientLastPage[clientId] = toPage;
    
    EV << "Pattern learning: Client " << clientId << " transition " 
       << fromPage << " -> " << toPage 
       << " (count: " << patternTable[transition] << ")" << endl;
}

double HttpServer::calculateTransitionProbability(const std::string& fromPage, const std::string& toPage)
{
    if (fromPage.empty() || toPage.empty()) {
        return 0.0;
    }
    
    // Find the specific transition
    std::pair<std::string, std::string> targetTransition(fromPage, toPage);
    auto targetIt = patternTable.find(targetTransition);
    
    if (targetIt == patternTable.end()) {
        return 0.0;  // No occurrences of this transition
    }
    
    // Calculate total transitions from the fromPage
    int totalFromPage = 0;
    for (const auto& entry : patternTable) {
        if (entry.first.first == fromPage) {
            totalFromPage += entry.second;
        }
    }
    
    if (totalFromPage == 0) {
        return 0.0;
    }
    
    // Calculate probability
    double probability = static_cast<double>(targetIt->second) / totalFromPage;
    
    EV << "Transition probability " << fromPage << " -> " << toPage 
       << ": " << probability << " (" << targetIt->second 
       << "/" << totalFromPage << ")" << endl;
    
    return probability;
}

std::string HttpServer::getPageName(int pageId)
{
    // Convert page ID to page name
    switch (pageId) {
        case HOME: return "home";
        case LOGIN: return "login";
        case DASHBOARD: return "dashboard";
        case PROFILE: return "profile";
        case SETTINGS: return "settings";
        case LOGOUT: return "logout";
        default: return "unknown";
    }
}

void HttpServer::printPatternStatistics()
{
    EV << "=== Pattern Learning Statistics ===" << endl;
    EV << "Total unique transitions learned: " << patternTable.size() << endl;
    
    if (patternTable.empty()) {
        EV << "No patterns learned yet." << endl;
        return;
    }
    
    // Sort patterns by frequency
    std::vector<std::pair<int, std::pair<std::string, std::string>>> sortedPatterns;
    for (const auto& entry : patternTable) {
        sortedPatterns.push_back({entry.second, entry.first});
    }
    
    std::sort(sortedPatterns.rbegin(), sortedPatterns.rend());
    
    EV << "Top navigation patterns:" << endl;
    int count = 0;
    for (const auto& pattern : sortedPatterns) {
        if (count >= 10) break;  // Show top 10
        
        int frequency = pattern.first;
        const std::string& fromPage = pattern.second.first;
        const std::string& toPage = pattern.second.second;
        double probability = calculateTransitionProbability(fromPage, toPage);
        
        EV << "  " << fromPage << " -> " << toPage 
           << ": " << frequency << " times (probability: " 
           << std::fixed << std::setprecision(3) << probability << ")" << endl;
        count++;
    }
    
    // Record pattern statistics
    recordScalar("totalPatterns", patternTable.size());
    recordScalar("activeClients", clientLastPage.size());
    
    // Record most frequent transition
    if (!sortedPatterns.empty()) {
        recordScalar("maxTransitionCount", sortedPatterns[0].first);
    }
}

bool HttpServer::checkResponseCache(const std::string& page, std::string& cachedResponse)
{
    auto cacheIt = responseCache.find(page);
    if (cacheIt != responseCache.end()) {
        if (!cacheIt->second.isExpired()) {
            // Cache hit
            cachedResponse = cacheIt->second.getContent();
            cacheIt->second.updateAccess();
            return true;
        } else {
            // Cache expired, remove entry
            EV << "Cache entry for page '" << page << "' expired during lookup" << endl;
            
            // Cancel expiry message if it exists
            auto expiryIt = cacheExpiryMessages.find(page);
            if (expiryIt != cacheExpiryMessages.end()) {
                cancelAndDelete(expiryIt->second);
                cacheExpiryMessages.erase(expiryIt);
            }
            
            responseCache.erase(cacheIt);
            currentCacheSize--;
            emit(cacheExpiredSignal, 1);
            emit(cacheSizeSignal, currentCacheSize);
        }
    }
    return false;
}

void HttpServer::predictivePreCache(const std::string& currentPage)
{
    // Find all transitions from current page
    for (const auto& pattern : patternTable) {
        const std::string& fromPage = pattern.first.first;
        const std::string& toPage = pattern.first.second;
        
        if (fromPage == currentPage) {
            double probability = calculateTransitionProbability(fromPage, toPage);
            
            if (probability > predictionThreshold) {
                // Check if already cached and not expired
                auto cacheIt = responseCache.find(toPage);
                bool needsPreCache = true;
                
                if (cacheIt != responseCache.end()) {
                    if (!cacheIt->second.isExpired()) {
                        needsPreCache = false; // Already cached and fresh
                    } else {
                        responseCache.erase(cacheIt); // Remove expired entry
                    }
                }
                
                if (needsPreCache) {
                    // Pre-generate response for likely next page
                    std::string responseContent = generatePageContent(toPage);
                    
                    // Create cache entry with 5 second TTL
                    // We'll use -1 as resourceId since we're indexing by page name
                    CacheEntry cacheEntry(-1, responseContent, 5);
                    cacheEntry.setTimestamp(simTime());
                    
                    // Use cache management system to add entry
                    if (addToCacheWithManagement(toPage, cacheEntry)) {
                        EV << "Pre-cached response for page '" << toPage 
                           << "' (probability: " << std::fixed << std::setprecision(3) 
                           << probability << ")" << endl;
                        emit(cachePreGeneratedSignal, 1);
                        
                        // Schedule expiry for this cache entry
                        scheduleCacheExpiry(toPage, 5.0);
                    } else {
                        EV << "Failed to cache page '" << toPage << "' - cache full" << endl;
                    }
                }
            }
        }
    }
}

void HttpServer::scheduleCacheExpiry(const std::string& pageName, double ttlSeconds)
{
    // Cancel existing expiry message if any
    auto expiryIt = cacheExpiryMessages.find(pageName);
    if (expiryIt != cacheExpiryMessages.end()) {
        cancelAndDelete(expiryIt->second);
        cacheExpiryMessages.erase(expiryIt);
    }
    
    // Schedule new expiry message
    std::string msgName = "CacheExpiry_" + pageName;
    cMessage* expiryMsg = new cMessage(msgName.c_str());
    cacheExpiryMessages[pageName] = expiryMsg;
    scheduleAt(simTime() + ttlSeconds, expiryMsg);
    
    EV << "Scheduled cache expiry for page '" << pageName << "' in " << ttlSeconds << "s" << endl;
}

void HttpServer::handleCacheExpiry(const std::string& pageName)
{
    auto cacheIt = responseCache.find(pageName);
    if (cacheIt != responseCache.end()) {
        EV << "Cache entry for page '" << pageName << "' expired and removed" << endl;
        responseCache.erase(cacheIt);
        currentCacheSize--;
        emit(cacheExpiredSignal, 1);
        emit(cacheSizeSignal, currentCacheSize);
    }
    
    // Remove expiry message reference
    auto expiryIt = cacheExpiryMessages.find(pageName);
    if (expiryIt != cacheExpiryMessages.end()) {
        cacheExpiryMessages.erase(expiryIt);
    }
}

void HttpServer::cleanupExpiredCache()
{
    int expiredCount = 0;
    auto cacheIt = responseCache.begin();
    
    while (cacheIt != responseCache.end()) {
        if (cacheIt->second.isExpired()) {
            EV << "Cleaning up expired cache entry for page '" << cacheIt->first << "'" << endl;
            
            // Cancel and remove expiry message if it exists
            auto expiryIt = cacheExpiryMessages.find(cacheIt->first);
            if (expiryIt != cacheExpiryMessages.end()) {
                cancelAndDelete(expiryIt->second);
                cacheExpiryMessages.erase(expiryIt);
            }
            
            cacheIt = responseCache.erase(cacheIt);
            currentCacheSize--;
            expiredCount++;
        } else {
            ++cacheIt;
        }
    }
    
    if (expiredCount > 0) {
        EV << "Cleaned up " << expiredCount << " expired cache entries" << endl;
        emit(cacheExpiredSignal, expiredCount);
        emit(cacheSizeSignal, currentCacheSize);
    }
}

void HttpServer::evictLeastRecentlyUsed()
{
    if (responseCache.empty()) return;
    
    // Find the least recently used cache entry
    auto lruIt = responseCache.begin();
    simtime_t oldestAccess = lruIt->second.getLastAccess();
    
    for (auto it = responseCache.begin(); it != responseCache.end(); ++it) {
        if (it->second.getLastAccess() < oldestAccess) {
            oldestAccess = it->second.getLastAccess();
            lruIt = it;
        }
    }
    
    EV << "Evicting LRU cache entry for page '" << lruIt->first << "'" << endl;
    
    // Cancel and remove expiry message if it exists
    auto expiryIt = cacheExpiryMessages.find(lruIt->first);
    if (expiryIt != cacheExpiryMessages.end()) {
        cancelAndDelete(expiryIt->second);
        cacheExpiryMessages.erase(expiryIt);
    }
    
    responseCache.erase(lruIt);
    currentCacheSize--;
    emit(cacheEvictedSignal, 1);
    emit(cacheSizeSignal, currentCacheSize);
}

void HttpServer::updateCacheSize()
{
    currentCacheSize = responseCache.size();
    emit(cacheSizeSignal, currentCacheSize);
}

bool HttpServer::addToCacheWithManagement(const std::string& pageName, const CacheEntry& entry)
{
    // Check if cache is full
    if (currentCacheSize >= maxCacheSize) {
        // First try to clean up expired entries
        cleanupExpiredCache();
        
        // If still full, evict LRU entry
        if (currentCacheSize >= maxCacheSize) {
            evictLeastRecentlyUsed();
        }
    }
    
    // Add to cache
    responseCache[pageName] = entry;
    currentCacheSize++;
    emit(cacheSizeSignal, currentCacheSize);
    
    EV << "Added page '" << pageName << "' to cache (size: " << currentCacheSize << "/" << maxCacheSize << ")" << endl;
    return true;
}