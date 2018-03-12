#include <iostream>

#include "async.h"
#include "processor.h"

ConsolePrint g_cp;
FilePrint g_fp;

void __attribute__((destructor)) _fini(void)
{
	std::cout << "_fini" << std::endl;
	// g_cp.done();
	// g_fp.done();
	std::cout << "end _fini" << std::endl;
}

namespace async {

handle_t connect(std::size_t bulk) {
	Reader* reader = new Reader(bulk);
	reader->subscribe(&g_cp);
	reader->subscribe(&g_fp);
	return reinterpret_cast<handle_t>(reader);
}

void receive(handle_t handle, const char *data, std::size_t size) {
	reinterpret_cast<Reader*>(handle)->push(data, size);
}

void disconnect(handle_t handle) {
	Reader* reader = reinterpret_cast<Reader*>(handle);
	reader->done();
	delete reader;	
}

}
