#include <ArduinoJson.h>
#include <ConfigUtils.h>

#include <EspMeshWrapper.h>

//see github repo readme for example config files and expected output log :
//https://github.com/ESP32Home/ESPMeshWrapper/blob/master/README.md

DynamicJsonDocument config(5*1024);//5 KB
DynamicJsonDocument secret(1*1024);//1 KB
MeshApp mesh;

uint32_t cycle_count = 0;

void meshMessage(String &payload,String from,int flag){
  Serial.printf("RX> from(%s) => (%s)\n",from.c_str(),payload.c_str());
}

void setup() {
  
  Serial.begin(115200);
  timelog("Boot ready");

  load_json(config,"/config.json");
  load_json(secret,"/secret.json");
  timelog("config loaded");

  mesh.start(config,secret);
  mesh.onMessage(meshMessage);


  timelog("setup() done");

}

void loop() {
  cycle_count++;
  timelog("loop start cycle "+String(cycle_count));
  mesh.print_info();
  mesh.send_down("Hello dowlink neighbors : "+String(cycle_count));
  delay(10000);
}
