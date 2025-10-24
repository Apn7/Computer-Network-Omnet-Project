package http_predictive_cache;

import ned.DatarateChannel;

//
// HTTP Predictive Caching Network
// Network topology with 10 HTTP clients connected to 1 HTTP server
//
network HttpNetwork
{
    parameters:
        @display("bgb=800,600");
        
    types:
        channel NetworkChannel extends DatarateChannel
        {
            datarate = 100Mbps;
            delay = 10ms;
        }
        
    submodules:
        server: HttpServer {
            @display("p=400,100");
        }
        
        client[10]: HttpClient {
            @display("p=100+i*70,300");
        }
        
    connections:
        for i=0..9 {
            client[i].out --> NetworkChannel --> server.in++;
            server.out++ --> NetworkChannel --> client[i].in;
        }
}