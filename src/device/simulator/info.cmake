CHECK_SUB_ENABLE(MODULE_ENABLE device)

# message(FATAL_ERROR ${MODULE_ENABLE})

if(${MODULE_ENABLE})
    file(GLOB CUR_SOURCES "${SUB_DIR}/*.cpp")

    SUB_ADD_SRC(CUR_SOURCES)
    SUB_ADD_INC(SUB_DIR)
endif()
