AUTOMAKE_OPTIONS = foreign subdir-objects

MAINTAINERCLEANFILES = aclocal.m4 autom4te.cache compile config.og autoscan.og config.guess config.status config.sub configure configure.scan depcomp install-sh ltmain.sh Makefile Makefile.in

AM_CFLAGS = -I./include

CLEANFILES = libexpreval.@SHARED_EXTENSION@ map.sym

# Static library

lib_LIBRARIES = libexpreval.a
include_HEADERS = include/expreval.h

libexpreval_a_SOURCES = src/lib/eval.c  src/lib/jit_compiler.c  src/lib/parser.c  src/lib/tokenizer.c src/lib/list.c


if ARCH_i686
libexpreval_a_SOURCES += src/lib/arch/@ARCH@/@OS@/linux_i686.c
libexpreval_a_LIBADD = src/lib/arch/@ARCH@/cpuid.o
CLEANFILES += src/lib/arch/@ARCH@/cpuid.o
src/lib/arch/i686/cpuid.o:
	$(AS) -f elf32 src/lib/arch/@ARCH@/cpuid.asm -o src/lib/arch/@ARCH@/cpuid.o
endif

if ARCH_x86_64
libexpreval_a_SOURCES += src/lib/arch/@ARCH@/@OS@/linux_x86_64.c
libexpreval_a_LIBADD = src/lib/arch/@ARCH@/cpuid.o
CLEANFILES += src/lib/arch/@ARCH@/cpuid.o
src/lib/arch/x86_64/cpuid.o:
	$(AS) -f elf64 src/lib/arch/@ARCH@/cpuid.asm -o src/lib/arch/@ARCH@/cpuid.o
endif

if ARCH_AARCH64
libexpreval_a_SOURCES += src/lib/arch/@ARCH@/@OS@/linux_aarch64.c
endif

# Shared library
if MAKE_SHARED_LIBRARY
shared_library: libexpreval.a map.sym
	$(CCLD) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -fPIC -Wl,--version-script=map.sym -shared $(libexpreval_a_LIBADD) $$(echo $(libexpreval_a_SOURCES) | $(SED) "s/\.c/.o/g") -o libexpreval.@SHARED_EXTENSION@
	rm map.sym

install-exec-local: libexpreval.@SHARED_EXTENSION@
	cp libexpreval.@SHARED_EXTENSION@ ${libdir}

uninstall-local:
	rm ${libdir}/libexpreval.@SHARED_EXTENSION@

map.sym: include/expreval.h
	echo -e "{\n\tglobal:" > map.sym
	$(AWK) -F'[ (]' '(/.+\(.*\);/ && !match($$1, /typedef/)){sub("*","",$$2);print "\t\t"$$2";";}' include/expreval.h >> map.sym
	echo -e "\tlocal:\n\t\t*;\n};" >> map.sym

else
shared_library:
endif

### Test programs

noinst_PROGRAMS = performance_test prova test_main

performance_test_SOURCES = src/test/performance_test.c
performance_test_LDADD = libexpreval.a

prova_SOURCES = src/test/prova.c
prova_LDADD = libexpreval.a

test_main_SOURCES = src/test/test_main.c
test_main_LDADD = libexpreval.a

all-local: shared_library
