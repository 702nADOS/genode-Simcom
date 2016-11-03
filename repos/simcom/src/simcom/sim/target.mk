TARGET = simcom_sim
SRC_CC = main_new.cc GenericPacket
LIBS   = base lwip libc stdcxx
REQUIRES = foc

#LWIP_PORT_DIR := $(call select_from_ports,lwip)
#LWIP_DIR      := $(LWIP_PORT_DIR)/src/lib/lwip

INC_DIR += $(REP_DIR)/../libports/include/lwip
