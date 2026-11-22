if(_CAN_LIBRARY_INCLUDED)
    return()
endif()
set(_CAN_LIBRARY_INCLUDED TRUE)

# Define the path to the generated directory
set(CAN_LIB_DIR ${CMAKE_SOURCE_DIR}/can_library)
set(CAN_GEN_DIR ${CAN_LIB_DIR}/generated)
get_filename_component(REPOSITORY_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(GENERATOR_SCRIPT ${REPOSITORY_DIR}/generators/generate.py)

# Run the generator during configuration to ensure files exist for compile checks
# This fixes the cases where CMake fails because generated source files are missing
message(STATUS "Running firmware generators (configuration phase)...")
execute_process(
    COMMAND python3 ${GENERATOR_SCRIPT}
    WORKING_DIRECTORY ${REPOSITORY_DIR}
    RESULT_VARIABLE CAN_GEN_RESULT
)

if (NOT CAN_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "Firmware generation failed during configuration! Return code: ${CAN_GEN_RESULT}")
endif()

# Create node-specific libraries
# Compile the CAN library sources with a node-specific define.
macro(create_can_node_lib NODE_NAME)
	set(LIB_NAME "can_node_${NODE_NAME}")
	
	# Give each node its own CAN and fault state.
	add_library(${LIB_NAME} STATIC 
        ${CAN_LIB_DIR}/source/can_init.c
        ${CAN_LIB_DIR}/source/can_rx.c
        ${CAN_LIB_DIR}/source/can_tx.c
        ${CAN_LIB_DIR}/source/faults_common.c
        ${CAN_GEN_DIR}/fault_data.c
    )

	# Force use of full include paths relative to project root
	target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
	
	# Set the node-specific define so can_router.h knows which header to include
	target_compile_definitions(${LIB_NAME} PUBLIC "CAN_NODE_${NODE_NAME}")
	
	# Link the STM32G474 and RTOS dependencies
	target_link_libraries(${LIB_NAME} PUBLIC CMSIS_G474 RTOS_G474)
	
	# Include the current directory so FreeRTOSConfig.h can be found if needed
	target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endmacro()
