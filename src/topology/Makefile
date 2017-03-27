# ============================================
#
# Make file for TMR_DIR/src/
#
# ============================================

include ../../Makefile.in
include ../../TMR_Common.mk
ifdef ${TMR_HAS_PAROPT}
include ${PAROPT_DIR}/Makefile.in
include ${PAROPT_DIR}/ParOpt_Common.mk
endif

CXX_OBJS = TMRTopoProblem.o \
	TMROctStiffness.o

DIR=${TMR_DIR}/src/topology

CXX_OBJS := $(CXX_OBJS:%=$(DIR)/%)

default: ${CXX_OBJS}
	${AR} ${AR_FLAGS} ${TMR_LIB} ${CXX_OBJS}

debug: TMR_CC_FLAGS=${TMR_DEBUG_CC_FLAGS}
debug: default

clean:
	${RM} *.o