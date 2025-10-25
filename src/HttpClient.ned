package http_predictive_cache;

//
// HTTP Client module for web browsing simulation
// Follows 80% predictable pattern (home→login→dashboard) and 20% random selection
// Measures response times and sends requests with 1-2s think time
//
simple HttpClient
{
    parameters:
        @display("i=device/pc2");
        
        // Statistics collection
        @signal[requestSent](type="long");
        @signal[responseReceived](type="long");
        @signal[responseTime](type="double");
        @signal[patternFollowed](type="long");
        @signal[randomChoice](type="long");
        
        @statistic[requestsSent](title="Requests Sent"; source=requestSent; record=count,vector);
        @statistic[responsesReceived](title="Responses Received"; source=responseReceived; record=count,vector);
        @statistic[responseTime](title="Response Time"; source=responseTime; record=mean,max,min,vector; unit=s);
        @statistic[patternUsage](title="Pattern Followed"; source=patternFollowed; record=count,vector);
        @statistic[randomSelections](title="Random Selections"; source=randomChoice; record=count,vector);
        
    gates:
        output out;
        input in;
}