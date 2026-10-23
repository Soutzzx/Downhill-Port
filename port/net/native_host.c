/* Native, single-EXE Player Hosted relay for Windows. No Node, Wrangler,
   executable extraction or shell processes. The listener never runs the
   simulation: it only handles the v1 room protocol and relays race frames.
   One host, one private room, up to 8 players, on TCP 8787. */
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "json.h"
#include "native_host.h"

enum { HOST_PORT = 8787, CLIENTS = 12, MAX_MEMBERS = 8, IN_CAP = 16384,
       OUT_CAP = 131072, FRAME_CAP = 4096, TICK_MS = 100 };
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define ALPHABET "23456789ABCDEFGHJKMNPQRSTUVWXYZ"

typedef struct {
    SOCKET fd;
    int websocket, member, slot, create, closing;
    unsigned char input[IN_CAP];
    int input_n;
    unsigned char output[OUT_CAP];
    int output_n;
    uint64_t connected_at;
    int ping_ms;
    uint64_t ping_sent;
} Peer;
typedef struct {
    int peer; /* index into peers, -1 if empty */
    int id, ready, rider, bike, slot;
    char name[64];
} Member;

static Peer peers[CLIENTS];
static Member members[MAX_MEMBERS];
static SOCKET listener = INVALID_SOCKET;
static HANDLE worker;
static volatile LONG stop_requested, server_state;
static char error_message[160];
static char room_code[7], host_token[37], build_version[70], iso_id[70];
static int host_id, member_count, next_id, max_players = 8, track, laps, racing;
static uint32_t race_seed;
static uint64_t start_at, last_ping, last_register;
static uint64_t last_field[MAX_MEMBERS];
static CRITICAL_SECTION guard;
static int guard_initialized;

static uint64_t now_ms(void) {
    FILETIME t;
    ULARGE_INTEGER n;
    GetSystemTimeAsFileTime(&t);
    n.LowPart = t.dwLowDateTime; n.HighPart = t.dwHighDateTime;
    return n.QuadPart / 10000ULL - 11644473600000ULL;
}
static void fail(const char *msg) {
    snprintf(error_message, sizeof error_message, "%s", msg);
    InterlockedExchange(&server_state, NATIVE_HOST_FAILED);
}
static int random_bytes(unsigned char *buffer, size_t n) {
    return BCryptGenRandom(NULL, buffer, (ULONG)n,
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}
static void make_code(void) {
    unsigned char bytes[20];
    if (!random_bytes(bytes, sizeof bytes)) {
        fail("Windows random generator is unavailable.");
        return;
    }
    room_code[0] = 'H';
    for (int i = 1; i < 6; ++i) room_code[i] = ALPHABET[bytes[i] % (sizeof(ALPHABET)-1)];
    room_code[6] = 0;
    /* Not a password; prevents other hosts overriding a live directory entry. */
    snprintf(host_token, sizeof host_token,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],
             bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],
             bytes[12],bytes[13],bytes[14],bytes[15]);
}
static void b64(const unsigned char *bytes, size_t len, char *out, size_t cap) {
    static const char abc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t k = 0;
    for (size_t i = 0; i < len && k + 5 < cap; i += 3) {
        unsigned a = bytes[i], b = i+1 < len ? bytes[i+1] : 0, c = i+2 < len ? bytes[i+2] : 0;
        out[k++] = abc[a >> 2]; out[k++] = abc[((a&3)<<4)|(b>>4)];
        out[k++] = i+1 < len ? abc[((b&15)<<2)|(c>>6)] : '=';
        out[k++] = i+2 < len ? abc[c&63] : '=';
    }
    out[k] = 0;
}
static int websocket_accept(const char *key, char *out, size_t cap) {
    char source[128];
    unsigned char digest[20];
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    if (strlen(key) != 24) return 0;
    for (int i = 0; i < 24; ++i) {
        char x = key[i];
        if (!(x >= 'A' && x <= 'Z') && !(x >= 'a' && x <= 'z') &&
            !(x >= '0' && x <= '9') && x != '+' && x != '/' && x != '=') return 0;
    }
    snprintf(source, sizeof source, "%s%s", key, WS_GUID);
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, NULL, 0) != 0) return 0;
    if (BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) != 0) goto failed;
    if (BCryptHashData(hash, (PUCHAR)source, (ULONG)strlen(source), 0) != 0) goto failed;
    if (BCryptFinishHash(hash, digest, sizeof digest, 0) != 0) goto failed;
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg, 0);
    b64(digest, sizeof digest, out, cap);
    return 1;
failed:
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    return 0;
}
static int append(Peer *p, const void *data, int len) {
    if (len < 0 || p->output_n + len > OUT_CAP) { p->closing = 1; return 0; }
    memcpy(p->output + p->output_n, data, (size_t)len);
    p->output_n += len;
    return 1;
}
static void frame(Peer *p, int opcode, const void *data, int len) {
    unsigned char h[4] = {(unsigned char)(0x80 | opcode),0,0,0};
    int header = 2;
    if (len < 0 || len > FRAME_CAP) { p->closing = 1; return; }
    if (len < 126) h[1] = (unsigned char)len;
    else { h[1]=126; h[2]=(unsigned char)(len>>8); h[3]=(unsigned char)len; header=4; }
    if (p->output_n + header + len > OUT_CAP) { p->closing = 1; return; }
    append(p,h,header); append(p,data,len);
}
static void send_text(Peer *p, const char *s) { frame(p,1,s,(int)strlen(s)); }
static void reply_error(Peer *p,const char *code,const char *msg) {
    char e[256], escaped[170];
    json_escape(escaped,sizeof escaped,msg);
    snprintf(e,sizeof e,"{\"type\":\"error\",\"code\":\"%s\",\"message\":\"%s\"}",code,escaped);
    send_text(p,e);
}
static void send_http(Peer *p,int status,const char *type,const char *content) {
    char header[320];int n=(int)strlen(content);
    int count=snprintf(header,sizeof header,
       "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",
       status,status==200?"OK":status==404?"Not Found":"Bad Request",type,n);
    append(p,header,count); append(p,content,n);p->closing=1;
}
static int member_index(int peer) {
    for(int i=0;i<MAX_MEMBERS;i++) if(members[i].peer==peer) return i;
    return -1;
}
static int host_member(void) {
    for(int i=0;i<MAX_MEMBERS;i++) if(members[i].peer>=0&&members[i].id==host_id)return i;
    return -1;
}
static void publish_room(void) {
    char msg[4096],name[150],version[150];
    json_escape(version,sizeof version,build_version);
    int len=snprintf(msg,sizeof msg,
       "{\"type\":\"room\",\"code\":\"%s\",\"hostId\":%d,\"roomName\":\"Player Hosted\","
       "\"public\":false,\"maxPlayers\":%d,\"started\":%s,\"version\":\"%s\","
       "\"settings\":{\"track\":%d,\"laps\":%d,\"seed\":%u},\"inputDelay\":1,\"members\":[",
       room_code,host_id,max_players,racing?"true":"false",version,track,laps,race_seed);
    int first=1;
    for(int i=0;i<MAX_MEMBERS;i++){
        Member *m=&members[i]; if(m->peer<0)continue;
        json_escape(name,sizeof name,m->name);
        int wrote=snprintf(msg+len,sizeof msg-len,
           "%s{\"id\":%d,\"name\":\"%s\",\"ready\":%s,\"rider\":%d,\"bike\":%d,\"rttMs\":%d}",
           first?"":",",m->id,name,m->ready?"true":"false",m->rider,m->bike,
           peers[m->peer].ping_ms);if(wrote<0||wrote>=(int)sizeof msg-len)return;
        len+=wrote;first=0;
    }
    if(len+3>sizeof msg)return;
    memcpy(msg+len,"]}",3);
    for(int i=0;i<MAX_MEMBERS;i++)
        if(members[i].peer>=0) send_text(&peers[members[i].peer],msg);
}
static void member_left(int at, const char *reason) {
    if(at<0||at>=MAX_MEMBERS||members[at].peer<0)return;
    int member=members[at].id, slot=members[at].slot;
    if(racing&&slot>=0){
        char msg[180];
        snprintf(msg,sizeof msg,
           "{\"type\":\"left\",\"slot\":%d,\"memberId\":%d,\"fromField\":null,\"reason\":\"%s\"}",
           slot,member,reason);
        for(int i=0;i<MAX_MEMBERS;i++)if(i!=at&&members[i].peer>=0)
            send_text(&peers[members[i].peer],msg);
    }
    members[at].peer=-1;members[at].slot=-1;member_count--;
    if(member==host_id&&member_count) {
        for(int i=0;i<MAX_MEMBERS;i++)if(members[i].peer>=0){host_id=members[i].id;break;}
    }
    if(!member_count){build_version[0]=iso_id[0]=0; racing=0;host_id=-1;next_id=0;}
    else publish_room();
}
static void disconnect_peer(int i) {
    Peer *p=&peers[i];
    int m=member_index(i);
    if(m>=0)member_left(m,"disconnect");
    if(p->fd!=INVALID_SOCKET)closesocket(p->fd);
    memset(p,0,sizeof *p);
    p->fd=INVALID_SOCKET;p->member=-1;p->slot=-1;
}
static void fatal(Peer *p,const char *code,const char *msg) {
    reply_error(p,code,msg);p->closing=1;
}
static void welcome(int peer,int member,int host) {
    char s[256];
    snprintf(s,sizeof s,
       "{\"type\":\"welcome\",\"protocol\":1,\"code\":\"%s\",\"memberId\":%d,"
       "\"host\":%s,\"serverTime\":%llu}",
       room_code,member,host?"true":"false",(unsigned long long)now_ms());
    send_text(&peers[peer],s);
}
static void start_race(void) {
    if(member_count<2||racing)return;
    for(int i=0;i<MAX_MEMBERS;i++)
        if(members[i].peer>=0&&!members[i].ready) return;
    int order[MAX_MEMBERS], count=0, h=host_member();
    if(h<0)return;
    order[count++]=h;
    for(int i=0;i<MAX_MEMBERS;i++)if(members[i].peer>=0&&i!=h)order[count++]=i;
    unsigned char seed[4];if(!random_bytes(seed,4))return;
    race_seed=(uint32_t)seed[0]|((uint32_t)seed[1]<<8)|((uint32_t)seed[2]<<16)|((uint32_t)seed[3]<<24);
    racing=1;start_at=now_ms()+3000;
    for(int i=0;i<count;i++)members[order[i]].slot=i;
    for(int k=0;k<count;k++){
        char s[4096], ename[150];
        int len=snprintf(s,sizeof s,
            "{\"type\":\"start\",\"yourSlot\":%d,\"track\":%d,\"laps\":%d,"
            "\"seed\":%u,\"inputDelay\":1,\"serverTime\":%llu,"
            "\"startAt\":%llu,\"players\":[",k,track,laps,race_seed,
            (unsigned long long)now_ms(),(unsigned long long)start_at);
        for(int j=0;j<count;j++){
            Member *m=&members[order[j]];
            json_escape(ename,sizeof ename,m->name);
            int w=snprintf(s+len,sizeof s-len,
              "%s{\"slot\":%d,\"memberId\":%d,\"name\":\"%s\",\"rider\":%d,\"bike\":%d}",
              j?",":"",j,m->id,ename,m->rider,m->bike);
            if(w<0||w>=(int)sizeof s-len)return;
            len+=w;
        }
        if(len+3>sizeof s)return;
        memcpy(s+len,"]}",3);
        send_text(&peers[members[order[k]].peer],s);
    }
    publish_room();
}
static void process_json(int peer,const char *text) {
    Peer *p=&peers[peer];
    char type[32];long long v;
    if(!json_string(text,"type",type,sizeof type)){fatal(p,"bad_message","Invalid message.");return;}
    int m=member_index(peer);
    if(m<0){
        if(strcmp(type,"hello")){fatal(p,"bad_message","Send hello first.");return;}
        char version[70],iso[70],name[70];long long protocol;
        if(!json_int(text,"protocol",&protocol)||protocol!=1||
           !json_string(text,"version",version,sizeof version)||!version[0]||
           !json_string(text,"isoSha256",iso,sizeof iso)||strlen(iso)!=64||
           !json_string(text,"name",name,sizeof name)||!name[0]) {
            fatal(p,"bad_message","Invalid hello fields.");return;
        }
        if(p->create&&!member_count){
            snprintf(build_version,sizeof build_version,"%s",version);
            snprintf(iso_id,sizeof iso_id,"%s",iso);
        }
        else {
            if(!member_count){fatal(p,"room_not_found","No room has this code.");return;}
            if(racing){fatal(p,"room_started","Race already running.");return;}
            if(strcmp(build_version,version)){fatal(p,"version_mismatch","Different game builds.");return;}
            if(strcmp(iso_id,iso)){fatal(p,"iso_mismatch","Incompatible game data.");return;}
            if(member_count>=max_players){fatal(p,"room_full","Room is full.");return;}
        }
        int index=-1;
        for(int i=0;i<MAX_MEMBERS;i++)if(members[i].peer<0){index=i;break;}
        if(index<0){fatal(p,"room_full","Room is full.");return;}
        Member *joined=&members[index];
        joined->peer=peer;joined->id=next_id++;joined->ready=0;
        joined->rider=joined->bike=0;joined->slot=-1;
        snprintf(joined->name,sizeof joined->name,"%s",name);
        p->member=joined->id;
        if(!member_count)host_id=joined->id;
        member_count++;
        welcome(peer,joined->id,member_count==1);
        publish_room();
        return;
    }
    int is_host=members[m].id==host_id;
    if(!strcmp(type,"ping")){
        double time_value=0;
        char msg[256];
        json_number(text,"t",&time_value);
        snprintf(msg,sizeof msg,"{\"type\":\"pong\",\"t\":%.3f,\"serverTime\":%llu}",
                 time_value,(unsigned long long)now_ms());
        send_text(p,msg);
    } else if(!strcmp(type,"pong")){
        if(json_int(text,"s",&v)&&v>=0&&v<=(long long)now_ms())
            p->ping_ms=(int)(now_ms()-(uint64_t)v);
    } else if(!strcmp(type,"ready")){
        if(racing)return;
        int ready=0;
        if(json_bool(text,"ready",&ready)){members[m].ready=ready;publish_room();}
    } else if(!strcmp(type,"pick")){
        if(racing)return;
        int target=m;long long id;
        if(json_int(text,"member",&id)&&id!=members[m].id) {
            if(!is_host){reply_error(p,"not_host","Host only.");return;}
            target=-1;for(int i=0;i<MAX_MEMBERS;i++)
                if(members[i].peer>=0&&members[i].id==id){target=i;break;}
            if(target<0)return;
        }
        if(json_int(text,"rider",&v)&&v>=0&&v<=255)members[target].rider=(int)v;
        if(json_int(text,"bike",&v)&&v>=0&&v<=255)members[target].bike=(int)v;
        publish_room();
    } else if(!strcmp(type,"settings")){
        if(!is_host||racing)return;
        if(json_int(text,"track",&v)&&v>=0&&v<=255)track=(int)v;
        if(json_int(text,"laps",&v)&&v>=0&&v<=255)laps=(int)v;
        if(json_int(text,"maxPlayers",&v)&&v>=member_count&&v>=2&&v<=8)max_players=(int)v;
        publish_room();
    } else if(!strcmp(type,"start")){
        if(is_host)start_race();
    } else if(!strcmp(type,"end")){
        if(is_host&&racing){
            racing=0;
            for(int i=0;i<MAX_MEMBERS;i++)if(members[i].peer>=0){
                members[i].ready=0;members[i].slot=-1;
                send_text(&peers[members[i].peer],"{\"type\":\"ended\"}");
            }
            publish_room();
        }
    } else if(!strcmp(type,"leave")){
        member_left(m,"leave");p->closing=1;
    } else if(!strcmp(type,"kick")){
        if(!is_host)return;
        if(json_int(text,"member",&v)){
            for(int i=0;i<MAX_MEMBERS;i++)if(members[i].peer>=0&&
                members[i].id==v&&i!=m) {
                int target=members[i].peer;
                member_left(i,"kicked");
                fatal(&peers[target],"kicked","Removed by host.");
                break;
            }
        }
    }
}
static void process_binary(int peer,unsigned char *buf,int len) {
    Peer *p=&peers[peer];
    int m=member_index(peer);
    if(m<0||!racing||members[m].slot<0||len<2||len>256)return;
    int slot=members[m].slot;
    buf[1]=(unsigned char)slot;
    if(buf[0]==3){buf[0]=4;frame(p,2,buf,len);return;}
    if(buf[0]==5||buf[0]==6||buf[0]==1){
        for(int i=0;i<MAX_MEMBERS;i++)
            if(i!=m&&members[i].peer>=0&&members[i].slot>=0)
                frame(&peers[members[i].peer],2,buf,len);
    }
}
static int process_http(int peer) {
    Peer *p=&peers[peer];
    char *end=NULL;
    for(int i=3;i<p->input_n;i++)
        if(!memcmp(p->input+i-3,"\r\n\r\n",4)){end=(char*)p->input+i+1;break;}
    if(!end)return p->input_n < IN_CAP-1;
    int total=(int)(end-(char*)p->input);
    char request[IN_CAP];
    memcpy(request,p->input,total); request[total]=0;
    char method[8]={0},path[128]={0};
    const char *space=strchr(request,' ');
    if(space&&space>request&&space-request<(int)sizeof method){
        memcpy(method,request,(size_t)(space-request));
        const char *endpath=strchr(space+1,' ');
        if(endpath&&endpath>space+1&&endpath-(space+1)<(int)sizeof path)
            memcpy(path,space+1,(size_t)(endpath-(space+1)));
    }
    if(!strcmp(path,"/v1/health")&&!strcmp(method,"GET")){
        send_http(p,200,"application/json","{\"ok\":true,\"protocol\":1,\"minVersion\":\"0.1.0\"}");
    } else if(!strncmp(path,"/v1/player-host/",16)&&!strcmp(method,"GET")){
        char body[100];int exists=member_count&&strcmp(path+16,room_code)==0;
        snprintf(body,sizeof body,exists?"{\"url\":\"ws://127.0.0.1:8787\"}":"{\"error\":\"not found\"}");
        send_http(p,exists?200:404,"application/json",body);
    } else if(!strcmp(method,"GET")&&
             (!strcmp(path,"/v1/rooms/new")||
             (!strncmp(path,"/v1/rooms/",10)&&!strcmp(path+10,room_code)))){
        int create=!strcmp(path,"/v1/rooms/new");
        char key[90]={0},accept[64],reply[320];
        char *header=strstr(request,"\r\nSec-WebSocket-Key:");
        if(!header)header=strstr(request,"\r\nsec-websocket-key:");
        if(header){
            header=strchr(header,':')+1;
            while(*header==' '||*header=='\t')header++;
            size_t n=0;while(header[n]&&header[n]!='\r'&&n<sizeof key-1)n++;
            memcpy(key,header,n);key[n]=0;
        }
        if((create&&member_count)||(!create&&!member_count)||racing||
           !websocket_accept(key,accept,sizeof accept)){
            send_http(p,404,"text/plain","Room not found or unavailable");
        } else {
            int n=snprintf(reply,sizeof reply,
              "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
              "Connection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",accept);
            append(p,reply,n);
            p->websocket=1;p->create=create;
        }
    } else send_http(p,404,"text/plain","Not found");
    int remaining=p->input_n-total;
    memmove(p->input,p->input+total,(size_t)remaining);
    p->input_n=remaining;
    return 1;
}
static int process_ws(int peer) {
    Peer *p=&peers[peer];
    while(p->input_n>=2) {
        unsigned char *b=p->input;
        int opcode=b[0]&15,final=b[0]&0x80,masked=b[1]&0x80;
        uint64_t len=b[1]&127;
        int off=2;
        if(len==126){if(p->input_n<4)return 1;len=((unsigned)b[2]<<8)|b[3];off=4;}
        else if(len==127)return 0;
        if(!masked||!final||len>FRAME_CAP)return 0;
        if(p->input_n<off+4+(int)len)return 1;
        unsigned char mask[4];memcpy(mask,b+off,4);
        for(unsigned i=0;i<len;i++)b[off+4+i]^=mask[i&3];
        unsigned char *data=b+off+4;
        if(opcode==8)return 0;
        if(opcode==9)frame(p,10,data,(int)len);
        if(opcode==1){
            if(len>4095)return 0;
            char msg[4096];memcpy(msg,data,(size_t)len);msg[len]=0;
            process_json(peer,msg);
        }
        if(opcode==2)process_binary(peer,data,(int)len);
        int used=off+4+(int)len;
        memmove(b,b+used,(size_t)(p->input_n-used));
        p->input_n-=used;
        if(p->closing)return 1;
    }
    return 1;
}
static void periodic(void) {
    uint64_t now=now_ms();
    if(now-last_ping>=2000){
        char msg[100];snprintf(msg,sizeof msg,"{\"type\":\"ping\",\"s\":%llu}",(unsigned long long)now);
        for(int i=0;i<MAX_MEMBERS;i++) if(members[i].peer>=0) {
            Peer *p=&peers[members[i].peer];
            if(p->ping_sent&&now-p->ping_sent>30000){p->closing=1;continue;}
            p->ping_sent=now;send_text(p,msg);
        }
        last_ping=now;
        if(member_count&&!racing)publish_room();
    }
}
static DWORD WINAPI server_main(void *arg) {
    (void)arg;
    for(int i=0;i<CLIENTS;i++) {peers[i].fd=INVALID_SOCKET;peers[i].member=-1;peers[i].slot=-1;}
    for(int i=0;i<MAX_MEMBERS;i++){members[i].peer=-1;members[i].slot=-1;}
    while(!InterlockedCompareExchange(&stop_requested,0,0)){
        fd_set rd,wr;
        FD_ZERO(&rd);FD_ZERO(&wr);
        FD_SET(listener,&rd);
        for(int i=0;i<CLIENTS;i++)if(peers[i].fd!=INVALID_SOCKET){
            FD_SET(peers[i].fd,&rd);
            if(peers[i].output_n)FD_SET(peers[i].fd,&wr);
        }
        struct timeval timeout={0,TICK_MS*1000};
        int r=select(0,&rd,&wr,NULL,&timeout);
        if(r==SOCKET_ERROR)break;
        if(FD_ISSET(listener,&rd)){
            SOCKET incoming=accept(listener,NULL,NULL);
            if(incoming!=INVALID_SOCKET){
                u_long async=1;ioctlsocket(incoming,FIONBIO,&async);
                int chosen=-1;for(int i=0;i<CLIENTS;i++)if(peers[i].fd==INVALID_SOCKET){chosen=i;break;}
                if(chosen>=0){
                    Peer *p=&peers[chosen];memset(p,0,sizeof *p);
                    p->fd=incoming;p->member=-1;p->slot=-1;p->ping_ms=-1;p->connected_at=now_ms();
                } else closesocket(incoming);
            }
        }
        for(int i=0;i<CLIENTS;i++){
            Peer *p=&peers[i];if(p->fd==INVALID_SOCKET)continue;
            if(FD_ISSET(p->fd,&wr)&&p->output_n){
                int sent=send(p->fd,(const char*)p->output,p->output_n,0);
                if(sent>0){p->output_n-=sent;memmove(p->output,p->output+sent,p->output_n);}
                else if(sent==0||(WSAGetLastError()!=WSAEWOULDBLOCK)){disconnect_peer(i);continue;}
            }
            if(p->closing&&p->output_n==0){disconnect_peer(i);continue;}
            if(!FD_ISSET(p->fd,&rd)||p->closing)continue;
            int left=IN_CAP-p->input_n;
            if(left<=0){disconnect_peer(i);continue;}
            int read=recv(p->fd,(char*)p->input+p->input_n,left,0);
            if(read<=0){if(read==0||WSAGetLastError()!=WSAEWOULDBLOCK)disconnect_peer(i);continue;}
            p->input_n+=read;
            int valid=p->websocket?process_ws(i):process_http(i);
            if(!valid)disconnect_peer(i);
        }
        periodic();
    }
    for(int i=0;i<CLIENTS;i++)if(peers[i].fd!=INVALID_SOCKET)disconnect_peer(i);
    return 0;
}


/* The directory is only a private-code lookup. Its Worker stays online,
   but race packets never pass through it. HTTPS registration uses the
   host's public source IP; every connection to the local relay is direct. */
static HANDLE registrar_thread, registrar_stop;
static void directory_register(void) {
    const char *custom = getenv("DHD_HOST_DIRECTORY");
    const char *base = custom && *custom ? custom :
        "https://dhd-online.downhill-revived.workers.dev";
    char uri[350];
    if (!strncmp(base,"wss://",6)) snprintf(uri,sizeof uri,"https://%s/v1/player-host/register",base+6);
    else if (!strncmp(base,"https://",8)) snprintf(uri,sizeof uri,"%s/v1/player-host/register",base);
    else return; /* private directory over plaintext is never used */
    wchar_t wide[400],host[256],path[256];
    MultiByteToWideChar(CP_UTF8,0,uri,-1,wide,400);
    URL_COMPONENTS u;
    memset(&u,0,sizeof u);u.dwStructSize=sizeof u;
    u.lpszHostName=host;u.dwHostNameLength=256;
    u.lpszUrlPath=path;u.dwUrlPathLength=256;
    if(!WinHttpCrackUrl(wide,0,0,&u)||u.nScheme!=INTERNET_SCHEME_HTTPS)return;
    char payload[190];
    snprintf(payload,sizeof payload,"{\"code\":\"%s\",\"port\":8787,\"token\":\"%s\"}",
             room_code,host_token);
    HINTERNET ses=WinHttpOpen(L"Downhill Revived native host",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!ses)return;
    WinHttpSetTimeouts(ses,3000,3000,5000,5000);
    HINTERNET conn=WinHttpConnect(ses,host,u.nPort,0);
    HINTERNET req=conn?WinHttpOpenRequest(conn,L"POST",path,NULL,WINHTTP_NO_REFERER,
                         WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):NULL;
    if(req){
        static const wchar_t hdr[]=L"Content-Type: application/json\r\n";
        if(WinHttpSendRequest(req,hdr,(DWORD)-1,payload,(DWORD)strlen(payload),
                               (DWORD)strlen(payload),0)) {
            WinHttpReceiveResponse(req,NULL);
        }
        WinHttpCloseHandle(req);
    }
    if(conn)WinHttpCloseHandle(conn);
    WinHttpCloseHandle(ses);
}
static DWORD WINAPI registrar_main(void *unused) {
    (void)unused;
    while(WaitForSingleObject(registrar_stop,1000)==WAIT_TIMEOUT) {
        if(member_count>0 && now_ms()-last_register>20000) {
            last_register=now_ms();
            directory_register();
        }
    }
    return 0;
}

int native_host_start(void) {
    if(InterlockedCompareExchange(&server_state,0,0)==NATIVE_HOST_READY)return 1;
    native_host_stop();
    WSADATA data;
    if(WSAStartup(MAKEWORD(2,2),&data)!=0){fail("Winsock startup failed.");return 0;}
    listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(listener==INVALID_SOCKET){fail("Cannot create a TCP socket.");WSACleanup();return 0;}
    struct sockaddr_in addr;
    memset(&addr,0,sizeof addr);
    addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(HOST_PORT);
    int exclusive=1;
    setsockopt(listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(char*)&exclusive,sizeof exclusive);
    if(bind(listener,(const struct sockaddr*)&addr,sizeof addr)==SOCKET_ERROR||
       listen(listener,SOMAXCONN)==SOCKET_ERROR){
        closesocket(listener);listener=INVALID_SOCKET;WSACleanup();
        fail("TCP 8787 is busy or blocked. Close PlayerHost.cmd first.");
        return 0;
    }
    u_long asynchronous=1;ioctlsocket(listener,FIONBIO,&asynchronous);
    member_count=next_id=track=laps=racing=0;host_id=-1;
    max_players=8;build_version[0]=iso_id[0]=0;
    last_ping=now_ms();last_register=0;
    InterlockedExchange(&stop_requested,0);
    make_code();
    if(!room_code[0])return 0;
    worker=CreateThread(NULL,0,server_main,NULL,0,NULL);
    if(!worker){closesocket(listener);listener=INVALID_SOCKET;WSACleanup();
        fail("Cannot start the local relay thread.");return 0;}
    registrar_stop=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(registrar_stop)registrar_thread=CreateThread(NULL,0,registrar_main,NULL,0,NULL);
    InterlockedExchange(&server_state,NATIVE_HOST_READY);
    return 1;
}
int native_host_state(void){return InterlockedCompareExchange(&server_state,0,0);}
const char *native_host_error(void){return error_message;}
int native_host_is_local_code(const char *code){
    return native_host_state()==NATIVE_HOST_READY&&member_count>0&&
           code&&strcmp(code,room_code)==0;
}
void native_host_stop(void){
    InterlockedExchange(&stop_requested,1);
    if(registrar_stop)SetEvent(registrar_stop);
    if(registrar_thread){
        WaitForSingleObject(registrar_thread,INFINITE);
        CloseHandle(registrar_thread);registrar_thread=NULL;
    }
    if(registrar_stop){CloseHandle(registrar_stop);registrar_stop=NULL;}
    if(worker){
        WaitForSingleObject(worker,3000);
        CloseHandle(worker);worker=NULL;
    }
    if(listener!=INVALID_SOCKET){closesocket(listener);listener=INVALID_SOCKET;WSACleanup();}
    InterlockedExchange(&server_state,NATIVE_HOST_STOPPED);
}
