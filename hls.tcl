open_project "conversion-example" -reset

set_top convert_packet

foreach file [ list conversion.cpp conversion.h] {
  add_files ${file}
}

open_solution solution1

# Outcome will work for all Virtex US+ HBM FPGAs (this is mostly for utilization statistics)
set_part xcvu33p-fsvh2104-2-e

create_clock -period 2.5 -name default
config_interface -m_axi_addr64=true
config_schedule -enable_dsp_full_reg=true

csynth_design

close_project
exit
