#include <stdint.h>
#include <hls_stream.h>
#include <ap_int.h>

typedef double type_p0;
typedef double type_g0;

// for tests need a relatively small number here, so it doesn't cause issues with stack size on CPU
#define N (256*1024)

// For compatibility with OC-Accel, control structure from hls_snap_1024.H
typedef struct {
        ap_uint<8> sat; // short action type
        ap_uint<8> flags;
        ap_uint<16> seq;
        ap_uint<32> Retc;
        ap_uint<64> Reserved; // Priv_data
} CONTROL;

// Action register that is providing all configuration exchange between host and FPGA action in OC-Accel
struct action_reg {
        CONTROL control;
	uint64_t offset_in;
	uint64_t offset_out;
	uint64_t offset_p0;
	uint64_t offset_g0;
	uint64_t npackets;
        uint8_t padding[108 - 5*8]; // Size of action register is fixed by OC-Accel specification
};

void hls_action(ap_uint<512> *host_mem_in, ap_uint<512> *host_mem_out, action_reg *act);
