#ifndef PARSER_HPP
#define PARSER_HPP

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
    int objectId;
    double confidence;
    BoundingBox boundingBox;
    Point centerOfGravity;
    int distance; // Distance from the camera
};

class MetadataParser {
    private: 
        std::string xml_buffer;
        std::vector<std::string> completed_streams;
        std::vector<Object> result;

        std::string extractTime(tinyxml2::XMLElement* element);
        std::vector<Object> extractObj(tinyxml2::XMLElement* element, const std::string& typeName = "LicensePlate");
        
        void processXmlDoc(tinyxml2::XMLDocument& doc, std::vector<Object>& result);
        void cleanupBuffer();
        void processCompletedStreams();

    public:
        void processPacket(const uint8_t* data, int size);
        void processBuffer();
        size_t getBufferSize() const;
        void debugBuffer();
        const std::vector<Object>& getResults() const;
};

#endif // PARSER_HPP