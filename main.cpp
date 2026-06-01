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
    if (argc > 1)
        fprintf(stderr, "[main] Konfigürasyon: %s (henüz ayrıştırılmıyor)\n", argv[1]);
    else
        fprintf(stderr, "[main] Varsayılan konfigürasyon kullanılıyor\n");

    SignalGuard::install();

    PollReactor reactor;
    HttpRequestHandler handler;
    ReactorBridge bridge(reactor, handler);

    bridge.activate();

    if (!reactor.add_server("0.0.0.0", 8080))
    {
        fprintf(stderr, "[main] Port 8080 açılamadı\n");
        return 1;
    }

    if (!reactor.add_server("0.0.0.0", 8081))
    {
        fprintf(stderr, "[main] Port 8081 açılamadı\n");
        return 1;
    }

    fprintf(stderr, "[main] Sunucu hazır. http://localhost:8080 adresini ziyaret edin\n");
    fprintf(stderr, "[main] Durdurmak için Ctrl+C\n");

    reactor.run();

    fprintf(stderr, "[main] Temiz çıkış\n");
    return 0;
}