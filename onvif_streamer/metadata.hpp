#ifndef METADATA_HPP
#define METADATA_HPP

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
}

#include <tinyxml2.h>
#include <iostream>
#include <string>
#include <mutex>
#include <unordered_set>
#include <algorithm>

struct BoundingBox {
    double left, top, right, bottom;
};

struct Point {
    double x, y;
};

struct Object {
    std::string typeName;
    std::string ocrText;
    std::string lineNumber;
    int objectId;
    double confidence;
    BoundingBox boundingBox;
    Point centerOfGravity;
    int distance;
};

struct OnvifMeta {
    int64_t pts;
    int64_t dts;
    std::vector<Object> objects;
};

class MetadataParser {
    private:
        std::string xml_buffer_;
        int64_t pts_buffer_;
        std::vector<std::string> fetched_buffer_;
        std::vector<Object> result_;

        std::vector<Object> extractObj(tinyxml2::XMLElement* element, const std::string& typeName = "LicensePlate");

        void cleanup();
        void process_packet(const uint8_t* data, int size);
        bool process_buffer();
        
    public:
        int64_t fetch_pts() const { return pts_buffer_; }
        OnvifMeta fetchMetadata(AVPacket* pkt);

};

#endif // METADATA_HPP