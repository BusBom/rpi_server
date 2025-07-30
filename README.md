## VEDA-Busbom Server-Side (on Raspberry Pi 4)

### Structure
#### `/cgi` : 
Nginx - FastCGI Applications
#### `/camera_stream` : 
use RPi camera, write frames to shared memory
#### `/yolo_lp_detector` :
use RPi camera with yolo, detect license plate and run OCR
#### `/onvif_streamer` :
use Hanwha IP camera, receive RTSP stream & onvif metadata, this also includes license plate detection and OCR
#### `/rtsp_simple_stream` :
receive generic RTSP stream, write frames to shared memory 
#### `/rtsp_server` :
read frame from shared memory, make RTSP stream forward to other client

### Requirements
- Raspberry Pi OS (64bit, Legacy)
- ncnn
- tensorflow-lite
- opencv4
- mediamtx (a.k.a rtsp-simple-server)
- ffmpeg (libav*)
- tinyxml2
- libsdl (SDL)
- libfcgi-dev (FastCGI)
- nginx
- cmake



