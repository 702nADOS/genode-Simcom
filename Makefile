# options: x86 arm
TOOLCHAIN_TARGET    ?= arm

# options: see tool/create_builddir
GENODE_TARGET       ?= foc_panda

BUILD_DIR           ?= build
TOOLCHAIN_BUILD_DIR ?= $(BUILD_DIR)/toolchain-$(TOOLCHAIN_TARGET)
GENODE_BUILD_DIR    ?= $(BUILD_DIR)/genode-$(GENODE_TARGET)
BUILD_CONF           = $(GENODE_BUILD_DIR)/etc/build.conf

.PHONY: all toolchain ports genode genode_build_dir clean

all: toolchain ports genode_build_dir genode-simcom genode-qemusavm

toolchain:
	mkdir -p $(TOOLCHAIN_BUILD_DIR)
	cd $(TOOLCHAIN_BUILD_DIR);\
	../../tool/tool_chain $(TOOLCHAIN_TARGET)

ports: foc dde_linux libports

foc:
	$(MAKE) -C repos/base-foc prepare

dde_linux:
	$(MAKE) -C repos/dde_linux prepare

libports:
	$(MAKE) -C repos/libports prepare

genode_build_dir:
	tool/create_builddir $(GENODE_TARGET) BUILD_DIR=$(GENODE_BUILD_DIR)
	printf 'REPOSITORIES += $$(GENODE_DIR)/repos/libports\n' >> $(BUILD_CONF)
	printf 'REPOSITORIES += $$(GENODE_DIR)/repos/dde_linux\n' >> $(BUILD_CONF)
	printf 'REPOSITORIES += $$(GENODE_DIR)/repos/ecu\n' >> $(BUILD_CONF)
	printf 'REPOSITORIES += $$(GENODE_DIR)/repos/simcom\n' >> $(BUILD_CONF)
	printf 'REPOSITORIES += $$(GENODE_DIR)/repos/qemusavm\n' >> $(BUILD_CONF)

genode-simcom:
	$(MAKE) -j10 -C $(GENODE_BUILD_DIR) run/simcom

genode-qemusavm:
	$(MAKE) -j10 -C $(GENODE_BUILD_DIR) run/qemusavm

clean:
	rm -rf $(BUILD_DIR)
