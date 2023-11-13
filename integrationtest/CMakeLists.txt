add_custom_target(Integrationtests)
set_target_properties(Integrationtests PROPERTIES FOLDER "integrationtest")

set(FINNC_INTEGRATIONTEST_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_integrationtest(SyncInference.cpp)