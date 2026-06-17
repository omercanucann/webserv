#include "../Config/ConfigParser.hpp"
#include "../Network_Server/Socketbinder.hpp"
#include "../Network_Server/Signalguard.hpp"
#include "../Network_Server/Pollreactor.hpp"
#include "../Network_Server/Reactorbridge.hpp"
#include "../Http/HttpRequestHandler.hpp"
#include <cstdio>
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
        fprintf(stderr, "[main] Config hatasi: %s\n", e.what());
        return 1;
    }

    SignalGuard::install();

    PollReactor reactor;
    HttpRequestHandler handler(config);
    ReactorBridge bridge(reactor, handler);

    bridge.activate();

    size_t i = 0;
    while (i < config.servers.size())
    {
        if (!(reactor.add_server(config.servers[i].listenHost,config.servers[i].listenPort)))
        {
            fprintf(stderr, "[main] Port %d acilamadi\n", config.servers[i].listenPort);
            return 1;
        }
        i++;
    }
    

    fprintf(stderr, "[main] Sunucu hazır. Port(lar) dinleniyor...\n");
    fprintf(stderr, "[main] Durdurmak için Ctrl+C\n");

    reactor.run();

    fprintf(stderr, "[main] Temiz çıkış\n");
    return 0;
}