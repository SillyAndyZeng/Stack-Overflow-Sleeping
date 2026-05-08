#include<bits/stdc++.h>
using namespace std;
class SleepData{
private:
    int Sleep_hour;
    int Sleep_min;
    int Wake_hour;
    int Wake_min;
    int Day_sleep;
    int Exercise_time;
    int Sit_time;
public:
    SleepData(int d1,int d2,int d3,int d4,int d5,int d6,int d7){
        Sleep_hour=d1;
        Sleep_min=d2;
        Wake_hour=d3;
        Wake_min=d4;
        Day_sleep=d5;
        Exercise_time=d6;
        Sit_time=d7;
    }
};
int main(){
    int sleephour;
    int sleepmin;
    int wakehour;
    int wakemin;
    int daysleep;
    int exertime;
    int sittime;
    cout<<"请以时 分的格式输入入睡时间,如22 50指晚上十点五十";
    cin>>sleephour>>sleepmin;
    cout<<"请以同样的格式输入起床时间";
    cin>>wakehour>>wakemin;
    cout<<"请输入当天的午睡时间、当天锻炼时间、当天久坐时长，空格分隔，均以分钟计";
    cin>>daysleep>>exertime>>sittime;
    SleepData(sleephour,sleepmin,wakehour,wakemin,daysleep,exertime,sittime);
}