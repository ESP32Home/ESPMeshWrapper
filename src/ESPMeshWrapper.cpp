#include "ESPMeshWrapper.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"

#define RX_SIZE          (1500)
#define TX_SIZE          (1460)
#define CONFIG_MESH_ROUTE_TABLE_SIZE_MAX 50

static bool is_mesh_connected = false;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;
static uint8_t tx_buf[TX_SIZE] = { 0, };
static uint8_t rx_buf[RX_SIZE] = { 0, };
static uint8_t rx_ext_buf[RX_SIZE] = { 0, };
static bool is_running = true;
static bool is_comm_p2p_started = false;

//reduced to singleton instance as the path such as mesh_event_handler, does not have data
MeshCallback message_callback = nullptr;

void string_to_char_len(String str,uint8_t* text,uint16_t &length){
    length = str.length();
    memcpy(text, str.c_str(), length);
    text[length] = '\0';//clean ending for printing
}

void string_to_char_len(String str,uint8_t* text,uint8_t &length){
    length = str.length();
    memcpy(text, str.c_str(), length);
    text[length] = '\0';//clean ending for printing
}

void string_to_char(String str,uint8_t* text){
    memcpy(text, str.c_str(), str.length());
    text[str.length()] = '\0';
}

void string_to_hextab(const char* str,mesh_addr_t &t){
    t.addr[0] = 0x77;
    t.addr[1] = 0x77;
    t.addr[2] = 0x77;
    t.addr[3] = 0x77;
    t.addr[4] = 0x77;
    t.addr[5] = 0x77;
}

void string_to_ip_port(String str,mesh_addr_t &add){
    int tp = str.indexOf(':');
    int p1 = str.indexOf('.');
    int p2 = str.indexOf('.',p1+1);
    int p3 = str.indexOf('.',p2+1);
    //Serial.printf("tp:%d p1,2,3:%d,%d,%d\n",tp,p1,p2,p3);

    uint16_t port = str.substring(tp+1).toInt();
    uint32_t ip1 = str.substring(0,p1).toInt();
    uint32_t ip2 = str.substring(p1+1,p2).toInt();
    uint32_t ip3 = str.substring(p2+1,p3).toInt();
    uint32_t ip4 = str.substring(p3+1,tp).toInt();
    //Serial.printf("port:%d ip1,2,3,4:%d,%d,%d,%d\n",port,ip1,ip2,ip3,ip4);

    add.mip.port = port;
    add.mip.ip4.addr = (ip1<<24) | (ip2<<16) | (ip3<<8) | (ip4);
    //Serial.printf("ip hex 0x%02x\n",add.mip.ip4.addr);
}

void ip_port_to_string(mesh_addr_t add,String &str){
    uint8_t u1 = (add.mip.ip4.addr>>24) & 0xff;
    uint8_t u2 = (add.mip.ip4.addr>>16) & 0xff;
    uint8_t u3 = (add.mip.ip4.addr>>8) & 0xff;
    uint8_t u4 =  add.mip.ip4.addr & 0xff;
    str = String(u1)+"."+String(u2)+"."+String(u3)+"."+String(u4)+":"+String(add.mip.port);
}

String hextab_to_string(uint8_t* add){
    String res;
    for(int i=0;i<5;i++){
        res = res + String((*add++),HEX)+":";
    }
    res = res + String((*add),HEX);
    return res;
}


void print_mac(uint8_t* add){
    for(int i=0;i<5;i++){
        Serial.printf("%02X:",(*add++));
    }
    Serial.printf("%02X\n",(*add));
}

void print_ip(ip4_addr_t add){
    uint8_t u1 = (add.addr>>24) & 0xff;
    uint8_t u2 = (add.addr>>16) & 0xff;
    uint8_t u3 = (add.addr>>8) & 0xff;
    uint8_t u4 = add.addr & 0xff;
    Serial.printf("%d.%d.%d.%d\n",u1,u2,u3,u4);
}

void esp_mesh_p2p_rx_main(void *arg)
{
    esp_err_t err;
    mesh_addr_t from;
    mesh_data_t data;
    int flag = 0;
    data.data = rx_buf;
    data.size = RX_SIZE;
    is_running = true;

    while (is_running) {
        data.size = RX_SIZE;
        err = esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
        if (err != ESP_OK || !data.size) {
            Serial.printf("error> 0x%x, size:%d\n", err, data.size);
            continue;
        }
        if(data.size<RX_SIZE){
            data.data[data.size] = 0;
        }
        String result(reinterpret_cast<char*>(data.data));
        String from_text = hextab_to_string(from.addr);
        message_callback(result,from_text,flag);
    }
    vTaskDelete(NULL);
}

void esp_mesh_ext_rx_main(void *arg)
{
    esp_err_t err;
    mesh_addr_t from_add;
    mesh_addr_t to_add;
    mesh_data_t data;
    int flag = 0;
    data.data = rx_ext_buf;
    data.size = RX_SIZE;

    while (is_comm_ext_started && is_mesh_connected) {
        data.size = RX_SIZE;
        err = esp_mesh_recv_toDS(&from_add,&to_add, &data, portMAX_DELAY, &flag, NULL, 0);
        if (err != ESP_OK || !data.size) {
            if(err != ESP_ERR_MESH_TIMEOUT){//otherwise don't bother with notifications for timeouts
                Serial.printf("error> 0x%x, size:%d\n", err, data.size);
            }
            continue;
        }
        if(data.size<RX_SIZE){//null termination for strings
            data.data[data.size] = 0;
        }
        String result(reinterpret_cast<char*>(data.data));
        String to_add_text;
        ip_port_to_string(to_add,to_add_text);
        Serial.printf("  message to external ip : %s\n",to_add_text.c_str());
        Serial.print("  => "+result);
        //String from_text = hextab_to_string(from_add.addr);
        //message_callback(result,from_text,flag);
    }
    vTaskDelete(NULL);
}

esp_err_t esp_mesh_comm_p2p_start(void)
{
    if (!is_comm_p2p_started) {
        is_comm_p2p_started = true;
        xTaskCreate(esp_mesh_p2p_rx_main, "MPRX", 3072, NULL, 5, NULL);
    }
    return ESP_OK;
}

esp_err_t esp_mesh_comm_p2p_stop(void)
{
    is_running = false;//the task will delete itself
    is_comm_p2p_started = false;//so that it can be created next time after next receive
    return ESP_OK;
}

esp_err_t esp_mesh_comm_ext_start(void)
{
    if (!is_comm_ext_started) {
        is_comm_ext_started = true;
        xTaskCreate(esp_mesh_ext_rx_main, "ETRX", 3072, NULL, 5, NULL);
    }
    return ESP_OK;
}

//will simply stop with "is_mesh_connected == false"
esp_err_t esp_mesh_comm_ext_stop(void)
{
    is_comm_ext_started = false;//the task will delete itself within timeout
    return ESP_OK;
}

void mesh_event_handler(mesh_event_t event)
{
    mesh_addr_t id = {0,};
    static uint8_t last_layer = 0;

    switch (event.id) {
    case MESH_EVENT_STARTED:
        esp_mesh_get_id(&id);
        Serial.printf("<MESH_EVENT_STARTED>ID:");
        print_mac(id.addr);
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
        break;
    case MESH_EVENT_STOPPED:
        Serial.printf("<MESH_EVENT_STOPPED>\n");
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
        break;
    case MESH_EVENT_CHILD_CONNECTED:
        Serial.printf("<MESH_EVENT_CHILD_CONNECTED>aid:%d,",event.info.child_connected.aid);
        print_mac(event.info.child_connected.mac);
        break;
    case MESH_EVENT_CHILD_DISCONNECTED:
        Serial.printf("<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, ",event.info.child_disconnected.aid);
        print_mac(event.info.child_disconnected.mac);
        break;
    case MESH_EVENT_ROUTING_TABLE_ADD:
        Serial.printf("<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d\n",
                 event.info.routing_table.rt_size_change,
                 event.info.routing_table.rt_size_new);
        break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE:
        Serial.printf("<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d\n",
                 event.info.routing_table.rt_size_change,
                 event.info.routing_table.rt_size_new);
        break;
    case MESH_EVENT_NO_PARENT_FOUND:
        Serial.printf("<MESH_EVENT_NO_PARENT_FOUND>scan times:%d\n",event.info.no_parent.scan_times);
        /* TODO handler for the failure */
        break;
    case MESH_EVENT_PARENT_CONNECTED:
        esp_mesh_get_id(&id);
        mesh_layer = event.info.connected.self_layer;
        memcpy(&mesh_parent_addr.addr, event.info.connected.connected.bssid, 6);
        Serial.printf("<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:%s, ID:",
                 last_layer, mesh_layer, 
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "");
                 print_mac(mesh_parent_addr.addr);
                 print_mac(id.addr);
        last_layer = mesh_layer;
        is_mesh_connected = true;
        if (esp_mesh_is_root()) {
            tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
        }
        esp_mesh_comm_p2p_start();
        //after esp_mesh_comm_p2p_start()
        if (esp_mesh_is_root()) {
            esp_mesh_comm_ext_start();//will stop when is_mesh_connected <= false
        }
        break;
    case MESH_EVENT_PARENT_DISCONNECTED:
        Serial.printf("<MESH_EVENT_PARENT_DISCONNECTED>reason:%d\n",event.info.disconnected.reason);
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
        //do not stop here as children might still be connected and send messages
        break;
    case MESH_EVENT_LAYER_CHANGE:
        mesh_layer = event.info.layer_change.new_layer;
        Serial.printf("<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>\n" : "\n");
        last_layer = mesh_layer;
        if (esp_mesh_is_root()) {
            esp_mesh_comm_ext_start();//will stop when is_mesh_connected <= false
        }
        break;
    case MESH_EVENT_ROOT_ADDRESS:
        Serial.printf("<MESH_EVENT_ROOT_ADDRESS>root address:");
                 print_mac(event.info.root_addr.addr);
        break;
    case MESH_EVENT_ROOT_GOT_IP:
        /* root starts to connect to server */
        Serial.printf("<MESH_EVENT_ROOT_GOT_IP>sta ip:, mask: , gw: \n");
                 print_ip(event.info.got_ip.ip_info.ip);
                 print_ip(event.info.got_ip.ip_info.netmask);
                 print_ip(event.info.got_ip.ip_info.gw);
        break;
    case MESH_EVENT_ROOT_LOST_IP:
        Serial.printf("<MESH_EVENT_ROOT_LOST_IP>\n");
        break;
    case MESH_EVENT_VOTE_STARTED:
        Serial.printf("<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:",
                 event.info.vote_started.attempts,
                 event.info.vote_started.reason);
                 print_mac(event.info.vote_started.rc_addr.addr);
        break;
    case MESH_EVENT_VOTE_STOPPED:
        Serial.printf("<MESH_EVENT_VOTE_STOPPED>\n");
        break;
    case MESH_EVENT_ROOT_SWITCH_REQ:
        Serial.printf("<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:",
                 event.info.switch_req.reason);
                 print_mac( event.info.switch_req.rc_addr.addr);
        break;
    case MESH_EVENT_ROOT_SWITCH_ACK:
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&mesh_parent_addr);
        Serial.printf("<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:", mesh_layer);
        print_mac(mesh_parent_addr.addr);
        if (esp_mesh_is_root()) {
            esp_mesh_comm_ext_start();//will stop when is_mesh_connected <= false
        }
        break;
    case MESH_EVENT_TODS_STATE:
        Serial.printf("<MESH_EVENT_TODS_REACHABLE>state:%d\n",event.info.toDS_state);
        break;
    case MESH_EVENT_ROOT_FIXED:
        Serial.printf("<MESH_EVENT_ROOT_FIXED>%s\n",event.info.root_fixed.is_fixed ? "fixed" : "not fixed");
        break;
    case MESH_EVENT_ROOT_ASKED_YIELD:
        Serial.printf("<MESH_EVENT_ROOT_ASKED_YIELD>, rssi:%d, capacity:%d",
                 event.info.root_conflict.rssi,
                 event.info.root_conflict.capacity);
                 print_mac(event.info.root_conflict.addr);
        break;
    case MESH_EVENT_CHANNEL_SWITCH:
        Serial.printf("<MESH_EVENT_CHANNEL_SWITCH>new channel:%d\n", event.info.channel_switch.channel);
        break;
    case MESH_EVENT_SCAN_DONE:
        Serial.printf("<MESH_EVENT_SCAN_DONE>number:%d\n",event.info.scan_done.number);
        break;
    case MESH_EVENT_NETWORK_STATE:
        Serial.printf("<MESH_EVENT_NETWORK_STATE>is_rootless:%d\n",event.info.network_state.is_rootless);
        break;
    case MESH_EVENT_STOP_RECONNECTION:
        Serial.printf("<MESH_EVENT_STOP_RECONNECTION>\n");
        break;
    case MESH_EVENT_FIND_NETWORK:
        Serial.printf("<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:",event.info.find_network.channel);
        print_mac(event.info.find_network.router_bssid);
        break;
    case MESH_EVENT_ROUTER_SWITCH:
        Serial.printf("<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, ",
                 event.info.router_switch.ssid, event.info.router_switch.channel);
                 print_mac(event.info.router_switch.bssid);
        break;
    default:
        Serial.printf("esp_event_handler> unknown id:%d\n", event.id);
        break;
    }
}


MeshApp::MeshApp(){
}

bool MeshApp::start(DynamicJsonDocument &config,DynamicJsonDocument &secret){

    if(!config.containsKey("mesh")){
        Serial.printf("Error> config has no key 'mesh'");
        return false;
    }
    if(!secret.containsKey("wifi")){
        Serial.printf("Error> secret has no key 'wifi'");
        return false;
    }
    tcpip_adapter_init();
    esp_err_t err;
    err = tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
        if(err != ESP_OK)Serial.printf("tcpip_adapter_dhcps_stop: 0x%X\n",err);
    err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
        if(err != ESP_OK)Serial.printf("tcpip_adapter_dhcps_stop: 0x%X\n",err);
    err = esp_event_loop_init(NULL, NULL);
        if(err != ESP_OK)Serial.printf("esp_event_loop_init: 0x%X\n",err);
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_config);
        if(err != ESP_OK)Serial.printf("esp_wifi_initt: 0x%X\n",err);
    err = esp_wifi_start();
        if(err != ESP_OK)Serial.printf("esp_wifi_start: 0x%X\n",err);

    err = esp_mesh_init();
        if(err != ESP_OK)Serial.printf("esp_mesh_init: 0x%X\n",err);
    
    //esp_mesh_set_topology()               // Not available in 3.3
    //esp_mesh_set_active_duty_cycle()      // Not available in 3.3

    err = esp_mesh_set_announce_interval(config["mesh"]["parent"]["announce_short_ms"],config["mesh"]["parent"]["announce_long_ms"]);

    err = esp_mesh_set_max_layer(config["mesh"]["max_layer"]);
        if(err != ESP_OK)Serial.printf("esp_mesh_set_max_layer: 0x%X\n",err);
    err = esp_mesh_set_vote_percentage(1);
        if(err != ESP_OK)Serial.printf("esp_mesh_set_vote_percentage: 0x%X\n",err);
    err = esp_mesh_set_ap_assoc_expire(10);
        if(err != ESP_OK)Serial.printf("esp_mesh_set_ap_assoc_expire: 0x%X\n",err);

    err = esp_mesh_set_ap_authmode(WIFI_AUTH_WPA_WPA2_PSK);
        if(err != ESP_OK)Serial.printf("esp_mesh_set_ap_authmode: 0x%X\n",err);

    mesh_cfg_t cfg;
    cfg.crypto_funcs = &g_wifi_default_mesh_crypto_funcs;
    cfg.event_cb = &mesh_event_handler;

    string_to_hextab(config["mesh"]["id"],cfg.mesh_id);
    cfg.channel = config["mesh"]["channel"];
    string_to_char_len(secret["wifi"]["access_point"],cfg.router.ssid,cfg.router.ssid_len);
    string_to_char(secret["wifi"]["password"],cfg.router.password);
    string_to_char(secret["mesh"]["password"],cfg.mesh_ap.password);

    cfg.mesh_ap.max_connection = config["mesh"]["parent"]["ap_connections"];
    
    Serial.printf("cfg.mesh_id = ");print_mac(cfg.mesh_id.addr);
    Serial.printf("cfg.channel = %d\n",cfg.channel);
    Serial.printf("cfg.router.ssid = %s\n",cfg.router.ssid);
    Serial.printf("cfg.router.password = %s\n",cfg.router.password);
    Serial.printf("cfg.mesh_ap.password = %s\n",cfg.mesh_ap.password);
    Serial.printf("cfg.mesh_ap.max_connection = %d\n",cfg.mesh_ap.max_connection);
    

    err = esp_mesh_set_config(&cfg);
        if(err != ESP_OK)Serial.printf("esp_mesh_set_config: 0x%X\n",err);
    /* mesh start */
    esp_err_t res = esp_mesh_start();
    if(res != ESP_OK){
        Serial.printf("mesh start failed, heap:%d, %s\n",  esp_get_free_heap_size(),
                esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed");
    }

    return res;
}

void MeshApp::onMessage(MeshCallback cb){
    message_callback = cb;
}

bool MeshApp::send_down(String message){
    int i;
    esp_err_t err;
    mesh_addr_t route_table[CONFIG_MESH_ROUTE_TABLE_SIZE_MAX];
    int route_table_size = 0;
    mesh_data_t data;
    data.data = tx_buf;
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;
    if(!is_running){
        return false;
    }

    err = esp_mesh_get_routing_table(route_table,CONFIG_MESH_ROUTE_TABLE_SIZE_MAX * sizeof(mesh_addr_t), &route_table_size);
    if (err != ESP_OK) {
        Serial.printf("error>on esp_mesh_get_routing_table(size=%d) [Layer:%d] [heap:%d] [err:0x%x]\n",
                    route_table_size,mesh_layer,esp_get_minimum_free_heap_size(),err);
        return false;
    }

    string_to_char_len(message,data.data,data.size);

    Serial.printf("route table size = %d\n",route_table_size);
    for (i = 0; i < route_table_size; i++) {
        Serial.printf("  Sending to ");
        print_mac(route_table[i].addr);
        err = esp_mesh_send(&route_table[i], &data, MESH_DATA_P2P, NULL, 0);
        if (err) {
            Serial.printf("error>on(%d/%d) [Layer:%d] [heap:%d] [err:0x%x, proto:%d, tos:%d] *parent *dest\n",
                        i,route_table_size,
                        mesh_layer,esp_get_minimum_free_heap_size(),err, data.proto, data.tos);
            print_mac(mesh_parent_addr.addr);
            print_mac(route_table[i].addr);
            return false;
        }
    }
    return true;
}

bool MeshApp::send_parent(String message){
    esp_err_t err;
    mesh_data_t data;
    data.data = tx_buf;
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;
    if(!is_running){
        return false;
    }

    string_to_char_len(message,data.data,data.size);

    if(esp_mesh_is_root()){
        Serial.printf("  error>Node is ROOT, can't send to parent router\n");
        return false;
    }

    Serial.printf("  Sending to Parent :");
    print_mac(mesh_parent_addr.addr);

    err = esp_mesh_send(&mesh_parent_addr, &data, MESH_DATA_P2P, NULL, 0);
    if (err) {
        Serial.printf("  error:0x%x>[Layer:%d] [heap:%d] [proto:%d, tos:%d] *parent\n",
                    err,mesh_layer,esp_get_minimum_free_heap_size(),data.proto, data.tos);
        print_mac(mesh_parent_addr.addr);
        return false;
    }
    return true;
}

bool MeshApp::send_out(String ip_port,String message){
    esp_err_t err;
    mesh_data_t data;
    data.data = tx_buf;
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;        //Type Of Service, Point 2 Point reliable
    int flag = MESH_DATA_TODS;      //To external IP
    mesh_addr_t ext_addr;

    if(!is_running){
        return false;
    }

    string_to_ip_port(ip_port,ext_addr);
    string_to_char_len(message,data.data,data.size);
    
    Serial.printf("  Sending to external address : %s\n",ip_port.c_str());

    err = esp_mesh_send(&ext_addr, &data, flag, NULL, 0);
    if (err) {
        String used_addr;
        ip_port_to_string(ext_addr,used_addr);
        Serial.printf("  error:0x%x>[Layer:%d] [heap:%d] [proto:%d, tos:%d] =>(%s)\n",
                    err,mesh_layer,esp_get_minimum_free_heap_size(),data.proto, data.tos,used_addr.c_str());
        return false;
    }else{
        Serial.printf("  esp_mesh_send ESP_OK\n");
        return true;
    }
}

void MeshApp::print_info(){
    esp_err_t err;
    String info;

    info = "Layer="+String(esp_mesh_get_layer());

    info += is_mesh_connected?" 'connected'":" 'not connected'";

    mesh_addr_t bssid;
    err = esp_mesh_get_parent_bssid(&bssid);
    if(err == ESP_OK){
        info += " Parent="+hextab_to_string(bssid.addr)+" ";
    }
    if(!esp_mesh_is_root()){
        info += " Not";
    }
    info += " root  ";

    info += "NbNodes="+String(esp_mesh_get_total_node_num());
    info += "  TableSize="+String(esp_mesh_get_routing_table_size());
    mesh_rx_pending_t pending;
    esp_mesh_get_rx_pending(&pending);
    info += "  pending toDS="+String(pending.toDS)+" toSelf"+String(pending.toSelf);

    Serial.println(info);
}

bool MeshApp::connected(){
    return is_mesh_connected;
}