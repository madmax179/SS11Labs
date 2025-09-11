cmake_minimum_required (VERSION 3.10)
project (elevenlabs-synth C)

# Sources
set (ELEVENLABS_SYNTH_SOURCES
	src/elevenlabs_synth_engine.c
	src/elevenlabs_synth_channel.c
	src/elevenlabs_http.c
	src/ulaw_decode.c
	# src/elevenlabs_utils.c
)
source_group ("src" FILES ${ELEVENLABS_SYNTH_SOURCES})

# Option: standalone build vs. in-tree UniMRCP build
option(ELEVENLABS_STANDALONE "Build outside UniMRCP source tree" ON)

# Detect in-tree build by checking for known targets/variables
if (TARGET mrcpengine OR DEFINED MRCP_ENGINE_INCLUDE_DIRS)
	set(ELEVENLABS_STANDALONE OFF)
endif()

if (NOT ELEVENLABS_STANDALONE)
	# In-tree UniMRCP build: use existing targets and variables
	find_package(CURL REQUIRED)

	add_library (${PROJECT_NAME} MODULE ${ELEVENLABS_SYNTH_SOURCES}
		$<TARGET_OBJECTS:mrcpengine>
		$<TARGET_OBJECTS:mrcp>
		$<TARGET_OBJECTS:mpf>
		$<TARGET_OBJECTS:aprtoolkit>
	)
	set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "plugins")

	target_link_libraries(${PROJECT_NAME}
		${APU_LIBRARIES}
		${APR_LIBRARIES}
		CURL::libcurl
	)

	# Preprocessor definitions
	add_definitions (
		${MRCP_DEFINES}
		${MPF_DEFINES}
		${APR_TOOLKIT_DEFINES}
		${APR_DEFINES}
		${APU_DEFINES}
	)

	# Include directories
	include_directories (
		${PROJECT_SOURCE_DIR}/include
		${MRCP_ENGINE_INCLUDE_DIRS}
		${MRCP_INCLUDE_DIRS}
		${MPF_INCLUDE_DIRS}
		${APR_TOOLKIT_INCLUDE_DIRS}
		${APR_INCLUDE_DIRS}
		${APU_INCLUDE_DIRS}
	)
else()
	# Standalone build: locate system deps and UniMRCP installation prefix
	find_package(PkgConfig REQUIRED)
	find_package(CURL REQUIRED)
	pkg_check_modules(APR REQUIRED apr-1)
	pkg_check_modules(APU REQUIRED apr-util-1)

	# UniMRCP prefix (headers + optional libs). Default to /opt/unimrcp
	set(UNIMRCP_DIR "/opt/unimrcp" CACHE PATH "Path to UniMRCP install prefix")

	# Try to find key headers to validate prefix
	find_path(UNIMRCP_INCLUDE_DIR mrcp_synth_engine.h
		HINTS
			${UNIMRCP_DIR}/include
			${UNIMRCP_DIR}/include/unimrcp
		PATH_SUFFIXES mrcp-engine mrcp)

	if (NOT UNIMRCP_INCLUDE_DIR)
		message(FATAL_ERROR "Cannot find UniMRCP headers (mrcp_synth_engine.h). Set UNIMRCP_DIR to your UniMRCP install prefix (e.g., /opt/unimrcp)")
	endif()

	# Includes: plugin + UniMRCP public headers (APT/MPF/MRCP)
	include_directories(
		${PROJECT_SOURCE_DIR}/include
		${UNIMRCP_DIR}/include
		${UNIMRCP_DIR}/include/apt
		${UNIMRCP_DIR}/include/mpf
		${UNIMRCP_DIR}/include/mrcp
		${UNIMRCP_DIR}/include/mrcp-engine
		${UNIMRCP_DIR}/include/apr-toolkit
		${APR_INCLUDE_DIRS}
		${APU_INCLUDE_DIRS}
	)

	add_library(${PROJECT_NAME} MODULE ${ELEVENLABS_SYNTH_SOURCES})

	# Link against system libs; UniMRCP symbols are resolved at runtime by the server
	target_link_libraries(${PROJECT_NAME}
		CURL::libcurl
		${APR_LIBRARIES}
		${APU_LIBRARIES}
	)

	if (UNIX)
		target_link_libraries(${PROJECT_NAME} m)
		# Ensure we don't accidentally add -Wl,-z,defs which would forbid unresolved symbols
	endif()

	# Optional: if UniMRCP libs are available, link them to catch missing symbols at link-time
	find_library(MPF_LIB mpf HINTS ${UNIMRCP_DIR}/lib ${UNIMRCP_DIR}/lib64)
	find_library(MRCP_LIB mrcp HINTS ${UNIMRCP_DIR}/lib ${UNIMRCP_DIR}/lib64)
	find_library(ENGINE_LIB mrcpengine HINTS ${UNIMRCP_DIR}/lib ${UNIMRCP_DIR}/lib64)
	find_library(APRTOOLKIT_LIB aprtoolkit HINTS ${UNIMRCP_DIR}/lib ${UNIMRCP_DIR}/lib64)
	if (MPF_LIB AND MRCP_LIB AND ENGINE_LIB AND APRTOOLKIT_LIB)
		target_link_libraries(${PROJECT_NAME} ${MPF_LIB} ${MRCP_LIB} ${ENGINE_LIB} ${APRTOOLKIT_LIB})
	endif()
endif()

# Installation directives
install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION plugin)
if (MSVC)
	install (FILES ${PROJECT_BINARY_DIR}/Debug/${PROJECT_NAME}.pdb DESTINATION plugin CONFIGURATIONS Debug)
	install (FILES ${PROJECT_BINARY_DIR}/RelWithDebInfo/${PROJECT_NAME}.pdb DESTINATION plugin CONFIGURATIONS RelWithDebInfo)
endif ()
