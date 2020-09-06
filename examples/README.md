# ESPMeshWrapper
An ESP-MESH wrapper library for an Arduino like API and using json for runtime configuration

# Complete mesh examples
flash the data file system containing config files `config.json` and the passwords `secret.json` with the command

    >pio run -t uploadfs

compile and flash

    >pio run
    >pio run -t upload

It's enough repeat for multiple ESP boards and watch the serial output.
Depending on the IDF version, some framework take time to scan (60 sec) before becoming a root node.

## code example
```c++
#include <Arduino.h>
#include <ArduinoJson.h>

#include <ConfigUtils.h>
#include <EspMeshWrapper.h>

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
```

## Mesh config
in `data\config.json`
```json
{
    "mesh":{
        "id":"77:77:77:77:77:77",
        "channel":0,
        "topology":"tree",
        "max_layer":6,
        "leaf":{
            "ap_expire_sec":60,
            "duty_type":"request",
            "demand_cycle":12
        },
        "parent":{
            "ap_connections":6,
            "route_table_size":50,
            "announce_short_ms":600,
            "announce_long_ms":3300,
            "network":{
                "duty":12,
                "duration":-1,
                "rule":"entire"
            }
        }
    }
}
```
## passwords config
in `data\secret.json`
```json
{
    "wifi":{
        "access_point":"MY-WLAN",
        "password":"**************"
    },
    "mesh":{
        "password":"**********"
    }
}
```

## log output root node
```console
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0018,len:4
load:0x3fff001c,len:1044
load:0x40078000,len:8896
load:0x40080400,len:5828
entry 0x400806ac
40 : Boot ready
159 : config loaded
cfg.mesh_id = 77:77:77:77:77:77
cfg.channel = 0
cfg.router.ssid = N-WLAN
cfg.router.password = ******
cfg.mesh_ap.password = ********
cfg.mesh_ap.max_connection = 6
<MESH_EVENT_STARTED>ID:77:77:77:77:77:77
1820 : setup() done
1821 : loop start cycle 1
*Layer=-1 Not root  NbNodes=0  TableSize=1
route table size = 1
  Sending to 80:7D:3A:D5:2F:CC
.....................
route table size = 1
  Sending to 80:7D:3A:D5:2F:CC
161841 : loop start cycle 17
*Layer=-1 Not root  NbNodes=0  TableSize=1
route table size = 1
  Sending to 80:7D:3A:D5:2F:CC
<MESH_EVENT_NO_PARENT_FOUND>scan times:60
<MESH_EVENT_FIND_NETWORK>new channel:8, router BSSID:00:00:00:00:00:00
171842 : loop start cycle 18
*Layer=-1 Not root  NbNodes=0  TableSize=1
route table size = 1
  Sending to 80:7D:3A:D5:2F:CC
E (175452) event: invalid static ip
<MESH_EVENT_PARENT_CONNECTED>layer:0-->1, parent:<ROOT>, ID:2C:30:33:9C:BF:C0
77:77:77:77:77:77
<MESH_EVENT_TODS_REACHABLE>state:0
<MESH_EVENT_ROUTER_SWITCH>new router:N-WLAN, channel:8, 2C:30:33:9C:BF:C0
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 1)
<MESH_EVENT_ROOT_ADDRESS>root address:RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 2)
80:7D:RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 3)
3A:D5:2F:CD
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 4)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 5)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 6)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 7)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 8)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 9)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 10)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 11)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 12)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 13)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 14)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 15)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 16)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 17)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 18)
<MESH_EVENT_ROOT_GOT_IP>sta ip:, mask: , gw:
1.2.5.10
255.255.255.255
1.2.5.10
181844 : loop start cycle 19
*Layer=1 Parent=2c:30:33:9c:bf:c0  root  NbNodes=1  TableSize=1
route table size = 1
  Sending to 80:7D:3A:D5:2F:CC
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 19)
<MESH_EVENT_ROUTING_TABLE_ADD>add 1, new:2
<MESH_EVENT_CHILD_CONNECTED>aid:1,3C:71:BF:83:BD:40
191851 : loop start cycle 20
*Layer=1 Parent=2c:30:33:9c:bf:c0  root  NbNodes=2  TableSize=2

route table size = 2
  Sending to 80:7D:3A:D5:2F:CC
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 20)
  Sending to 3C:71:BF:83:BD:40
```

## log output second node
```console
281870 : loop start cycle 29
*Layer=2 Parent=80:7d:3a:d5:2f:cd  Not root  NbNodes=2  TableSize=1
route table size = 1
  Sending to 3C:71:BF:83:BD:40
RX> from(3c:71:bf:83:bd:40) => (Hello dowlink neighbors : 29)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 30)

291878 : loop start cycle 30
*Layer=2 Parent=80:7d:3a:d5:2f:cd  Not root  NbNodes=2  TableSize=1
route table size = 1
  Sending to 3C:71:BF:83:BD:40
RX> from(3c:71:bf:83:bd:40) => (Hello dowlink neighbors : 30)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 31)

301886 : loop start cycle 31
*Layer=2 Parent=80:7d:3a:d5:2f:cd  Not root  NbNodes=2  TableSize=1
route table size = 1
  Sending to 3C:71:BF:83:BD:40
RX> from(3c:71:bf:83:bd:40) => (Hello dowlink neighbors : 31)
RX> from(80:7d:3a:d5:2f:cc) => (Hello dowlink neighbors : 32)
```
