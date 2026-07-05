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

void ReactorBridge::respond(int slot_index, const std::string &response)
{
    _reactor.queue_response(slot_index, response);
}
 
void ReactorBridge::respond(int slot_index, const std::vector<char>& response)
{
    _reactor.queue_response(slot_index, response);
}
 
void ReactorBridge::_on_data_static(void *context, ConnectionSlot &slot)
{
    static_cast<ReactorBridge*>(context)->_on_data(slot);
}
 
void ReactorBridge::_on_write_static(void *context, ConnectionSlot &slot)
{
    static_cast<ReactorBridge*>(context)->_on_write(slot);
}
 
void ReactorBridge::_on_close_static(void *context, ConnectionSlot &slot)
{
    static_cast<ReactorBridge*>(context)->_on_close(slot);
}
 
void ReactorBridge::_on_data(ConnectionSlot &slot)
{
    if (slot.self_index < 0)
    {
        std::cout << "[ReactorBridge] ERROR: self_index not set fd=" << slot.fd << std::endl;
        return;
    }
 
    if (_handler.handle_data(slot.self_index, slot))
        _reactor.queue_response(slot.self_index, slot.writebuffer);
}
 
void ReactorBridge::_on_write(ConnectionSlot &slot)
{
    std::cout << "[ReactorBridge] The answer is complete: slot[" << slot.self_index << "] fd=" << slot.fd << std::endl;

    slot.state = ConnectState_CLOSING;
}
 
void ReactorBridge::_on_close(ConnectionSlot &slot)
{
    if (slot.self_index >= 0)
        _handler.handle_close(slot.self_index);
 
    std::cout << "[ReactorBridge] Connection closed: slot[" << slot.self_index << "] fd=" << slot.fd << std::endl;
}