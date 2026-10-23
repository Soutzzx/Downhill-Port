/* Best-effort UPnP IGD TCP mapping, Windows only, no external utilities.
   Only the explicit Host A Private Game action calls nat_forward_start.
   No mapping made by another application is removed or overwritten. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <natupnp.h>
#include <stdio.h>
#include "nat_forward.h"

enum { FORWARD_PORT = 8787 };
static HANDLE nat_thread, nat_stop;
static volatile LONG nat_state;

static int local_ipv4(wchar_t *wide, int cap) {
    SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(s==INVALID_SOCKET)return 0;
    struct sockaddr_in remote,local;
    memset(&remote,0,sizeof remote);
    remote.sin_family=AF_INET;
    remote.sin_port=htons(53);
    InetPtonA(AF_INET,"1.1.1.1",&remote.sin_addr);
    int len=sizeof local,valid=0;
    /* UDP connect picks the route's private-network interface without
       sending anything to the remote server. */
    if(connect(s,(const struct sockaddr*)&remote,sizeof remote)==0 &&
       getsockname(s,(struct sockaddr*)&local,&len)==0) {
        char ip[64];
        if(InetNtopA(AF_INET,&local.sin_addr,ip,sizeof ip) &&
           strncmp(ip,"127.",4) && strcmp(ip,"0.0.0.0") &&
           MultiByteToWideChar(CP_UTF8,0,ip,-1,wide,cap)>0)
            valid=1;
    }
    closesocket(s);
    return valid;
}
static DWORD WINAPI map_port(void *arg) {
    HANDLE stop=(HANDLE)arg;
    wchar_t addr[64]={0};
    HRESULT init=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    if(FAILED(init)){InterlockedExchange(&nat_state,-1);return 0;}
    IUPnPNAT *nat=NULL;
    IStaticPortMappingCollection *collection=NULL;
    IStaticPortMapping *mapping=NULL;
    BSTR protocol=NULL,client=NULL,description=NULL;
    int mapped=0;
    if(!local_ipv4(addr,(int)(sizeof addr/sizeof *addr)))goto done;
    if(FAILED(CoCreateInstance(&CLSID_UPnPNAT,NULL,CLSCTX_INPROC_SERVER,
                               &IID_IUPnPNAT,(void**)&nat))||!nat)goto done;
    if(FAILED(IUPnPNAT_get_StaticPortMappingCollection(nat,&collection))||
       !collection)goto done;
    protocol=SysAllocString(L"TCP");
    client=SysAllocString(addr);
    description=SysAllocString(L"Downhill Revived Player Hosted");
    if(!protocol||!client||!description)goto done;
    /* Do NOT Remove before Add: another app might already own 8787. */
    if(SUCCEEDED(IStaticPortMappingCollection_Add(collection,
           FORWARD_PORT,protocol,FORWARD_PORT,client,VARIANT_TRUE,
           description,&mapping))){
        mapped=1;
        InterlockedExchange(&nat_state,1);
        /* Delete only the mapping this process created when host exits. */
        WaitForSingleObject(stop,INFINITE);
        IStaticPortMappingCollection_Remove(collection,FORWARD_PORT,protocol);
    }
done:
    if(!mapped)InterlockedExchange(&nat_state,-1);
    if(mapping)IStaticPortMapping_Release(mapping);
    if(description)SysFreeString(description);
    if(client)SysFreeString(client);
    if(protocol)SysFreeString(protocol);
    if(collection)IStaticPortMappingCollection_Release(collection);
    if(nat)IUPnPNAT_Release(nat);
    CoUninitialize();
    return 0;
}
void nat_forward_start(void) {
    if(nat_thread)return;
    InterlockedExchange(&nat_state,0);
    nat_stop=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!nat_stop){InterlockedExchange(&nat_state,-1);return;}
    nat_thread=CreateThread(NULL,0,map_port,nat_stop,0,NULL);
    if(!nat_thread){CloseHandle(nat_stop);nat_stop=NULL;InterlockedExchange(&nat_state,-1);}
}
void nat_forward_stop(void) {
    if(nat_stop)SetEvent(nat_stop);
    if(nat_thread){
        /* Router discovery can hang on broken gateways; never block exit
           forever. Keep the stop event valid until worker completion. */
        if(WaitForSingleObject(nat_thread,1500)!=WAIT_OBJECT_0)return;
        CloseHandle(nat_thread);nat_thread=NULL;
    }
    if(nat_stop){CloseHandle(nat_stop);nat_stop=NULL;}
    InterlockedExchange(&nat_state,0);
}
int nat_forward_state(void) {
    return (int)InterlockedCompareExchange(&nat_state,0,0);
}
