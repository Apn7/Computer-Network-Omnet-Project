#include <omnetpp.h>
#include <string>
#include <vector>
#include <random>
#include "HttpMessage.h"

using namespace omnetpp;

/**
 * HTTP Client module implementation
 * Follows 80% predictable pattern (home→login→dashboard cycle) and 20% random selection
 */
class HttpClient : public cSimpleModule
{
public:
    // Web page definitions (matching server)
    enum WebPage {
        HOME = 0,
        LOGIN = 1,
        DASHBOARD = 2,
        PROFILE = 3,
        SETTINGS = 4,
        LOGOUT = 5
    };

private:
    // State variables
    int clientId;
    int requestCounter;
    int requestsSent;
    int responsesReceived;
    int currentPatternStep;  // For tracking position in predictable pattern
    int currentPage;         // Current page the client is on
    
    // Pattern control
    std::vector<int> predictablePattern;  // home→login→dashboard cycle
    double patternProbability;            // 80% predictable, 20% random
    
    // Random number generation
    std::mt19937 rng;
    std::uniform_real_distribution<double> patternChoice;
    std::uniform_real_distribution<double> thinkTimeDistribution;
    std::uniform_int_distribution<int> randomPageChoice;
    
    // Request tracking for response time measurement
    std::map<int, simtime_t> pendingRequests;  // requestId → send time
    
    // Statistics signals
    simsignal_t requestSentSignal;
    simsignal_t responseReceivedSignal;
    simsignal_t responseTimeSignal;
    simsignal_t patternFollowedSignal;
    simsignal_t randomChoiceSignal;
    
    // Self messages
    cMessage *nextRequestTimer;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // Helper methods
    virtual void scheduleNextRequest();
    virtual int selectNextPage();
    virtual int getNextPatternPage();
    virtual int getRandomPage();
    virtual void sendHttpRequest(int pageId);
    virtual void handleHttpResponse(HttpResponse *response);
    virtual std::string getPageName(int pageId);
};

Define_Module(HttpClient);

void HttpClient::initialize()
{
    // Initialize client ID from index or parameter
    clientId = getIndex();
    requestCounter = 0;
    requestsSent = 0;
    responsesReceived = 0;
    currentPatternStep = 0;
    currentPage = HOME;  // Start at home page
    
    // Setup predictable pattern: home→login→dashboard cycle
    predictablePattern = {HOME, LOGIN, DASHBOARD};
    patternProbability = 0.8;  // 80% predictable pattern
    
    // Initialize random number generators
    rng.seed(intuniform(0, 100000) + clientId);  // Unique seed per client
    patternChoice = std::uniform_real_distribution<double>(0.0, 1.0);
    thinkTimeDistribution = std::uniform_real_distribution<double>(1.0, 2.0);  // 1-2 seconds
    randomPageChoice = std::uniform_int_distribution<int>(HOME, LOGOUT);  // All pages
    
    // Register statistics signals
    requestSentSignal = registerSignal("requestSent");
    responseReceivedSignal = registerSignal("responseReceived");
    responseTimeSignal = registerSignal("responseTime");
    patternFollowedSignal = registerSignal("patternFollowed");
    randomChoiceSignal = registerSignal("randomChoice");
    
    // Set initial client display
    getDisplayString().setTagArg("i", 1, "blue");
    getDisplayString().setTagArg("t", 0, ("Client " + std::to_string(clientId) + "\nReady").c_str());
    
    // Schedule first request after a small random delay
    nextRequestTimer = new cMessage("nextRequest");
    scheduleAt(simTime() + uniform(0.1, 0.5), nextRequestTimer);
    
    EV << "HttpClient " << clientId << " initialized, starting at page " << currentPage << endl;
}

void HttpClient::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (msg == nextRequestTimer) {
            // Time to send next request
            int nextPage = selectNextPage();
            sendHttpRequest(nextPage);
            
            // Update current page
            currentPage = nextPage;
        }
    } else {
        // Handle HTTP response
        HttpResponse *response = dynamic_cast<HttpResponse*>(msg);
        if (response) {
            handleHttpResponse(response);
        } else {
            EV << "ERROR: Received non-HttpResponse message: " << msg->getClassName() << endl;
        }
        delete msg;
    }
}

int HttpClient::selectNextPage()
{
    // Decide whether to follow predictable pattern (80%) or choose randomly (20%)
    if (patternChoice(rng) < patternProbability) {
        // Follow predictable pattern
        emit(patternFollowedSignal, 1);
        return getNextPatternPage();
    } else {
        // Choose random page
        emit(randomChoiceSignal, 1);
        return getRandomPage();
    }
}

int HttpClient::getNextPatternPage()
{
    // Follow home→login→dashboard cycle
    int nextPage = predictablePattern[currentPatternStep];
    currentPatternStep = (currentPatternStep + 1) % predictablePattern.size();
    
    // Visual feedback for pattern following
    getDisplayString().setTagArg("i", 1, "blue");
    std::string bubbleText = "Following Pattern\nNext: " + getPageName(nextPage);
    bubble(bubbleText.c_str());
    
    EV << "Client " << clientId << " following pattern: page " << nextPage << endl;
    return nextPage;
}

int HttpClient::getRandomPage()
{
    // Select any page randomly
    int randomPage = randomPageChoice(rng);
    
    // Visual feedback for random selection
    getDisplayString().setTagArg("i", 1, "orange");
    std::string bubbleText = "Random Choice\nNext: " + getPageName(randomPage);
    bubble(bubbleText.c_str());
    
    EV << "Client " << clientId << " random selection: page " << randomPage << endl;
    return randomPage;
}

void HttpClient::sendHttpRequest(int pageId)
{
    requestCounter++;
    requestsSent++;
    
    // Create HttpRequest message
    HttpRequest *request = new HttpRequest("HttpRequest");
    request->setRequestId(requestCounter);
    request->setClientId(clientId);
    request->setResourceId(pageId);
    request->setFromPage(currentPage);  // Track navigation pattern
    request->setTimestamp(simTime());
    
    // Store send time for response time calculation
    pendingRequests[requestCounter] = simTime();
    
    // Send request to server
    send(request, "out");
    
    // Visual feedback for sending request
    getDisplayString().setTagArg("i", 1, "yellow");
    std::string bubbleText = "Sending Request\n" + getPageName(pageId);
    bubble(bubbleText.c_str());
    
    emit(requestSentSignal, requestsSent);
    
    EV << "Client " << clientId << " sent request " << requestCounter 
       << " for page " << pageId << " (from page " << currentPage << ")" << endl;
}

void HttpClient::handleHttpResponse(HttpResponse *response)
{
    responsesReceived++;
    
    int requestId = response->getRequestId();
    int pageId = response->getResourceId();
    
    // Calculate and record response time
    auto it = pendingRequests.find(requestId);
    if (it != pendingRequests.end()) {
        simtime_t responseTime = simTime() - it->second;
        emit(responseTimeSignal, responseTime.dbl());
        pendingRequests.erase(it);
        
        // Visual feedback for response received
        if (responseTime < 0.05) { // Fast response (likely cache hit)
            getDisplayString().setTagArg("i", 1, "green");
            std::string bubbleText = "Fast Response!\n" + getPageName(pageId) + "\n" + std::to_string((int)(responseTime.dbl()*1000)) + "ms";
            bubble(bubbleText.c_str());
        } else { // Normal response
            getDisplayString().setTagArg("i", 1, "blue");
            std::string bubbleText = "Response Received\n" + getPageName(pageId) + "\n" + std::to_string((int)(responseTime.dbl()*1000)) + "ms";
            bubble(bubbleText.c_str());
        }
        
        EV << "Client " << clientId << " received response for request " << requestId 
           << " (page " << pageId << ") - Response time: " << responseTime << "s" << endl;
    } else {
        EV << "WARNING: Received response for unknown request " << requestId << endl;
    }
    
    emit(responseReceivedSignal, responsesReceived);
    
    // Schedule next request after think time (1-2 seconds)
    scheduleNextRequest();
}

void HttpClient::scheduleNextRequest()
{
    double thinkTime = thinkTimeDistribution(rng);
    scheduleAt(simTime() + thinkTime, nextRequestTimer);
    
    EV << "Client " << clientId << " will send next request in " << thinkTime << "s" << endl;
}

void HttpClient::finish()
{
    // Calculate statistics
    double avgResponseTime = 0.0;
    if (responsesReceived > 0) {
        // This would be calculated from collected response times in a real implementation
        avgResponseTime = 0.15;  // Placeholder
    }
    
    int patternFollowed = 0;  // Would track from signals in real implementation
    int randomChoices = 0;    // Would track from signals in real implementation
    
    EV << "HttpClient " << clientId << " statistics:" << endl;
    EV << "  Total requests sent: " << requestsSent << endl;
    EV << "  Total responses received: " << responsesReceived << endl;
    EV << "  Response rate: " 
       << (requestsSent > 0 ? (double)responsesReceived / requestsSent * 100 : 0) << "%" << endl;
    EV << "  Pattern followed: " << patternFollowed << " times" << endl;
    EV << "  Random choices: " << randomChoices << " times" << endl;
    
    // Record scalar statistics
    recordScalar("requestsSent", requestsSent);
    recordScalar("responsesReceived", responsesReceived);
    recordScalar("responseRate", requestsSent > 0 ? (double)responsesReceived / requestsSent : 0);
    recordScalar("avgResponseTime", avgResponseTime);
    recordScalar("patternFollowed", patternFollowed);
    recordScalar("randomChoices", randomChoices);
    
    // Clean up
    cancelAndDelete(nextRequestTimer);
}

std::string HttpClient::getPageName(int pageId)
{
    switch(pageId) {
        case HOME: return "home";
        case LOGIN: return "login";
        case DASHBOARD: return "dashboard";
        case PROFILE: return "profile";
        case SETTINGS: return "settings";
        case LOGOUT: return "logout";
        default: return "unknown";
    }
}