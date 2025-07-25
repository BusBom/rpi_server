# ...existing code...

test_shm_reader: test_shm_reader.cpp
	g++ -o test_shm_reader test_shm_reader.cpp -lrt

clean:
	rm -f test_shm_reader
	# ...existing clean rules...

.PHONY: clean test_shm_reader