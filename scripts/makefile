CXX = g++
CXXFLAGS = -Wall -O2 -pthread -I. -I../protocol -I../attack_detection -I../core
LDFLAGS = -L/usr/lib/python3.9/config-3.9-arm-linux-gnueabihf -lpython3.9
TARGET_DIR = ../scripts

# 기본 소스 파일과 오브젝트 파일
SRCS = ../core/main.cpp ../protocol/cQueue.cpp ../protocol/periodic.cpp ../core/CANStats.cpp

# DBC 체크가 필요한 경우 컴파일 플래그와 소스 파일 추가
ifeq ($(wildcard ../protocol/dbcparsed_dbc.cpp)$(wildcard ../attack_detection/Fuzzing_based_DBC_detection.cpp),../protocol/dbcparsed_dbc.cpp../attack_detection/Fuzzing_based_DBC_detection.cpp)
    CXXFLAGS += -DSET_DBC_CHECK
    SRCS += ../protocol/dbcparsed_dbc.cpp ../attack_detection/Fuzzing_based_DBC_detection.cpp
endif

# 공격 탐지 관련 파일들
SRCS += ../attack_detection/ClockSkew_detection.cpp \
        ../attack_detection/DoS_detection.cpp \
        ../attack_detection/Event_detection.cpp \
        ../attack_detection/Normal_detection.cpp \
        ../attack_detection/Replay_Suspension_detection.cpp \
        ../attack_detection/AttackFilter.cpp

OBJS = $(SRCS:.cpp=.o)

# 실행 파일 이름
TARGET = $(TARGET_DIR)/ids

# 주 규칙
all: $(TARGET) clean_objects

# 메인 타겟 빌드 규칙
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# 성공적인 빌드 후 오브젝트 파일 정리
clean_objects:
	rm -f $(OBJS)

# .cpp 파일에서 개별 오브젝트 파일을 컴파일하는 규칙
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 전체 빌드 정리 (실행 파일 포함)
clean:
	rm -f $(OBJS) $(TARGET)

