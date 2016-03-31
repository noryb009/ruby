###############################################################################
#
# (c) Copyright IBM Corp. 2015, 2016
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
  --enable-OMR_PORT \
  --disable-OMR_OMRSIG \
  --enable-OMR_THREAD

ifeq (linux_390-64, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_OBJECT_MAP \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_RUNTIME_INSTRUMENTATION \
		--enable-OMR_THR_FORK_SUPPORT \
		--enable-OMR_THR_THREE_TIER_LOCKING \
		--enable-OMR_THR_YIELD_ALG \
		--enable-OMR_GC_ARRAYLETS
endif

CONFIGURE_ARGS += --enable-fvtest-agent
CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS += 'AS=as'
CONFIGURE_ARGS += 'CC=cc'
CONFIGURE_ARGS += 'CXX=$$(CC)'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=gcc'
CONFIGURE_ARGS += 'CXXLINKEXE=c++'
CONFIGURE_ARGS += 'CXXLINKSHARED=c++'
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=linux'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=s390'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'
