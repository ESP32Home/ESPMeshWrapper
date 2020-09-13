#include <ArduinoJson.h>

typedef void (*MeshCallback)(String &payload,String from,int flag);
class MeshApp{
    public:
        MeshApp();
        bool start(DynamicJsonDocument &config,DynamicJsonDocument &secret);
        void onMessage(MeshCallback cb);
        bool connected();
        bool send_down(String message);
        bool send_parent(String message);
        bool send_out(String ip_port,String message);
        void print_info();
};
