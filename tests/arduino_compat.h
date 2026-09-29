#pragma once
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <vector>
#include <algorithm>
#include <cstdint>
class String {
 std::string s;
public:
 String()=default;String(const char* p):s(p?p:""){} String(const std::string& p):s(p){}
 String(int n):s(std::to_string(n)){} String(long long n):s(std::to_string(n)){}
 size_t length()const{return s.size();} bool isEmpty()const{return s.empty();}
 const char* c_str()const{return s.c_str();}char operator[](size_t i)const{return i<s.size()?s[i]:0;}
 char charAt(size_t i)const{return (*this)[i];}void reserve(size_t n){s.reserve(n);}
 bool startsWith(const String& x)const{return s.rfind(x.s,0)==0;}
 bool endsWith(const String& x)const{return s.size()>=x.s.size() && s.compare(s.size()-x.s.size(),x.s.size(),x.s)==0;}
 int indexOf(char c,int p=0)const{auto n=s.find(c,p);return n==s.npos?-1:(int)n;}
 int indexOf(const String& x,int p=0)const{auto n=s.find(x.s,p);return n==s.npos?-1:(int)n;}
 String substring(size_t a,size_t b=std::string::npos)const{return a>s.size()?String():String(s.substr(a,b==s.npos?b:b-a));}
 long toInt()const{return std::strtol(s.c_str(),nullptr,10);}
 void trim(){auto a=s.find_first_not_of(" \t\r\n");if(a==s.npos){s.clear();return;}s=s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
 void replace(const String& a,const String& b){size_t p=0;while((p=s.find(a.s,p))!=s.npos){s.replace(p,a.s.size(),b.s);p+=b.s.size();}}
 void remove(size_t n){s.erase(n);} void toCharArray(char* p,size_t n)const{snprintf(p,n,"%s",s.c_str());}
 String& operator+=(const String& x){s+=x.s;return *this;}String& operator+=(char c){s+=c;return *this;}
 friend String operator+(String a,const String& b){return a+=b;}
 friend bool operator==(const String& a,const String& b){return a.s==b.s;}
 friend bool operator!=(const String& a,const String& b){return !(a==b);}
 friend bool operator<(const String& a,const String& b){return a.s<b.s;}
};
struct SerialStub {template<class... T> void printf(const char* f,T...args){std::fprintf(stderr,f,args...);} };
inline SerialStub Serial;
