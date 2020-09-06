#include <ArduinoJson.h>

typedef void (*MeshCallback)(String &payload,String from,int flag);
class MeshApp{
    public:
        MeshApp();
        bool start(DynamicJsonDocument &config,DynamicJsonDocument &secret);
        void onMessage(MeshCallback cb);
        bool send_down(String message);
        void print_info();
};
