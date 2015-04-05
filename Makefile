# SHELL:=/bin/bash

all:

install:
	echo "export DATA_CHALLENGES_ROOT=$(PWD)" >setup.sh
	echo "export LD_LIBRARY_PATH=$(PIPELINE_TOOLKIT_ROOT)/lib:$(LD_LIBRARY_PATH)" >>setup.sh
