#ifndef REACTORBRIDGE_HPP
#define REACTORBRIDGE_HPP

#include "Nettypes.hpp"
#include "Connectionslot.hpp"
#include "Pollreactor.hpp"
#include <string>
#include <vector>
#include <cstdio>

class IRequestHandler
{
    public:
        virtual ~IRequestHandler() {} // ne işe yarar? : Sanal bir yıkıcı, bu sınıfın bir arayüz olduğunu belirtir ve türetilmiş sınıfların kendi yıkıcılarını tanımlamasına izin verir, ayrıca bu sınıfın nesnelerinin doğrudan oluşturulmasını engeller
        virtual bool handle_data(int slot_index, ConnectionSlot &slot) = 0; // Yeni veri okunduğunda çağrılır, slot.readbuffer'ında yeni veriler bulunur, bu fonksiyonun görevi bu verileri işlemek ve gerekirse slot.writebuffer'ına yanıt eklemektir, true döndürürse slot.writebuffer'ındaki verilerin gönderilmesi gerektiği anlamına gelir, false döndürürse yazma işlemi yapılmaz
        virtual void handle_close(int slot_index) = 0; // Bağlantı kapandığında çağrılır, bu fonksiyonun görevi slot_index ile ilişkili kaynakları temizlemek ve gerekirse uygulama düzeyinde kapanış işlemleri yapmaktır, örneğin oturum sonlandırma, kaynak serbest bırakma vb.
};

class ReactorBridge
{
    public:
        ReactorBridge(PollReactor &reactor, IRequestHandler &handler); // PollReactor ve IRequestHandler referanslarını alır, bu sınıfın görevi bu iki bileşen arasında köprü görevi görmektir, PollReactor'dan gelen olayları IRequestHandler'a iletmek ve IRequestHandler'ın yanıtlarını PollReactor'a iletmektir
        ~ReactorBridge();
        void activate();
        void respond(int slot_index, const std::string  &response); // string versiyonu, vector<char> versiyonunu çağırır
        void respond(int slot_index, const std::vector<char>    &response); // Belirli bir slot için gönderilecek yanıtı PollReactor'a iletir, bu genellikle slot.writebuffer'ına yanıt eklemek ve yazma olaylarını tetiklemek anlamına gelir, böylece PollReactor bu verileri istemciye gönderebilir
    
    private:
        static void _on_data_static (void* ctx, ConnectionSlot  &slot); // PollReactor'ın callback'leri genellikle statik fonksiyonlar olarak tanımlanır, çünkü PollReactor bu fonksiyonları belirli bir nesne bağlamı olmadan çağırır, bu statik fonksiyonlar ctx parametresi aracılığıyla ReactorBridge nesnesine erişir ve ardından ilgili üye fonksiyonları çağırarak olayları işler
        static void _on_write_static(void* ctx, ConnectionSlot  &slot); // PollReactor'ın yazma olayları için statik callback fonksiyonu, bu fonksiyon yazma işlemi tamamlandığında çağrılır, ctx parametresi aracılığıyla ReactorBridge nesnesine erişir ve ardından _on_write üye fonksiyonunu çağırarak yazma olayını işler
        static void _on_close_static(void* ctx, ConnectionSlot  &slot); // PollReactor'ın kapatma olayları için statik callback fonksiyonu, bu fonksiyon bağlantı kapandığında çağrılır, ctx parametresi aracılığıyla ReactorBridge nesnesine erişir ve ardından _on_close üye fonksiyonunu çağırarak kapatma olayını işler
    
        void _on_data (ConnectionSlot   &slot); // PollReactor'dan gelen veri okuma olaylarını işler, bu fonksiyon genellikle IRequestHandler'ın handle_data fonksiyonunu çağırarak slot.readbuffer'ındaki verileri işler ve gerekirse slot.writebuffer'ına yanıt ekler
        void _on_write(ConnectionSlot   &slot); // PollReactor'dan gelen yazma olaylarını işler, bu fonksiyon genellikle slot.writebuffer'ındaki verilerin gönderilmesi tamamlandığında çağrılır, bu noktada yazma işlemi tamamlanmış olur ve gerekirse IRequestHandler'ın handle_write gibi bir fonksiyonunu çağırarak yazma işlemi tamamlandığını bildirebilir
        void _on_close(ConnectionSlot   &slot); // PollReactor'dan gelen kapatma olaylarını işler, bu fonksiyon genellikle IRequestHandler'ın handle_close fonksiyonunu çağırarak slot_index ile ilişkili kaynakları temizlemek ve uygulama düzeyinde kapanış işlemleri yapmak için kullanılır
    
        PollReactor&     _reactor; // Bu sınıfın kullandığı PollReactor örneğine referans, bu sayede PollReactor'ın fonksiyonlarını çağırarak olayları yönetebiliriz
        IRequestHandler& _handler; // Bu sınıfın kullandığı IRequestHandler örneğine referans, bu sayede gelen verileri işlemek ve yanıtlar oluşturmak için IRequestHandler'ın fonksiyonlarını çağırabiliriz   
    
        ReactorBridge(const ReactorBridge&); // Kopyalama yapıcı ve atama operatörünü özel olarak tanımlayarak bu sınıfın kopyalanmasını engelleriz, çünkü bu sınıfın içinde referanslar bulunur ve kopyalama işlemi bu referansların geçersiz hale gelmesine neden olabilir, bu nedenle bu fonksiyonları tanımlayıp gövdesini boş bırakarak veya silerek kopyalamayı engelleriz    
        ReactorBridge& operator=(const ReactorBridge&); // Kopyalama yapıcı ve atama operatörünü özel olarak tanımlayarak bu sınıfın kopyalanmasını engelleriz, çünkü bu sınıfın içinde referanslar bulunur ve kopyalama işlemi bu referansların geçersiz hale gelmesine neden olabilir, bu nedenle bu fonksiyonları tanımlayıp gövdesini boş bırakarak veya silerek kopyalamayı engelleriz 
};

#endif