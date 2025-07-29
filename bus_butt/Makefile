# 컴파일러 및 옵션
CXX = g++
CXXFLAGS = -std=c++17 -Wall -g -Ishm_reader -Ibutt_matcher -Idisplay_writer -I../../common -I/home/cty0613/rpi_server/common

LDFLAGS = -lrt -lcurl

# 소스 파일 목록
SRCS = \
    main.cpp \
    butt_matcher/butt_matcher.cpp \
    display_writer/display_writer.cpp \
    shm_reader/shm_reader.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = bus_butt

# 기본 빌드 규칙
all: $(TARGET)

# 링크 규칙
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# 개별 오브젝트 파일 빌드 규칙
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 정리
clean:
	rm -f $(OBJS) $(TARGET)
