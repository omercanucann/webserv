#include "../Config/ConfigParser.hpp"
#include "../Network_Server/Socketbinder.hpp"
#include "../Network_Server/Signalguard.hpp"
#include "../Network_Server/Signals.hpp"
#include "../Network_Server/Pollreactor.hpp"
#include "../Network_Server/Reactorbridge.hpp"
#include "../Http/HttpRequestHandler.hpp"
#include <iostream>
#include <cstring>
#include <string>

int main(int argc, char* argv[])
{
    std::string configPath;

    if (argc == 2)
        configPath = argv[1];
    else
        configPath = "configs/default.conf";

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

    PollReactor reactor;
    HttpRequestHandler handler(config, reactor);
    ReactorBridge bridge(reactor, handler);

    bridge.activate();

    size_t i = 0;
    while (i < config.servers.size())
    {
        if (!(reactor.add_server(config.servers[i].listenHost,config.servers[i].listenPort)))
        {
            std::cerr << "[main] Port " << config.servers[i].listenPort
                << " acilamadi" << std::endl;
            return 1;
        }
        i++;
    }
    

    std::cerr << "[main] Sunucu hazir. Port(lar) dinleniyor..." << std::endl;
    std::cerr << "[main] Durdurmak icin Ctrl+C" << std::endl;
    reactor.run();

    std::cerr << "[main] Temiz cikis" << std::endl;
    return 0;
}
