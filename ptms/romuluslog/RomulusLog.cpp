
#include "RomulusLog.hpp"

namespace romuluslog {

// Global with the 'main' size. Used by pload()
uint64_t g_main_size = 0;
// Global with the 'main' addr. Used by pload()
uint8_t* g_main_addr = 0;

// Counter of nested write transactions
thread_local int64_t tl_nested_write_trans = 0;
// Counter of nested read-only transactions
thread_local int64_t tl_nested_read_trans = 0;
bool histoOn = false;
bool histoflag = false;
RomulusLog gRomLog {};

/*
 * <h1> Romulus Log </h1>
* TODO: explain this...
*
*
*
*/




}
