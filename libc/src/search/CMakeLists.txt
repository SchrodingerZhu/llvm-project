add_subdirectory(hashtable)

add_entrypoint_object(
  hsearch
  SRCS
    hsearch.cpp
  HDRS 
    hsearch.h
  DEPENDS
    .hashtable.global
    .hashtable.search_impl
)

add_entrypoint_object(
  hsearch_r
  SRCS
    hsearch_r.cpp
  HDRS 
    hsearch_r.h
  DEPENDS
    .hashtable.search_impl
)

add_entrypoint_object(
  hcreate
  SRCS
    hcreate.cpp
  HDRS 
    hcreate.h
  DEPENDS
    .hashtable.global
    .hashtable.utils
    libc.include.errno
)

add_entrypoint_object(
  hcreate_r
  SRCS
    hcreate_r.cpp
  HDRS 
    hcreate_r.h
  DEPENDS
    .hashtable.utils
    libc.include.stdlib
    libc.include.errno
)

add_entrypoint_object(
  hdestroy
  SRCS
  hdestroy.cpp
  HDRS 
  hdestroy.h
  DEPENDS
    .hashtable.global
    .hashtable.utils
)

add_entrypoint_object(
  hdestroy_r
  SRCS
    hdestroy_r.cpp
  HDRS 
    hdestroy_r.h
  DEPENDS
    .hashtable.utils
    libc.include.stdlib
)
