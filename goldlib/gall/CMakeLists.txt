set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(gall_SOURCES
  gbmh.cpp
  gcharset.cpp
  gcrc16tb.cpp
  gcrc32tb.cpp
  gcrchash.cpp
  gcrckeyv.cpp
  gcrcm16.cpp
  gcrcm32.cpp
  gcrcs16.cpp
  gcrcs32.cpp
  gdbgerr.cpp
  gdbgexit.cpp
  gdbgtrk.cpp
  gdirposx.cpp
  gespell.cpp
  geval.cpp
  gevalhum.cpp
  gevalrpn.cpp
  gfile.cpp
  gfilport.cpp
  gfilutl1.cpp
  gfilutl2.cpp
  gftnaddr.cpp
  gftnnl.cpp
  gftnnlfd.cpp
  gftnnlfu.cpp
  gftnnlge.cpp
  gftnnlv7.cpp
  gfuzzy.cpp
  ghdrmime.cpp
  glog.cpp
  gmemdbg.cpp
  gmemutil.cpp
  gmsgattr.cpp
  gprnutil.cpp
  gregex.cpp
  gsearch.cpp
  gsnd.cpp
  gsndwrap.cpp
  gstrctyp.cpp
  gstrmail.cpp
  gstrname.cpp
  gstrutil.cpp
  gtimjuld.cpp
  gtimutil.cpp
  gtxtpara.cpp
  gusrbase.cpp
  gusrezyc.cpp
  gusrgold.cpp
  gusrhuds.cpp
  gusrmax.cpp
  gusrpcb.cpp
  gusrra2.cpp
  gusrxbbs.cpp
  gutlclip.cpp
  gutlcode.cpp
  gutlgrp.cpp
  gutlmisc.cpp
  gutlmtsk.cpp
  gutltag.cpp
  gutlvers.cpp
  gwildmat.cpp
)
if (UNIX)
  list(APPEND gall_SOURCES
    gkbdunix.cpp
    gsigunix.cpp
    gutlunix.cpp
  )
elseif (MSVC)
  list(APPEND gall_SOURCES
    gutlwin.cpp
    gutlwinm.cpp
  )
endif()
ADD_LIBRARY(gall STATIC ${gall_SOURCES})
include_directories(.. ../glibc)
target_link_libraries(gall hunspell)