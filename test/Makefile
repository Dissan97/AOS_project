stress:
	gcc stress_test.c lib/*.c -o stress_test.out -lpthread

launc_stress_test:
	sudo ./stress_test.out -pwd RefMonitorPwd -conf resources/test_add_remove.csv

functionality:
	gcc functionality_test.c lib/*.c -o func_test.out

launch_functionality_test:
	sudo ./func_test.out -pwd RefMonitorPwd -conf resources/test_add_remove.csv