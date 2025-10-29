package http_predictive_cache;

//
// HTTP Server module serving 6 web pages
// Handles HttpRequest messages with random processing delay (100-200ms)
// Serves: home, login, dashboard, profile, settings, logout pages
//
simple HttpServer
{
    parameters:
        @display("i=device/server2,gold;t=HTTP Server");
        
        // Configurable parameters for predictive caching
        double predictionThreshold = default(0.6);  // Probability threshold for pre-caching (0.0-1.0)
        int cacheTTL @unit(s) = default(5s);        // Cache entry time-to-live in seconds
        int maxCacheSize = default(20);             // Maximum number of cached entries
        
        // Statistics collection
        @signal[requestReceived](type="long");
        @signal[responseGenerated](type="long");
        @signal[processingTime](type="double");
        @signal[patternLearned](type="long");
        @signal[cacheHit](type="long");
        @signal[cacheMiss](type="long");
        @signal[cachePreGenerated](type="long");
        @signal[cacheExpired](type="long");
        @signal[cacheEvicted](type="long");
        @signal[cacheSize](type="long");
        @signal[responseTime](type="double");
        @signal[cacheHitRate](type="double");
        @signal[timeSavings](type="double");
        @signal[requestComplete](type="long");
        
        @statistic[requestsReceived](title="Requests Received"; source=requestReceived; record=count,vector);
        @statistic[responsesGenerated](title="Responses Generated"; source=responseGenerated; record=count,vector);
        @statistic[processingDelay](title="Processing Delay"; source=processingTime; record=mean,max,min,vector; unit=s);
        @statistic[patternsLearned](title="Navigation Patterns Learned"; source=patternLearned; record=count,vector);
        @statistic[cacheHits](title="Cache Hits"; source=cacheHit; record=count,vector);
        @statistic[cacheMisses](title="Cache Misses"; source=cacheMiss; record=count,vector);
        @statistic[predictiveCaching](title="Pages Pre-cached"; source=cachePreGenerated; record=count,vector);
        @statistic[cacheExpired](title="Cache Entries Expired"; source=cacheExpired; record=count,vector);
        @statistic[cacheEvicted](title="Cache Entries Evicted"; source=cacheEvicted; record=count,vector);
        @statistic[cacheSize](title="Current Cache Size"; source=cacheSize; record=mean,max,vector);
        @statistic[responseTimeStats](title="Response Time per Request"; source=responseTime; record=mean,max,min,histogram,vector; unit=s);
        @statistic[cacheHitRateStats](title="Cache Hit Rate"; source=cacheHitRate; record=last,mean,vector; unit=%);
        @statistic[timeSavingsStats](title="Time Savings from Cache"; source=timeSavings; record=mean,max,sum,vector; unit=s);
        @statistic[requestsCompleted](title="Completed Requests"; source=requestComplete; record=count,vector);
        
    gates:
        input in[];
        output out[];
}
