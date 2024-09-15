#creating test file
rm -rf $(pwd)/test-dir
mkdir -p $(pwd)/test-dir/test-dir-child
echo "This file must not be modified when ref-state=(ON|REC_ON)" > $(pwd)/test-dir/test_file
echo "This child dir 1 file must not be modified when ref-state=(ON|REC_ON)" > $(pwd)/test-dir/test-dir-child/test_file_child1
echo "This child dir 2 file must not be modified when ref-state=(ON|REC_ON)" > $(pwd)/test-dir/test-dir-child/test_file_child2
ln $(pwd)/test-dir/test_file $(pwd)/test-dir/test_file_hl
#build admin.c
make
./admin.out RefMonitorPwd -cs rec_on -cp add $(pwd)/test-dir/test_file
./admin.out RefMonitorPwd -cp add $(pwd)/test-dir/test-dir-child
./admin.out RefMonitorPwd -cp add $(pwd)/test-dir/test-dir-child/test_file_child1
./admin.out RefMonitorPwd -cp add $(pwd)/test-dirs/test-dir-child
