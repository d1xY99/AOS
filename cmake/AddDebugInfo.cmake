# This macro adds a post-build hook to the specified target in order to attach
# AOS-readable debug info.

function(ADD_DEBUG_INFO TARGET)
  set(dbg_binary $<TARGET_FILE:${TARGET}>)

  add_custom_command(TARGET ${TARGET}
      POST_BUILD
      COMMAND "${CMAKE_BINARY_DIR}/aos-dwarf" "${dbg_binary}" "${dbg_binary}.dbg"
      COMMAND "${OBJCOPY_EXECUTABLE}" --remove-section .aosdbg "${dbg_binary}"
      COMMAND "${OBJCOPY_EXECUTABLE}" --add-section .aosdbg="${dbg_binary}.dbg"
                                      --set-section-flags .aosdbg=noload,readonly "${dbg_binary}"
      COMMAND ${CMAKE_COMMAND} -E remove -f "${dbg_binary}.dbg"
  )

  # Requires aos-dwarf to be built first.
  add_dependencies(${TARGET} aos-dwarf)
endfunction(ADD_DEBUG_INFO)
