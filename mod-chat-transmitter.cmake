target_include_directories(modules PUBLIC "${MYSQL_INCLUDE_DIR}")
target_include_directories(modules PUBLIC "${CMAKE_CURRENT_LIST_DIR}/libs")

# Eluna is optional. The Eluna parts of this module (the "eluna" query database and
# forwarding Lua errors to Discord) are only built when an Eluna module is part of the build:
# mod-ale (the new name) sets MOD_ALE_FOUND, the older mod-eluna is found in the module list.
set(CHAT_TRANSMITTER_ELUNA 0)
if (MOD_ALE_FOUND)
  set(CHAT_TRANSMITTER_ELUNA 1)
elseif ("mod-eluna" IN_LIST MODULES_MODULE_LIST)
  ModuleNameToVariable("mod-eluna" CHAT_TRANSMITTER_ELUNA_VARIABLE)
  if (NOT ("${${CHAT_TRANSMITTER_ELUNA_VARIABLE}}" STREQUAL "disabled"))
    set(CHAT_TRANSMITTER_ELUNA 1)
  endif()
endif()

target_compile_definitions(modules PRIVATE CHAT_TRANSMITTER_ELUNA=${CHAT_TRANSMITTER_ELUNA})
if (CHAT_TRANSMITTER_ELUNA)
  message(STATUS "mod-chat-transmitter: Eluna found, the Eluna features are on")
else()
  message(STATUS "mod-chat-transmitter: no Eluna, building without the Eluna features")
endif()
