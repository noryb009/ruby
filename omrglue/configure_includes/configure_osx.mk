###############################################################################
#
# (c) Copyright IBM Corp. 2016, 2016
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

# Detect 64-bit vs. 32-bit
ifneq (,$(findstring -64,$(SPEC)))
  TEMP_TARGET_DATASIZE:=64
else
  TEMP_TARGET_DATASIZE:=32
endif

CONFIGURE_ARGS += \
  --enable-OMR_GC \
  --disable-OMR_JIT \
  --enable-OMR_PORT \
  --disable-OMR_OMRSIG \
  --enable-OMR_THREAD

CONFIGURE_ARGS += --enable-fvtest-agent

ifeq (osx_x86-64, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_GC_OBJECT_MAP \
		--enable-OMR_GC_TLH_PREFETCH_FTA \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT \
		--enable-OMR_THR_FORK_SUPPORT \
		--enable-OMR_THR_THREE_TIER_LOCKING \
		--enable-OMR_THR_YIELD_ALG \
		--enable-OMR_GC_ARRAYLETS
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS += 'AS=clang'
CONFIGURE_ARGS += 'CC=clang'
CONFIGURE_ARGS += 'CXX=clang++'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CXX)'
CONFIGURE_ARGS += 'CXXLINKSHARED=$$(CXX)'
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=osx'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=x86'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'
