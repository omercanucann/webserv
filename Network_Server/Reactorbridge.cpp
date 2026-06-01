#include "Reactorbridge.hpp"

ReactorBridge::ReactorBridge(PollReactor &reactor, IRequestHandler &handler): _reactor(reactor), _handler(handler)
{}

ReactorBridge::~ReactorBridge()
{}

void ReactorBridge::activate()
{
    _reactor.set_callbacks(
        _on_data_static,
        _on_write_static,
        _on_close_static,
        this
    );
}

void ReactorBridge::respond(int slot_index, const std::string &response) // respond: Belirli bir slot için gönderilecek yanıtı PollReactor'a iletir, bu genellikle slot.writebuffer'ına yanıt eklemek ve yazma olaylarını 
// tetiklemek anlamına gelir, böylece PollReactor bu verileri istemciye gönderebilir
{
    _reactor.queue_response(slot_index, response); // queue_response: Belirli bir slot için yanıt verilerini kuyruğa ekler, bu fonksiyon genellikle slot index'inin geçerli olup olmadığını kontrol eder,
    // slotun boş olmadığını ve dosya tanıtıcısının geçerli olduğunu kontrol eder, eğer tüm kontroller geçerse, slotun writebuffer'ına yanıt verilerini ekler ve pollfd yapısındaki events alanını POLLOUT olarak ayarlar, 
    // böylece poll() fonksiyonu bu fd üzerinden gelen yazma olaylarını izler
}
 
void ReactorBridge::respond(int slot_index, const std::vector<char>& response) // 
{
    _reactor.queue_response(slot_index, response);
}
 
void ReactorBridge::_on_data_static(void *context, ConnectionSlot &slot)  // PollReactor'ın callback'leri genellikle statik fonksiyonlar olarak tanımlanır, çünkü PollReactor bu fonksiyonları belirli bir nesne bağlamı olmadan
//  çağırır, bu statik fonksiyonlar ctx parametresi aracılığıyla ReactorBridge nesnesine erişir ve ardından ilgili üye fonksiyonları çağırarak olayları işler
{
    static_cast<ReactorBridge*>(context)->_on_data(slot); // static_cast: Bu, context parametresini ReactorBridge* türüne dönüştürür, böylece bu statik fonksiyon içinde ReactorBridge nesnesine erişebiliriz, ardından 
    // _on_data üye fonksiyonunu çağırarak veri okuma olayını işler
}
 
void ReactorBridge::_on_write_static(void *context, ConnectionSlot &slot) // PollReactor'ın yazma olayları için statik callback fonksiyonu, bu fonksiyon yazma işlemi tamamlandığında çağrılır, ctx parametresi aracılığıyla 
// ReactorBridge nesnesine erişir ve ardından _on_write üye fonksiyonunu çağırarak yazma olayını işler
{
    static_cast<ReactorBridge*>(context)->_on_write(slot); // static_cast: Bu, context parametresini ReactorBridge* türüne dönüştürür, böylece bu statik fonksiyon içinde ReactorBridge nesnesine erişebiliriz, 
    // ardından _on_write üye fonksiyonunu çağırarak yazma olayını işler
}
 
void ReactorBridge::_on_close_static(void *context, ConnectionSlot &slot) // on_close_static: PollReactor'ın kapatma olayları için statik callback fonksiyonu, bu fonksiyon bağlantı kapandığında çağrılır,
// ctx parametresi aracılığıyla ReactorBridge nesnesine erişir ve ardından _on_close üye fonksiyonunu çağırarak kapatma olayını işler
{
    static_cast<ReactorBridge*>(context)->_on_close(slot); // static_cast: Bu, context parametresini ReactorBridge* türüne dönüştürür, böylece bu statik fonksiyon içinde ReactorBridge nesnesine erişebiliriz, 
    //ardından _on_close üye fonksiyonunu çağırarak kapatma olayını işler
}
 
void ReactorBridge::_on_data(ConnectionSlot &slot) // PollReactor'dan gelen veri okuma olaylarını işler, bu fonksiyon genellikle IRequestHandler'ın handle_data fonksiyonunu çağırarak slot.readbuffer'ındaki verileri işler 
// ve gerekirse slot.writebuffer'ına yanıt ekler
{
    if (slot.self_index < 0) // Eğer slotun self_index'i geçerli değilse, bu genellikle bir hata durumunu gösterir, bu durumda hata mesajı yazdırılır ve fonksiyon döner, 
    //böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
    {
        std::cout << "[ReactorBridge] HATA: self_index ayarlanmamis fd=" << slot.fd << std::endl;
        return;
    }
 
    if (_handler.handle_data(slot.self_index, slot))
        _reactor.queue_response(slot.self_index, slot.writebuffer);
}
 
void ReactorBridge::_on_write(ConnectionSlot &slot) // PollReactor'dan gelen yazma olaylarını işler, bu fonksiyon genellikle slot.writebuffer'ındaki verilerin gönderilmesi tamamlandığında çağrılır, 
// bu noktada yazma işlemi tamamlanmış olur ve gerekirse IRequestHandler'ın handle_write gibi bir fonksiyonunu çağırarak yazma işlemi tamamlandığını bildirebilir
{
    std::cout << "[ReactorBridge] Yanit tamamlandi: slot[" << slot.self_index << "] fd=" << slot.fd << std::endl;

    slot.state = ConnectState_CLOSING;
}
 
void ReactorBridge::_on_close(ConnectionSlot &slot) // PollReactor'dan gelen kapatma olaylarını işler, bu fonksiyon genellikle IRequestHandler'ın handle_close fonksiyonunu çağırarak slot_index ile ilişkili kaynakları temizlemek 
// ve uygulama düzeyinde kapanış işlemleri yapmak için kullanılır
{
    if (slot.self_index >= 0) // Eğer slotun self_index'i geçerli ise, bu durumda IRequestHandler'ın handle_close fonksiyonunu çağırarak slot_index ile ilişkili kaynakları temizlemek ve uygulama düzeyinde kapanış işlemleri 
    // yapmak için kullanılır
        _handler.handle_close(slot.self_index); // IRequestHandler'ın handle_close fonksiyonunu çağırarak slot_index ile ilişkili kaynakları temizlemek ve uygulama düzeyinde kapanış işlemleri yapmak için kullanılır
 
    std::cout << "[ReactorBridge] Baglanti kapatildi: slot[" << slot.self_index << "] fd=" << slot.fd << std::endl;
}