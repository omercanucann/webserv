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
        virtual ~IRequestHandler() {}
        virtual bool handle_data(int slot_index, ConnectionSlot &slot) = 0;
        virtual void handle_close(int slot_index) = 0;
};

class ReactorBridge
{
    public:
        ReactorBridge(PollReactor &reactor, IRequestHandler &handler);
        ~ReactorBridge();
        void activate();
        void respond(int slot_index, const std::string  &response);
        void respond(int slot_index, const std::vector<char>    &response);
    
    private:
        static void _on_data_static (void* ctx, ConnectionSlot  &slot);
        static void _on_write_static(void* ctx, ConnectionSlot  &slot); 
        static void _on_close_static(void* ctx, ConnectionSlot  &slot);
    
        void _on_data (ConnectionSlot   &slot);
        void _on_write(ConnectionSlot   &slot);
        void _on_close(ConnectionSlot   &slot);
    
        PollReactor&     _reactor;
        IRequestHandler& _handler;
    
        ReactorBridge(const ReactorBridge&);   
        ReactorBridge& operator=(const ReactorBridge&);
};

#endif