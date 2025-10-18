bin_PROGRAMS=\
	broker
	
broker_SOURCES=\
		src/main.cc \
		src/PlaySound.h \
		src/PlaySound.cc \
		src/App.h \
		src/App.cc \
		src/Configfile2.h \
		src/Configfile2.cc \
		src/ConfigGlobal.h \
		src/ConfigGlobal.cc \
		src/ConfigDatabase.h \
		src/ConfigDatabase.cc \
		src/bindtypes.h \
		src/bindtypes.cc \
		tools_config.h
		
AM_CPPFLAGS = -I$(top_srcdir)/tools \
	-I$(top_srcdir)/cpputils/cpputilsshared  \
	-I$(top_srcdir)/cpputils/cpputilsshared/cpputilsformat \
	-I$(top_srcdir)/cpputils/io \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/db \
	-std=gnu++23 \
	-fstack-protector \
	-D_GLIBCXX_USE_CXX11_ABI=1 \
	`pkg-config --cflags SDL2_mixer` \
	`pkg-config --cflags mysqlclient`

noinst_LIBRARIES=cpputils/io/libcpputilsio.a \
	cpputils/cpputilsshared/libcpputilsshared.a \
	cpputils/cpputilsshared/cpputilsformat/libcpputilsformat.a \
	common/libcommon.a \
	db/libdb.a
	
db_libdb_a_SOURCES= \
		db/db.h \
		db/db.cc \
		db/dbi.h \
		db/dbi.cc \
		db/database.cc \
		db/database.h \
		db/mysql_db.cc \
		db/oracle_db.h \
		db/oracle_db.cc \
		db/odbc_db.h \
		db/odbc_db.cc

cpputils_io_libcpputilsio_a_SOURCES= \
	cpputils/io/ColoredOutput.h \
	cpputils/io/ColoredOutput.cc \
	cpputils/io/CpputilsDebug.cc \
	cpputils/io/CpputilsDebug.h \
	cpputils/io/DetectLocale.h \
	cpputils/io/DetectLocale.cc \
	cpputils/io/OutDebug.h \
	cpputils/io/OutDebug.cc \
	cpputils/io/read_file.h \
	cpputils/io/read_file.cc


cpputils_cpputilsshared_libcpputilsshared_a_SOURCES= \
		cpputils/cpputilsshared/arg.h \
		cpputils/cpputilsshared/arg.cc \
		cpputils/cpputilsshared/backtrace.h \
		cpputils/cpputilsshared/backtrace.cc \
		cpputils/cpputilsshared/cppdir.h \
		cpputils/cpputilsshared/cppdir.cc \
		cpputils/cpputilsshared/file_option.h \
		cpputils/cpputilsshared/file_option.cc \
		cpputils/cpputilsshared/iterator.h \
		cpputils/cpputilsshared/leoini.h \
		cpputils/cpputilsshared/leoini.cc \
		cpputils/cpputilsshared/map_utils.h \
		cpputils/cpputilsshared/pairs.h \
		cpputils/cpputilsshared/range.h \
		cpputils/cpputilsshared/ref.h \
		cpputils/cpputilsshared/static_debug_exception.h \
		cpputils/cpputilsshared/static_debug_exception.cc \
		cpputils/cpputilsshared/stderr_exception.h \
		cpputils/cpputilsshared/stderr_exception.cc \
		cpputils/cpputilsshared/string_utils.h \
		cpputils/cpputilsshared/string_utils.cc \
		cpputils/cpputilsshared/utf8.h \
		cpputils/cpputilsshared/utf8/core.h \
		cpputils/cpputilsshared/utf8/checked.h \
		cpputils/cpputilsshared/utf8/unchecked.h \
		cpputils/cpputilsshared/utf8/cpp11.h \
		cpputils/cpputilsshared/utf8/cpp17.h \
		cpputils/cpputilsshared/utf8_util.h \
		cpputils/cpputilsshared/utf8_util.cc \
		cpputils/cpputilsshared/xml.h \
		cpputils/cpputilsshared/xml.cc \
		cpputils/cpputilsshared/CyclicArray.h \
		cpputils/cpputilsshared/static_vector.h \
		cpputils/cpputilsshared/static_list.h \
		cpputils/cpputilsshared/static_string.h \
		cpputils/cpputilsshared/string_adapter.h

cpputils_cpputilsshared_cpputilsformat_libcpputilsformat_a_SOURCES= \
	cpputils/cpputilsshared/cpputilsformat/format.h \
	cpputils/cpputilsshared/cpputilsformat/format1.cc \
	cpputils/cpputilsshared/cpputilsformat/format2.cc \
	cpputils/cpputilsshared/cpputilsformat/format2.h \
	cpputils/cpputilsshared/cpputilsformat/format1.h \
	cpputils/cpputilsshared/cpputilsformat/cformat.h \
	cpputils/cpputilsshared/cpputilsformat/cformat.cc \
	cpputils/cpputilsshared/cpputilsformat/wformat2.cc \
	cpputils/cpputilsshared/cpputilsformat/wformat2.h \
	cpputils/cpputilsshared/cpputilsformat/cwformat.h \
	cpputils/cpputilsshared/cpputilsformat/cwformat.cc \
	cpputils/cpputilsshared/cpputilsformat/static_format.h \
	cpputils/cpputilsshared/cpputilsformat/static_format.cc
	
	
	
	
common_libcommon_a_SOURCES=\
	common/ColBuilder.h \
	common/ColBuilder.cc

broker_LDADD = \
	common/libcommon.a \
	db/libdb.a \
	cpputils/io/libcpputilsio.a \
	cpputils/cpputilsshared/libcpputilsshared.a \
	cpputils/cpputilsshared/cpputilsformat/libcpputilsformat.a
	

LIBS=
    
AM_LDFLAGS= \
	`pkg-config --libs SDL2_mixer` \
	`pkg-config --libs mysqlclient`
    
if MINGW
#AM_LDFLAGS += -mwindows
LIBS += -liconv
# with -std=c++11 WIN32 is disabled (_WIN32) would be the valid one, so we define it in mingw.
AM_CPPFLAGS += -DWIN32
else
#AM_LDFLAGS += -lX11
endif

if CYGWIN
LIBS += -liconv
endif

    

