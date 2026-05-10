#include<bits/stdc++.h>
using namespace std;
class SleepData{
protected:
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
class SleepAnalyzer : public SleepData {//自动统计每日睡眠时长、一周规律及熬夜天数
public:
    SleepAnalyzer(int d1, int d2, int d3, int d4, int d5, int d6, int d7)
        : SleepData(d1, d2, d3, d4, d5, d6, d7) {}

    int calculateNightSleep() {
        int start = Sleep_hour * 60 + Sleep_min;
        int end = Wake_hour * 60 + Wake_min;
        if (end < start) end += 24 * 60; 
        return end - start;
    }

    int getTotalSleep() {
        return calculateNightSleep() + Day_sleep;
    }

    bool isStayUpLate() {
        if (Sleep_hour >= 0 && Sleep_hour < 6) return true;
        return false;
    }
    int EnoughSleep(){
        if(getTotalSleep()<360) return -3;
        else if(getTotalSleep()>=360&&getTotalSleep()<420) return -1;
        else if(getTotalSleep()>=420&&getTotalSleep()<480) return 2;
        else if(getTotalSleep()>=480&&getTotalSleep()<=540) return 3;
        else  return -1;
    }
    void displayReport() {
        int total = getTotalSleep();
        cout << "--- 每日睡眠报告 ---" << endl;
        cout << "总睡眠时长: " << total / 60 << "小时 " << total % 60 << "分钟" << endl;
    }
};
void PrintTimeScore(int n){
    if(n>=16)cout<<"你这样的睡眠不可特意去求！";
    else if(n>=10&&n<=15)cout<<"不错的睡眠,算挺健康的大学生了qaq";
    else if(n>=4&&n<=9)cout<<"xs你是赶早八的大学生吗";
    else if(n>=-6&&n<=3)cout<<"攻城狮劝你别炼丹了";
    else cout<<"不是哥们,睡眠时长这一块咱上点心吧";
}
class WeeklyTracker {
private:
    vector<SleepAnalyzer> weekData;
public:
    void addDay(SleepAnalyzer sa) {
        weekData.push_back(sa);
    }

    void showWeeklySummary() {
        int totalStayUp = 0;
        double weeklyAvg = 0;
        int timescore=0;
        for (auto &day : weekData) {
            if (day.isStayUpLate()) totalStayUp++;
            weeklyAvg += day.getTotalSleep();
            timescore+=day.EnoughSleep();
        }
        cout << "\n=== 一周数据汇总 ===" << endl;
        cout << "本周熬夜天数: " << totalStayUp << " 天" << endl;
        cout << "日均睡眠时长: " << (weeklyAvg / weekData.size()) / 60 << " 小时" << endl;
        cout<<"本周的睡眠分: "<<timescore<<endl;
        PrintTimeScore(timescore);
    }
};
int main(){
    int sleephour,sleepmin,wakehour,wakemin,daysleep,exertime,sittime;
    WeeklyTracker myWeek;
    for(int i = 0; i < 7; i++) {
        while (true) { 
            char confirm;
            cout << "请输入本周周 " << i+1 << " 数据 (入睡h m, 起床h m, 午睡, 运动, 久坐)(空格分隔,24小时制): " << endl;
            cout << "入睡时间 (时 分): "; cin >> sleephour >> sleepmin;
            cout << "起床时间 (时 分): "; cin >>  wakehour >> wakemin;
            cout << "午睡, 运动, 久坐时长 (min): "; cin >> daysleep >> exertime >> sittime;

            // 回显数据供用户核对
            cout << "\n您输入的数据如下" << endl;
            printf("入睡：%02d:%02d,起床：%02d:%02d\n", sleephour, sleepmin, wakehour, wakemin);
            printf("午睡：%dmin,运动：%dmin,久坐：%dmin\n", daysleep, exertime, sittime);
            
            cout << "确认无误吗？(输入 y 确认，输入 n 重新填写): ";cin >> confirm;

            if (confirm == 'y' || confirm == 'Y') {
                SleepAnalyzer today(sleephour, sleepmin, wakehour, wakemin, daysleep, exertime, sittime);
                today.displayReport();
                myWeek.addDay(today);
                break; // 用户确认，跳出循环
            }
            cout << "-> 检测到输入有误，请重新填写。" << endl;
        }
    }
    myWeek.showWeeklySummary();
}