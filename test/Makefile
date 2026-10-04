# Runs the sketch's logic on your computer (no Arduino needed).
#   make        build and run the tests
CXX ?= g++

test: test_dial
	./test_dial

test_dial: test_dial.cpp arduino_stub.h mock/RTClib.h mock/Wire.h ../firmware/desk_clock_dial/desk_clock_dial.ino
	$(CXX) -std=gnu++17 -Imock -w test_dial.cpp -o test_dial

clean:
	rm -f test_dial

.PHONY: test clean
