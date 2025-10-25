package http_predictive_cache;

//
// HTTP Server module serving 6 web pages
// Handles HttpRequest messages with random processing delay (100-200ms)
// Serves: home, login, dashboard, profile, settings, logout pages
//
simple HttpServer
{
    parameters:
        @display("i=device/server2");
        
        // Statistics collection
        @signal[requestReceived](type="long");
        @signal[responseGenerated](type="long");
        @signal[processingTime](type="double");
        @signal[patternLearned](type="long");
        
        @statistic[requestsReceived](title="Requests Received"; source=requestReceived; record=count,vector);
        @statistic[responsesGenerated](title="Responses Generated"; source=responseGenerated; record=count,vector);
        @statistic[processingDelay](title="Processing Delay"; source=processingTime; record=mean,max,min,vector; unit=s);
        @statistic[patternsLearned](title="Navigation Patterns Learned"; source=patternLearned; record=count,vector);
        
    gates:
        input in[];
        output out[];
}