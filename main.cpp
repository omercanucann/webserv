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
        std::cerr << "[main] Config hatasi: " << e.what() << std::endl;
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
                << " acilamadi" << std::endl;
            return 1;
        }
        i++;
    }
    

    std::cerr << "[main] Sunucu hazir. Port(lar) dinleniyor..." << std::endl;
    std::cerr << "[main] Durdurmak icin Ctrl+C" << std::endl;
    reactor->run();

    std::cerr << "[main] Temiz cikis" << std::endl;
    return 0;
}
