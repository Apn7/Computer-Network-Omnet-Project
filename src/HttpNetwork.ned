package http_predictive_cache;

import ned.DatarateChannel;

//
// HTTP Request Prediction Network
// Simulates web browsing behavior with pattern-based navigation
// 10 HTTP clients following 80% predictable patterns, 20% random browsing
//
network HttpNetwork
{
    parameters:
        @display("bgb=800,600;bgg=100,1,grey95");
        int numClients = default(10);
        
    types:
        channel NetworkChannel extends DatarateChannel
        {
            datarate = 100Mbps;
            delay = 10ms;
            @display("ls=blue,2");
        }
        
    submodules:
        server: HttpServer {
            @display("p=400,100;i=device/server,gold");
        }
        
        client[numClients]: HttpClient {
            @display("p=100+i*60,300;i=device/laptop");
        }
        
    connections:
        for i=0..numClients-1 {
            client[i].out --> NetworkChannel --> server.in++;
            server.out++ --> NetworkChannel --> client[i].in;
        }
}