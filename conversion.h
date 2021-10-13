#include <stdint.h>
#include <hls_stream.h>
#include <ap_int.h>

typedef double type_p0;
typedef double type_g0;

//typedef ap_ufixed<16, 14, AP_RND_CONV> type_p0;
//typedef ap_ufixed<16,  1, AP_RND_CONV> type_g0;

// for tests need a relatively small number here, so it doesn't cause issues with stack size on CPU
#define N (256*1024)

struct action_reg {
	uint64_t offset_in;
	uint64_t offset_out;
	uint64_t offset_p0;
	uint64_t offset_g0;
	uint64_t npackets;
};

void convert_packet(ap_uint<512> *host_mem_in, ap_uint<512> *host_mem_out, action_reg *act);
