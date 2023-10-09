# Running integration tests
add_custom_target(integration_test_slurm ALL
    COMMAND sbatch run_test.sh ../example_networks/identitiy_net ../src/config/FinnDriverUsedDatatypes.h
    WORKING_DIRECTORY .
    COMMENT "Running integraton tests using slurm as job scheduler"
    VERBATIM
)