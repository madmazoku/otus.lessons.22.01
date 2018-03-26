#include <iostream>

#include "async.h"
#include "processor.h"

struct Context {
    Reader _r;
    ConsolePrint _cp;
    FilePrint _fp;

    Context(size_t n) : _r(n)
    {
        _r.subscribe(&_cp);
        _r.subscribe(&_fp);
    }
    virtual ~Context()
    {
        _r.done();
        _cp.done();
        _fp.done();
    }
};

namespace async
{

handle_t connect(std::size_t bulk)
{
    return reinterpret_cast<handle_t>(new Context(bulk));
}

void receive(handle_t handle, const char *data, std::size_t size)
{
    reinterpret_cast<Context*>(handle)->_r.push(data, size);
}

void disconnect(handle_t handle)
{
    delete reinterpret_cast<Context*>(handle);
}

}
