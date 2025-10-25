#include <omnetpp.h>
#include <string>
#include <vector>
#include <map>
#include <random>
#include "HttpMessage.h"

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
    
    // Random number generation for processing delay
    std::mt19937 rng;
    std::uniform_real_distribution<double> delayDistribution;
    
    // Statistics signals
    simsignal_t requestReceivedSignal;
    simsignal_t responseGeneratedSignal;
    simsignal_t processingTimeSignal;

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
    
    // Initialize web pages
    initializeWebPages();
    
    // Register signals for statistics
    requestReceivedSignal = registerSignal("requestReceived");
    responseGeneratedSignal = registerSignal("responseGenerated");
    processingTimeSignal = registerSignal("processingTime");
    
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
        // This is a delayed processing message
        processDelayedRequest(msg);
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
    
    // Generate random processing delay (100-200ms)
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
}