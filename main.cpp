#include "../Config/ConfigParser.hpp"
#include "../Network_Server/Socketbinder.hpp"
#include "../Network_Server/Signalguard.hpp"
#include "../Network_Server/Signals.hpp"
#include "../Network_Server/Pollreactor.hpp"
#include "../Network_Server/Reactorbridge.hpp"
#include "HttpRequestHandler.hpp"
#include <iostream>
#include <cstring>
#include <string>
#include <memory>

int main(int argc, char* argv[])
{
    std::string configPath;

    if (argc == 2)
        configPath = argv[1];
    else if (argc == 1)
        configPath = "configs/default.conf";
    else
        return (1);

    ConfigParser parser;
    Config config;

    try
    {
        config = parser.parseConfig(configPath);
    }
    catch (const ConfigParser::ParseException &e)
    {
        std::cerr << "[main] Config error: " << e.what() << std::endl;
        return 1;
    }

    SignalGuard::install();
    SignalHandler::install();

    std::auto_ptr<PollReactor> reactor(new PollReactor());
    std::auto_ptr<HttpRequestHandler> handler(new HttpRequestHandler(config, *reactor));
    std::auto_ptr<ReactorBridge> bridge(new ReactorBridge(*reactor, *handler));

    bridge->activate();

    size_t i = 0;
    while (i < config.servers.size())
    {
        if (!(reactor->add_server(config.servers[i].listenHost,config.servers[i].listenPort)))
        {
            std::cerr << "[main] Port " << config.servers[i].listenPort
                << " can't open" << std::endl;
            return 1;
        }
        i++;
    }
    

    std::cerr << "[main] Server is ready. Ports are listening..." << std::endl;
    std::cerr << "[main] For stop Ctrl+C" << std::endl;
    reactor->run();

    std::cerr << "[main] Clearly exit" << std::endl;
    return 0;
}
