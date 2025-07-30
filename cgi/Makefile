# Default target if not specified
TARGET ?= stop-status
SRCS = $(TARGET).cgi/main.cpp
BIN = $(TARGET).cgi

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
FCGI_FLAGS = -lfcgi
RT_FLAGS = -lrt
# OPENCV_FLAGS = `pkg-config --cflags --libs opencv4`  # 필요 없다면 주석 처리

CGI_DIR = /usr/lib/cgi-bin

CONF_FILE = my-cgi.conf
SITES_AVAILABLE = /etc/nginx/sites-available
SITES_ENABLED = /etc/nginx/sites-enabled

.PHONY: build install-cgi install-conf reload-nginx deploy clean

# 빌드 대상 생성
build: $(SRCS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o build/$(BIN) $(SRCS) $(FCGI_FLAGS) $(RT_FLAGS)

# CGI 바이너리 설치
install-cgi: build
	@echo "Installing CGI binary to $(CGI_DIR)..."
	sudo cp build/$(BIN) $(CGI_DIR)/
	sudo chmod +x $(CGI_DIR)/$(BIN)

<<<<<<< HEAD
# Nginx 설정 복사 및 링크
install-conf: $(CONF_FILE)
	@echo "Copying $(CONF_FILE) to $(SITES_AVAILABLE)..."
	sudo cp $(CONF_FILE) $(SITES_AVAILABLE)/
	@echo "Linking to $(SITES_ENABLED)..."
	sudo ln -sf $(SITES_AVAILABLE)/$(CONF_FILE) $(SITES_ENABLED)/$(CONF_FILE)

# Nginx 리로드
=======
>>>>>>> origin/main
reload-nginx:
	@echo "Reloading nginx..."
	sudo nginx -s reload

<<<<<<< HEAD
# 전체 배포
deploy: install-cgi install-conf reload-nginx
=======
# 전체 배포 한 번에
deploy: install-cgi reload-nginx
>>>>>>> origin/main
	@echo "======= ALL DONE! ======="

# 빌드 클린
clean:
	sudo rm -f build/*
