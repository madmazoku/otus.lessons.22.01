#include <iostream>

#include "async.h"
#include "processor.h"

struct GlobalState {
    Metrics* _m;
    ConsolePrint* _cp;
    FilePrint* _fp;

    GlobalState()
    {
        _m = new Metrics;
        _cp = new ConsolePrint(_m);
        _fp = new FilePrint(_m);
    }

    ~GlobalState()
    {
        _cp->done();
        _fp->done();

        delete _cp;
        delete _fp;

        _m->dump();

        delete _m;
    }
} g_state;

namespace async
{

handle_t connect(std::size_t bulk)
{
    Reader* r = new Reader(g_state._m, bulk);
    r->subscribe(g_state._cp);
    r->subscribe(g_state._fp);
    return reinterpret_cast<handle_t>(r);
}

void receive(handle_t handle, const char *data, std::size_t size)
{
    reinterpret_cast<Reader*>(handle)->push(data, size);
}

void disconnect(handle_t handle)
{
    Reader* r = reinterpret_cast<Reader*>(handle);
    r->done();
    delete r;
}

}
