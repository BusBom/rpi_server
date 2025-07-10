#include "yolo-fastestv2.h"

int main(int argc, char** argv)
{   
    static const char* class_names[] = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
        "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
        "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
        "hair drier", "toothbrush"
    };
    
    yoloFastestv2 api;

    api.loadModel("yolo-fastestv2-opt.param",
                  "yolo-fastestv2-opt.bin");


    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video capture." << std::endl;
        return -1;
    }
    cv::Mat frame;

    while (true){
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "Error: Could not read frame from video capture." << std::endl;
            break;  
        }
        std::vector<TargetBox> boxes;
        api.detection(frame, boxes);
    
        for (int i = 0; i < boxes.size(); i++) {
            std::cout<<boxes[i].x1<<" "<<boxes[i].y1<<" "<<boxes[i].x2<<" "<<boxes[i].y2
                     <<" "<<boxes[i].score<<" "<<boxes[i].cate<<std::endl;
            
            char text[256];
            sprintf(text, "%s %.1f%%", class_names[boxes[i].cate], boxes[i].score * 100);
    
            int baseLine = 0;
            cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
    
            int x = boxes[i].x1;
            int y = boxes[i].y1 - label_size.height - baseLine;
            if (y < 0)
                y = 0;
            if (x + label_size.width > frame.cols)
                x = frame.cols - label_size.width;
    
            cv::rectangle(frame, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                          cv::Scalar(255, 255, 255), -1);
    
            cv::putText(frame, text, cv::Point(x, y + label_size.height),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
    
            cv::rectangle (frame, cv::Point(boxes[i].x1, boxes[i].y1), 
                           cv::Point(boxes[i].x2, boxes[i].y2), cv::Scalar(255, 255, 0), 2, 2, 0);
        }
        
        cv::imwrite("output.jpg", frame);
    }
    return 0;
}
