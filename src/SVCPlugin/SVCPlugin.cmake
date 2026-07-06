set(SVCPLUGIN_NAME svcConv)				#dTwin plugin dll target name

file(GLOB SVCPLUGIN_CPP_COMMON_SOURCES  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB SVCPLUGIN_CPP_COMMON_INCS  ${CMAKE_CURRENT_LIST_DIR}/src/*.h)
file(GLOB SVCPLUGIN_INC_GUI  ${NATID_SDK_INC}/gui/*.h)
file(GLOB SVCPLUGIN_INC_TD  ${NATID_SDK_INC}/td/*.h)
file(GLOB SVCPLUGIN_INC_CNT  ${NATID_SDK_INC}/cnt/*.h)
file(GLOB SVCPLUGIN_INC_MU  ${NATID_SDK_INC}/mu/*.h)
file(GLOB SVCPLUGIN_INC_MEM  ${NATID_SDK_INC}/mem/*.h)
file(GLOB SVCPLUGIN_INC_FO ${NATID_SDK_INC}/fo/*.h)
file(GLOB SVCPLUGIN_INC_SC ${NATID_SDK_INC}/sc/*.h)
file(GLOB SVCPLUGIN_INC_SYST ${NATID_SDK_INC}/syst/*.h)
file(GLOB SVCPLUGIN_INC_DENSE ${NATID_SDK_INC}/dense/*.h)
file(GLOB SVCPLUGIN_INC_SPARSE ${NATID_SDK_INC}/sparse/*.h)
file(GLOB SVCPLUGIN_INC_XML ${NATID_SDK_INC}/xml/*.h)
file(GLOB SVCPLUGIN_INC_ARCH ${NATID_SDK_INC}/arch/*.h)

# add shared library (plugin is a shared executatable binary file)
add_library(${SVCPLUGIN_NAME} SHARED ${SVCPLUGIN_CPP_COMMON_SOURCES} ${SVCPLUGIN_INC_GUI} ${SVCPLUGIN_CPP_COMMON_INCS}
							${SVCPLUGIN_INC_TD} ${SVCPLUGIN_INC_SYST}
							${SVCPLUGIN_INC_CNT} ${SVCPLUGIN_INC_MU} ${SVCPLUGIN_INC_MEM} ${SVCPLUGIN_INC_FO}
							${SVCPLUGIN_INC_SC} ${SVCPLUGIN_INC_DENSE} ${SVCPLUGIN_INC_SPARSE}
							${SVCPLUGIN_INC_XML} ${SVCPLUGIN_INC_ARCH})

source_group("inc\\inc"        FILES ${SVCPLUGIN_CPP_COMMON_INCS})
source_group("inc\\gui"        FILES ${SVCPLUGIN_INC_GUI})
source_group("inc\\td"        FILES ${SVCPLUGIN_INC_TD})
source_group("inc\\cnt"        FILES ${SVCPLUGIN_INC_CNT})
source_group("inc\\dense"        FILES ${SVCPLUGIN_INC_DENSE})
source_group("inc\\mu"        FILES ${SVCPLUGIN_INC_MU})
source_group("inc\\mem"        FILES ${SVCPLUGIN_INC_MEM})
source_group("inc\\fo"        FILES ${SVCPLUGIN_INC_FO})
source_group("inc\\sc"        FILES ${SVCPLUGIN_INC_SC})
source_group("inc\\sparse"        FILES ${SVCPLUGIN_INC_SPARSE})
source_group("inc\\syst"        FILES ${SVCPLUGIN_INC_SYST})
source_group("inc\\xml"        FILES ${SVCPLUGIN_INC_XML})
source_group("inc\\arch"        FILES ${SVCPLUGIN_INC_ARCH})

source_group("src\\cpp"			FILES ${SVCPLUGIN_CPP_COMMON_SOURCES})

target_link_libraries(${SVCPLUGIN_NAME} debug ${MU_LIB_DEBUG} optimized ${MU_LIB_RELEASE}
										debug ${MATRIX_LIB_DEBUG} optimized ${MATRIX_LIB_RELEASE}
									  debug ${NATGUI_LIB_DEBUG} optimized ${NATGUI_LIB_RELEASE})

target_compile_definitions(${SVCPLUGIN_NAME} PUBLIC PLUGIN_EXPORTS)

# NOTE: EQPlugin's own .cmake template calls setIDEPropertiesForLib(${...}) here, but that
# function is NOT defined anywhere in this natID.SDK's DevEnv/*.cmake (only
# setIDEPropertiesForExecutable and setIDEPropertiesForGUIExecutable exist - see
# PROGRESS_NOTES.txt). Calling it fails CMake configuration with "Unknown CMake command".
# If your installed SDK version does define it, uncomment the next line.
# setIDEPropertiesForLib(${SVCPLUGIN_NAME})
