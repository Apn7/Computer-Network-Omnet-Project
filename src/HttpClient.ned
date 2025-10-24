package http_predictive_cache;

//
// HTTP Client module for predictive caching system
// Sends HTTP requests and receives responses from server
//
simple HttpClient
{
    parameters:
        @display("i=device/pc2");
        double requestInterval @unit(s) = default(exponential(1s));
        int numRequests = default(100);
        double thinkTime @unit(s) = default(exponential(0.1s));
        string requestPattern = default("random"); // "random", "sequential", "hotspot"
        int cacheSize = default(10);
        bool enablePrediction = default(false);
        
    gates:
        output out;
        input in;
}