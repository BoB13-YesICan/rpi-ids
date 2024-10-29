# 컴파일러 설정
CXX = g++

# 컴파일러 옵션
CXXFLAGS = -Wall -g $(shell pkg-config --cflags jsoncpp) -pthread

# 소스 파일 및 헤더 파일

SRCS = main.cpp periodic.cpp CANStats.cpp cQueue.cpp all_attack_detection.cpp check_clock_error.cpp dbc.cpp

OBJS = $(SRCS:.cpp=.o)

# 실행 파일 이름
TARGET = ids

# 기본 빌드 명령
all: $(TARGET) clean_objs

# 실행 파일 생성
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(shell pkg-config --libs jsoncpp)

# 개별 소스 파일 컴파일
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 빌드 후 오브젝트 파일 삭제
clean_objs:
	rm -f $(OBJS)

# 전체 파일 삭제
clean:
	rm -f $(OBJS) $(TARGET)

