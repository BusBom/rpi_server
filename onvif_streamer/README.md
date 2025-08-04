# ONVIF Streamer

실시간 RTSP 스트림에서 차량 감지 및 번호판 인식을 수행하는 멀티스레드 애플리케이션입니다.

## 개요

이 프로젝트는 ONVIF 호환 IP 카메라의 RTSP 스트림을 처리하여 다음과 같은 기능을 제공합니다:

- **실시간 비디오 스트리밍**: RTSP 스트림을 SDL2를 사용하여 실시간으로 표시
- **메타데이터 파싱**: ONVIF 메타데이터에서 객체 감지 정보 추출
- **차량 감지**: 메타데이터 기반 차량 객체 바운딩 박스 표시
- **번호판 인식**: TensorFlow Lite를 사용한 OCR로 번호판 텍스트 인식
- **공유 메모리**: 인식된 번호판 정보를 공유 메모리를 통해 다른 프로세스와 공유

## 주요 기능

### 1. 멀티스레드 아키텍처
- **Stream Thread**: RTSP 스트림에서 비디오/메타데이터 패킷 수신
- **Decode Thread**: 비디오 프레임 디코딩 및 메타데이터 파싱  
- **Render Thread**: SDL2를 사용한 실시간 비디오 렌더링
- **OCR Thread**: 차량 영역 크롭 후 번호판 OCR 처리

### 2. 동기화 및 타이밍
- FFmpeg 스타일의 PTS 기반 A/V 동기화
- 프레임 드롭 및 지연 보상 메커니즘
- 메타데이터와 비디오 프레임 타임스탬프 매칭

### 3. 객체 감지 및 추적
- ONVIF 메타데이터에서 차량 객체 정보 추출
- 바운딩 박스 기반 관심 영역(ROI) 크롭
- 거리 기반 우선순위 정렬 (가까운 객체 우선)

### 4. OCR 처리
- TensorFlow Lite 모델을 사용한 번호판 텍스트 인식
- 전처리: 이미지 크기 조정, 노이즈 제거, 영역 추출
- CTC 디코딩으로 문자 시퀀스 추출
- 신뢰도 기반 필터링 (기본 임계값: 35%)

## 의존성

### 시스템 라이브러리
- **FFmpeg**: 비디오 스트리밍 및 코덱 처리
  - libavformat, libavcodec, libavutil, libswscale
- **OpenCV**: 이미지 처리 및 컴퓨터 비전
- **SDL2**: 실시간 비디오 렌더링
- **TinyXML2**: XML 메타데이터 파싱
- **TensorFlow Lite**: 기계학습 추론

### 빌드 도구
- CMake 3.10+
- C++17 호환 컴파일러
- pkg-config

## 빌드 방법

```bash
# 의존성 설치 (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
sudo apt-get install libopencv-dev libsdl2-dev libtinyxml2-dev

# TensorFlow Lite 설치 (사용자 환경에 맞게 경로 수정 필요)
# CMakeLists.txt에서 TFLITE_INCLUDE_DIR과 TFLITE_STATIC_LIB 경로 확인

# 빌드
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 사용법

### 기본 실행
```bash
./onvif_streamer
```

애플리케이션은 하드코딩된 RTSP URL에 연결합니다:
```
rtsp://192.168.0.64/profile2/media.smp
```

### 설정
main.cpp에서 다음 항목들을 수정할 수 있습니다:

- **RTSP URL**: `const char* url` 변수
- **해상도 설정**: `HANWHA_ORIGINAL_WIDTH`, `HANWHA_ORIGINAL_HEIGHT` 상수
- **관심 지점**: `custom_point` 변수 (객체 우선순위 기준점)
- **공유 메모리**: `shm_name`, `shm_size` 변수

### 공유 메모리 출력
인식된 번호판 정보는 JSON 형태로 공유 메모리(`/bus_approach`)에 저장됩니다:

```json
[
  {"busNumber": "123가4567"},
  {"busNumber": "456나8901"}
]
```

## 파일 구조

```
onvif_streamer/
├── main.cpp           # 메인 애플리케이션 및 스레드 관리
├── parser.hpp/cpp     # ONVIF 메타데이터 XML 파싱
├── video.hpp/cpp      # 비디오 디코딩 및 프레임 처리
├── ocr.hpp/cpp        # TensorFlow Lite OCR 처리
├── bus_sequence.hpp   # 버스 시퀀스 데이터 구조
├── json.hpp           # JSON 라이브러리 (nlohmann/json)
├── model.tflite       # OCR 모델 파일
├── labels.names       # OCR 레이블 맵
└── CMakeLists.txt     # 빌드 설정
```

## 성능 특징

- **실시간 처리**: 25fps 비디오 스트림 실시간 처리
- **메모리 효율성**: 프레임 참조 및 큐 기반 메모리 관리
- **오류 복구**: I/P 프레임 참조 오류 처리 및 키프레임 대기
- **동기화 정확도**: ±50ms 이내 A/V 동기화

## 제한사항

- 하드코딩된 RTSP URL (향후 설정 파일로 개선 예정)
- 특정 카메라 해상도에 최적화 (3840x2160)
- 단일 카메라 스트림만 지원
- TensorFlow Lite 모델 경로 하드코딩

## 문제 해결

### 일반적인 문제들

1. **RTSP 연결 실패**
   - 카메라 IP 주소와 스트림 경로 확인
   - 네트워크 연결 및 방화벽 설정 확인

2. **SDL 초기화 실패**
   - X11 또는 Wayland 디스플레이 서버 실행 확인
   - 디스플레이 권한 설정 확인

3. **TensorFlow Lite 모델 로드 실패**
   - model.tflite 파일 존재 확인
   - 모델 파일 권한 확인

4. **공유 메모리 접근 오류**
   - `/dev/shm` 마운트 확인
   - 프로세스 권한 확인

### 디버깅

로그 레벨 조정:
```cpp
av_log_set_level(AV_LOG_DEBUG);  // 더 자세한 FFmpeg 로그
```

OCR 신뢰도 임계값 조정:
```cpp
ocrProcessor.setConfidenceThreshold(0.25f);  // 더 낮은 임계값
```

## 기여

버그 리포트나 기능 제안은 이슈를 통해 알려주세요.

## 라이선스

이 프로젝트는 MIT 라이선스 하에 배포됩니다.
