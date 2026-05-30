#include "../Network_Server/Socketbinder.hpp"
#include "../Network_Server/Signalguard.hpp"
#include "../Network_Server/Pollreactor.hpp"
#include <cstdio>
#include <cstring>
#include <string>


static const char DEMO_RESPONSE[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: 60\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<html><body><h1>webserv calistiyor!</h1></body></html>\r\n";


struct ReactorContext
{
    PollReactor* reactor;
};

static void on_data_cb(void* ctx, ConnectionSlot& slot)
{
    ReactorContext* rctx = static_cast<ReactorContext*>(ctx);

    fprintf(stderr, "[Demo] slot fd=%d'dan %zu bayt alındı\n",slot.fd, slot.readbuffer.size());

    std::string raw(slot.readbuffer.begin(), slot.readbuffer.end());
    if (raw.find("\r\n\r\n") != std::string::npos)
    {
        fprintf(stderr, "[Demo] İstek tamamlandı, yanıt gönderiliyor\n");
        rctx->reactor->queue_response(slot.self_index, DEMO_RESPONSE);
        (void)rctx; // Şu an kullanılmıyor
    }
}

static void on_write_cb(void* ctx, ConnectionSlot& slot)
{
    fprintf(stderr, "[Demo] slot fd=%d yanıt tamamlandı\n", slot.fd);
    slot.state = ConnectState_CLOSING;
    (void)ctx;
}

static void on_close_cb(void* ctx, ConnectionSlot& slot)
{
    fprintf(stderr, "[Demo] slot fd=%d bağlantı kapatılıyor (%s:%u)\n", slot.fd, slot.remote_ip.c_str(),
            (unsigned)slot.remote_port);
    (void)ctx;
}

int main(int argc, char* argv[])
{
    if (argc > 1)
        fprintf(stderr, "[main] Konfigürasyon: %s (henüz ayrıştırılmıyor)\n",
                argv[1]);
    else
        fprintf(stderr, "[main] Varsayılan konfigürasyon kullanılıyor\n");

    SignalGuard::install();
    PollReactor reactor;
    ReactorContext ctx;
    ctx.reactor = &reactor;
    reactor.set_callbacks(on_data_cb, on_write_cb, on_close_cb, &ctx);

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