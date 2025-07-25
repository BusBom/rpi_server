

TARGET = stop-status
SRCS = $(TARGET).cgi/main.cpp
BIN = $(TARGET).cgi

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
FCGI_FLAGS = -lfcgi
RT_FLAGS = -lrt
OPENCV_FLAGS = `pkg-config --cflags --libs opencv4`

CGI_DIR = /usr/lib/cgi-bin

CONF_FILE = my-cgi.conf
SITES_AVAILABLE = /etc/nginx/sites-available
SITES_ENABLED = /etc/nginx/sites-enabled

.PHONY: build install-cgi install-conf clean

build: $(SRC)
	$(CXX) $(CXXFLAGS) -o build/$(BIN) $(SRCS) $(FCGI_FLAGS) $(RT_FLAGS) $(OPENCV_FLAGS)

install-cgi: build
	@echo "Installing CGI binary to $(CGI_DIR)..."
	sudo cp build/$(BIN) $(CGI_DIR)/$(BIN)
	sudo chmod +x $(CGI_DIR)/$(BIN)

install-conf: $(CONF_FILE)
	@echo "Copying $(CONF_FILE) to $(SITES_AVAILABLE)..."
	sudo cp $(CONF_FILE) $(SITES_AVAILABLE)/
	@echo "Linking to $(SITES_ENABLED)..."
	sudo ln -sf $(SITES_AVAILABLE)/$(CONF_FILE) $(SITES_ENABLED)/$(CONF_FILE)
	@echo "Reloading nginx..."
	sudo nginx -s reload
	@echo "Done!"


reload-nginx:
	@echo "Reloading nginx..."
	sudo nginx -s reload

# 전체 배포 한 번에
deploy: install-cgi install-conf reload-nginx
	@echo "======= ALL DONE! ======="

clean:
	sudo rm -f build/*
