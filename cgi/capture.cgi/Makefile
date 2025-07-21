CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
FCGI_FLAGS = -lfcgi
RT_FLAGS = -lrt
OPENCV_FLAGS = `pkg-config --cflags --libs opencv4`

# Targets
CGI_TARGET = capture.cgi

# Source files
CGI_SOURCE = main.cpp

all: $(CGI_TARGET)

$(CGI_TARGET): $(CGI_SOURCE)
	$(CXX) $(CXXFLAGS) $(CGI_SOURCE) $(FCGI_FLAGS) $(RT_FLAGS) $(OPENCV_FLAGS) -o $(CGI_TARGET)

clean:
	rm -f $(CGI_TARGET)

.PHONY: all clean
