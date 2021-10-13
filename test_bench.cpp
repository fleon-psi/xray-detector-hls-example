#include <stdint.h>
#include <random>
#include <sys/mman.h>

#include "conversion.h"

template <class T>
T *mem_alloc(size_t nelems) {
	void *ptr = mmap(nullptr, nelems * sizeof(T), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ptr == nullptr) {
		std::cout << "Mem alloc error\n" << std::endl;
		exit(EXIT_FAILURE);
	}
	return (T *) ptr;
}

int main(int argc, char **argv) {
	// Allocate memory for communication with FPGA - page aligned
	auto input_data = mem_alloc<uint16_t>(N);
	auto output_data = mem_alloc<int16_t>(N);
	auto g0 = mem_alloc<type_g0>(N);
	auto p0 = mem_alloc<type_p0>(N);

	// Allocate memory for local-only storage (alignment not relevant)
	std::vector<double> ref(N);

	// Generate random input, p0, p1, p2, g0, g1, g2 arrays
	// But we use preset random generator seed, so the result is always the same
	std::mt19937 random_gen(16781);
	std::uniform_int_distribution<uint16_t> dist(0, 16383);

	std::uniform_real_distribution<double> p0_dist(0, 16383);
	std::uniform_real_distribution<double> g0_dist(1.0, 1.99999);

	for (int i = 0; i < N; i++) {
		// Make random p0 and g0 coefficients
		double fp_p0 = p0_dist(random_gen);
		double fp_g0 = g0_dist(random_gen);

		// Cast constants to whatever type used on FPGA
		p0[i] = fp_p0;
		g0[i] = fp_g0;

		// Make random input data
		input_data[i] = dist(random_gen);

		// Calculate reference solution
		double adu = input_data[i] & 0x3fff;
		ref[i] = (adu - fp_p0) * fp_g0;
	}

	// Set up action register
	action_reg reg;
	reg.npackets   = N / 32 ;
	reg.offset_in  = (uint64_t) input_data;
	reg.offset_out = (uint64_t) output_data;
	reg.offset_p0  = (uint64_t) p0;
	reg.offset_g0  = (uint64_t) g0;

	// Run conversion procedure
	convert_packet(nullptr, nullptr, &reg);

	// Calculate root mean square deviation (error measure)
	double err = 0;
	for (int i = 0; i < N; i++)
		err += (ref[i] - output_data[i]) * (ref[i] - output_data[i]);
	err = sqrt(err/N);
	std::cout << "error = " << err << " (0.25 is ideal value for integer rounding; less than 0.35 is good enough)" << std::endl;

	// Deallocate memory
	munmap( input_data, N * sizeof(uint16_t));
	munmap(output_data, N * sizeof( int16_t));
	munmap(p0, N * sizeof(type_p0));
	munmap(g0, N * sizeof(type_g0));

	if (err < 0.5) return EXIT_SUCCESS;
	else return EXIT_FAILURE;
}
