#include "../src/wakatime.h"
#include <iostream>
int main() {
    activity::Context c;c.window="synthetic-window";c.file="C:\\synthetic\\example.sql";c.connection="DO_NOT_SEND";
    if(activity::base64("12345")!="MTIzNDU=") return 1;
    activity::Context reference;reference.objectName="SYNTHETIC.TABLE";
    if(activity::wakaPayload(1791187200000,reference).find("PL/SQL Developer: SYNTHETIC.TABLE")==std::string::npos) return 6;
    activity::WakaTime client("synthetic-key",56662);
    if(client.send(1791187200000,c,56662)) return 2; // Mock returns 503 first.
    if(!client.send(1791187215000,c,56662)) return 3;
    if(!client.send(1791187230000,c,56662)) return 4; // Throttled; no request.
    c.file.clear();c.owner="SYNTHETIC_OWNER";c.objectName="SYNTHETIC_OBJECT";
    if(!client.send(1791187245000,c,56662)) return 5;
    std::cout<<"PASS WakaTime authentication, retry, throttle and unsaved context checks\n";return 0;
}
