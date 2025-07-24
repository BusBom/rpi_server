#include "metadata.hpp"

using namespace tinyxml2;

OnvifMeta MetadataParser::fetchMetadata(AVPacket* pkt) {
    OnvifMeta meta = {0, 0, {}};  // 기본값으로 초기화
    
    if (!pkt || !pkt->data || pkt->size <= 0) {
        std::cerr << "[HANWHA-ONVIF META] Invalid packet data" << std::endl;
        return meta;
    }

    // Process the packet data
    process_packet(pkt->data, pkt->size, pkt->pts, pkt->dts);

    // OnvifMeta 구조체에 데이터 설정
    meta.pts = pkt->pts;
    meta.dts = pkt->dts;
    meta.objects = result_;

    // Return the results
    return meta;
    
}

std::vector<Object> MetadataParser::extractObj(XMLElement* root, const std::string& typeName) {
    std::vector<Object> objItems;
        
    if (!root) return objItems;
    
    // find tt:VideoAnalytics from Root Element
    XMLElement* analytics = root->FirstChildElement("tt:VideoAnalytics");
    if (!analytics) return objItems;
    
    // find tt:Frame from tt:VideoAnalytics
    XMLElement* frame = analytics->FirstChildElement("tt:Frame");
    if (!frame) return objItems;
    
    // 각 객체를 순회
    for (XMLElement* obj = frame->FirstChildElement("tt:Object"); obj; 
            obj = obj->NextSiblingElement("tt:Object")) {
            
        const char* objectId = obj->Attribute("ObjectId");
        if (!objectId) continue;
        
        Object objItem;
        objItem.typeName = "None";
        objItem.objectId = std::stoi(objectId);
        objItem.confidence = 0.0;
        objItem.boundingBox = {0.0, 0.0, 0.0, 0.0};
        objItem.centerOfGravity = {0.0, 0.0};
        
        XMLElement* appearance = obj->FirstChildElement("tt:Appearance");
        if (appearance) {
            // Extract Shape information
            XMLElement* shape = appearance->FirstChildElement("tt:Shape");
            if (shape) {
                // Extract BoundingBox
                XMLElement* bbox = shape->FirstChildElement("tt:BoundingBox");
                if (bbox) {
                    const char* left = bbox->Attribute("left");
                    const char* top = bbox->Attribute("top");
                    const char* right = bbox->Attribute("right");
                    const char* bottom = bbox->Attribute("bottom");
                    
                    if (left && top && right && bottom) {
                        objItem.boundingBox.left = std::stod(left);
                        objItem.boundingBox.top = std::stod(top);
                        objItem.boundingBox.right = std::stod(right);
                        objItem.boundingBox.bottom = std::stod(bottom);
                    }
                }
                
                // Extract CenterOfGravity
                XMLElement* cog = shape->FirstChildElement("tt:CenterOfGravity");
                if (cog) {
                    const char* x = cog->Attribute("x");
                    const char* y = cog->Attribute("y");
                    
                    if (x && y) {
                        objItem.centerOfGravity.x = std::stod(x);
                        objItem.centerOfGravity.y = std::stod(y);
                    }
                }
            }
            
            // Extract Class information
            XMLElement* classElem = appearance->FirstChildElement("tt:Class");
            if (classElem) {
                XMLElement* type = classElem->FirstChildElement("tt:Type");
                if (type) {
                    const char* typeText = type->GetText();
                    const char* likelihood = type->Attribute("Likelihood");
                    
                    if (typeText && likelihood && (objItem.typeName = typeText, objItem.typeName == typeName)) {
                        objItem.confidence = std::stod(likelihood);
                        objItems.push_back(objItem);
                    }
                }
            }
        }
    }

    if (objItems.empty()) {
        return std::vector<Object>(); // 빈 vector 반환
    }
    return objItems;
}

// Process the packet data and extract raw metadata (raw xml)
void MetadataParser::process_packet(const uint8_t* data, int size, int64_t pts, int64_t dts) {
    if (!data || size <= 0) return;
    // Add data to the buffer
    xml_buffer_.append(reinterpret_cast<const char*>(data), size);
    pts_buffer_= pts;

    if (!process_buffer()) {
        std::cerr << "[HANWHA-ONVIF META] Failed to process metadata buffer" << std::endl;

    } 
    
}

// Process the buffer to extract completed metadata
bool MetadataParser::process_buffer() {
    size_t pos = 0;
    const std::string xml_declaration = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

    while (pos < xml_buffer_.size()) {

        // find <tt:MetadataStream start tag
        size_t start_pos = xml_buffer_.find("<tt:MetadataStream", pos);
        if (start_pos == std::string::npos) {
            std::cerr << "[HANWHA-ONVIF META] No MetadataStream start tag found" << std::endl;
            break;
        }
        
        size_t tag_end = xml_buffer_.find(">", start_pos); // find '>'
        if (tag_end == std::string::npos) {
            std::cerr << "[HANWHA-ONVIF META] No closing tag found for MetadataStream" << std::endl;
            break;
        }
        
        std::string start_tag = xml_buffer_.substr(start_pos, tag_end - start_pos + 1);  // exception absolute <tt:MetadataStream /> 
        if (start_tag.find("/>") != std::string::npos) {
            fetched_buffer_.push_back(xml_declaration + start_tag);
            pos = tag_end + 1;
            std::cout << "[HANWHA-ONVIF META] Found empty MetadataStream tag, added to fetched buffer" << std::endl;
            continue;
        }

        // find </tt:MetadataStream end tag
        size_t end_pos = xml_buffer_.find("</tt:MetadataStream>", tag_end);
        if (end_pos == std::string::npos) {
            std::cerr << "[HANWHA-ONVIF META] No MetadataStream end tag found" << std::endl;
            break; // end tag not found, wait for more data
        } else {
            size_t element_end = end_pos + std::string("</tt:MetadataStream>").length();
            std::string complete_element = xml_buffer_.substr(start_pos, element_end - start_pos);
            fetched_buffer_.push_back(xml_declaration + complete_element);
            std::cout << "[HANWHA-ONVIF META] Found complete MetadataStream element, added to fetched buffer" << std::endl;

            xml_buffer_.erase(0, element_end); // remove processed element from buffer
            pos = 0; // reset position for next search
        }

    }

    for (const std::string& xmlString : fetched_buffer_) {
        XMLDocument doc;
        XMLError par_result = doc.Parse(xmlString.c_str());
        
        if (par_result == XML_SUCCESS) {
            XMLElement* root = doc.RootElement();
            if (!root) {
                std::cerr << "[HANWHA-ONVIF META] No root element found" << std::endl;
                return false;
            }
            
            // 자식 요소가 있는지 확인
            bool hasChildren = root->FirstChildElement() != nullptr;
            if (!hasChildren) {
                std::cout << "[HANWHA-ONVIF META] Empty metadata document (no child elements)" << std::endl;
                return false;
            }
            
            
            const std::string whatIWant = "LicensePlate"; // Default type name
            // 객체 정보 추출 - 필터링을 위해 "LicensePlate"를 인자로 전달
            std::vector<Object> detections = extractObj(root, whatIWant);
            if (!detections.empty()) {
                std::cout << "[HANWHA-ONVIF META] Detected " << detections.size() << " LicensePlate(s):" << std::endl;
                result_ = detections; // Store the result_ in the provided vector
            } else {
                std::cout << "[HANWHA-ONVIF META] No "<< whatIWant << " objects found" << std::endl;
                result_.clear(); // Clear the results if no detections found
            }

        } else {
            std::cerr << "[HANWHA-ONVIF META] Failed to parse XML: " << doc.ErrorStr() << std::endl;
        }
    }
    
    // Clear processed streams
    fetched_buffer_.clear();
    cleanup();

    return true;
}


void MetadataParser::cleanup() {
    // MetadataStream 태그가 아닌 쓰레기 데이터 제거
    const std::string pattern = "<tt:MetadataStream";
    size_t next_metadata = xml_buffer_.find(pattern);
    
    if (next_metadata != std::string::npos && next_metadata > 0) {
        // MetadataStream 이전의 데이터 제거
        xml_buffer_.erase(0, next_metadata);
    } else if (next_metadata == std::string::npos) {
        // MetadataStream 태그가 없으면 버퍼의 대부분 제거 (마지막 일부만 보존)
        const size_t KEEP_SIZE = 100; // 다음 패킷에서 완성될 수 있는 부분
        if (xml_buffer_.size() > KEEP_SIZE) {
            xml_buffer_.erase(0, xml_buffer_.size() - KEEP_SIZE);
        }
    }
    
    // 버퍼 크기 제한
    const size_t MAX_BUFFER_SIZE = 512 * 1024; // 512KB
    if (xml_buffer_.size() > MAX_BUFFER_SIZE) {
        xml_buffer_.erase(0, xml_buffer_.size() - MAX_BUFFER_SIZE / 2);
    }
}