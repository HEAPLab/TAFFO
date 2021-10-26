set(SELF taffo-costmodel)

add_executable(${SELF}
  taffo-costmodel.c
  timing.c
  timing.h
  collector.h
  xutils.h
  )

target_compile_definitions(${SELF} PRIVATE
  -DNOFLT80 -DNOFLT128 -DNBRUN=128 -DMEMSIZE=10000000
  )

install(TARGETS ${SELF})

