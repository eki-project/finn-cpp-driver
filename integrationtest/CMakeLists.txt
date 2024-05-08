add_custom_target(Integrationtests)
set_target_properties(Integrationtests PROPERTIES FOLDER "integrationtest")

set(FINN_INTEGRATIONTEST_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_integrationtest(SyncInference.cpp)
add_integrationtest(AsyncInference.cpp)