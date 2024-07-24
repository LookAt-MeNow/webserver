#include <iostream>
#include <string>
#include <string.h>
using namespace std;

int main(){
    char p[16] = "hello";
    char *s = (char*)malloc(10);
    strcpy(s,"world");
    //将s拼接到p后    
    strncpy(p,s,5);
    cout<<p<<endl;
    free(s);
    return 0;

}