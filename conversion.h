#include <stdint.h>
#include <hls_stream.h>
#include <ap_int.h>

#define type_p  double
#define type_g0 double
#define type_g1 double
#define type_g2 double

struct action_reg {
	uint64_t offset_in;
	uint64_t offset_out;
	uint64_t npackets;
};

ap_int<16> xray_convert(ap_uint<16> input,
		type_g0 g0, type_g1 g1, type_g2 g2,
		type_p  p0, type_p  p1, type_p  p2);

void convert_packet(ap_uint<512> *host_mem_in, ap_uint<512> *host_mem_out,
					type_g0 *g0, type_g1 *g1, type_g2 *g2,
					type_p *p0, type_p *p1, type_p *p2,
					action_reg *act);
