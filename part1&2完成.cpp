//#include<bits/stdc++.h>
#include<iostream>
#include<stdlib.h>
#include<vector>


//发现有一些有意义的变量，我们可以在顶部定义并控制
//比如判断是否熬夜的区间 
#define oversleep_rewardScore 1
#define daysleep_rewardScore 1
#define daysleep_judgethreshold 15
int stayupBegin = 0;
int stayupEnd = 8;
int generalSleep_hour; // 一般入睡时间
int generalWake_hour;
using namespace std;

class SleepData{
protected:
    int Sleep_hour; //睡下的小时，应当用24h计数法？默认为25，防止熬大夜的时候没有数据
    int Sleep_min; //睡下的分钟
    int Wake_hour;
    int Wake_min;
    int Day_sleep; //午觉之类的睡眠时间（分钟）？
    int Exercise_time; //锻炼时间？
    int Sit_time; //坐的总时间？
    int UsualSleep_hour;
    bool stayUp = false;
    bool oversleep = false;
    bool catchupOnSleep = false;
    bool noNightSleep = false;
public:
    friend class WeeklyTracker;
    SleepData(int d1,int d2,int d3,int d4, int d5,int d6,int d7){
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
    SleepAnalyzer(int d1 = -1, int d2 = -1, int d3 = -1, int d4 = -1, int d5 = 0, int d6 = 0, int d7 = 0)
        : SleepData(d1, d2, d3, d4, d5, d6, d7) {
            if (d1 == -1) noNightSleep = true;
        }

    int calculateNightSleep() {  //计算晚上睡眠时间：[一般睡觉时间提前2h, 一般起床时间延后2h]之间的时间段
        if (noNightSleep) return 0; // 如果没有记录睡眠时间，则晚上睡眠时间为0

        int start = Sleep_hour * 60 + Sleep_min;  //计算睡眠时间点在一天中是第几分钟
        int end = Wake_hour * 60 + Wake_min;  //计算起床时间点在一天中是第几分钟
        //此时用户昨晚肯定是睡了觉的，也就是入睡时间不会晚于其一般起床时间。只会出现早睡（早于其一般入睡时间）与正常睡；以及睡懒觉和正常起
        //早睡
        //if (start <= (generalSleep_hour - 2) * 60) start = (generalSleep_hour - 2) * 60;
        //晚起床
        if (end >= (generalWake_hour + 2) * 60) oversleep = true;
        if (end < start) end += 24 * 60;   //如果睡眠时间（如23:00）晚于起床时间（如8:00），认为二者不在同一天，将8+24 = 32
        return end - start; //作差得到睡眠总分钟数
    }

    int getTotalSleep() {
        return calculateNightSleep() + Day_sleep;
    }

    
    bool isStayUpLate() { //判断是否熬夜，如果在设定的0点和6点间入睡就算作熬夜
        //这个是否熬夜和是否补觉的判断标准，初始让用户自己输入，后面可以根据记录的数据取平均作为建议
        if (Sleep_hour >= stayupBegin && Sleep_hour < stayupEnd){
            stayUp = true;
            return true;
        }
        else if (noNightSleep){
            stayUp = true;
            return true; //没睡也认为是熬夜
        }
        return false;
    }
    int getEnoughSleepScore(){ 
        int nightsleep = calculateNightSleep();
        //总之我觉得后面可以把晚上睡眠和白天睡眠分开考虑，晚上不睡白天昏昏欲睡也不好，但总比不睡好x
        if (noNightSleep) return -5; // 熬穿
        else if(oversleep) return 0; //睡懒觉，后面再评价：如果睡眠分小于一定数目，睡懒觉有加分（补觉）；否则不加分
        else if(nightsleep<360) return 1; //小于6h
        else if(nightsleep<420) return 2; //6-7h
        else if(nightsleep<480) return 3; //7-8h
        else if(nightsleep<=540) return 3; //8-9h
        else if(nightsleep<=600) return 2; //9-10h
        else return 1; //大于10h且不算睡懒觉；睡太长也不好
    }
    void displayReport() {
        int total = getTotalSleep();
        int night = calculateNightSleep();
        cout << "--- 每日睡眠报告 ---" << endl;
        cout << "总睡眠时长: " << total / 60 << "小时 " << total % 60 << "分钟" << endl;
        cout << "总夜间睡眠时长: " << night / 60 << "小时 " << night % 60 << "分钟" << endl;
    }
};

class WeeklyTracker {
protected:
        vector<SleepAnalyzer> weekData; //sleepData是基类，Analyzer是派生类，这个向量的名字是不是有混淆性x
        int totalStayUp = 0; //总熬夜天数。如isStayUpdate里注释所说，或许还应该来一个总“日夜颠倒”天数
        double weeklySum = 0;
        int timescore=0;
        int totalOverSleep = 0;
        int totalCatchupOnSleep = 0;
public:
    void addDay(SleepAnalyzer &sa) {
        weekData.push_back(sa);
    }
    void PrintComment(const int &n){  //n是一周7天所有的getEnoughSleepScore返回值的累加 参数是睡眠分x
        //这可以作为一个简短评价，比如作为整个睡眠评价页面的标题？（类似SBTI那样）剩下的部分是模型生成的分析报告
        //不过现在已经可以了x
        //或许加入睡懒觉天数的判断和记录后，可以另开一个if-else分支关注这个点，加一些特定条件下的称号
        if(n>=16)cout<<"你这样的睡眠不可特意去求！"; //或许睡眠分也可以打出来
        else if(n>=10&&n<=15)cout<<"不错的睡眠,算挺健康的大学生了qaq";
        else if(n>=4&&n<=9)cout<<"xs你是赶早八的大学生吗";
        else if(n>=-6&&n<=3)cout<<"攻城狮劝你别炼丹了";
        else cout<<"不是哥们,睡眠时长这一块咱上点心吧";
    } //另：何不将这个函数作为WeeklyTracer的成员函数x

    void showWeeklySummary() {
        //现在只评价了睡眠时间，以后要加入锻炼、久坐时间的评价？算了喂给大模型评价吧
        for (auto &day : weekData) {
            weeklySum += day.getTotalSleep();

            //如果熬了夜但是当天有午觉补回来，则睡眠分有补偿
            if (day.stayUp){
                totalStayUp++;
                if (day.Day_sleep > daysleep_judgethreshold){
                    timescore += daysleep_rewardScore;
                }
            } 
            // 如果熬了夜，那么睡懒觉视为补觉，应当有加分；否则就是纯懒，没有加分
            if (day.oversleep){
                totalOverSleep++;
                if (totalOverSleep <= totalStayUp + 2){
                    day.catchupOnSleep = true;
                    totalCatchupOnSleep++;
                    timescore += oversleep_rewardScore;
                }
            }
            timescore+=day.getEnoughSleepScore();
        }
        cout << "\n=== 一周数据汇总 ===" << endl;
        cout << "本周熬夜天数: " << totalStayUp << " 天" << endl;
        cout << "本周睡懒觉天数: " << totalOverSleep << " 天，补觉天数为：" << totalCatchupOnSleep << " 天" << endl;
        cout << "日均睡眠时长: " << (weeklySum / weekData.size()) / 60 << " 小时" << endl;
        cout<<"本周的睡眠分: "<<timescore<<endl;
        PrintComment(timescore);
    }
};
int main(){
    cout << "请输入你一般的入睡时间和起床时间（小时），作为是否睡懒觉的标准："; cin >> generalSleep_hour >> generalWake_hour;
    cout << "请输入你认为的熬夜区间在哪两个时间点之间（小时）："; cin >> stayupBegin >> stayupEnd;
    int sleephour = -1,sleepmin = -1,wakehour = -1,wakemin = -1,daysleep,exertime,sittime;
    WeeklyTracker myWeek;
    for(int i = 0; i < 7; i++) {
        while (true) { 
            // 记得以后要检查输入是否合法
            char confirm;
            cout << "请输入本周周 " << i+1 << " 数据 (入睡h m, 起床h m, 午睡, 运动, 久坐)(空格分隔,24小时制): " << endl;
            cout << "入睡时间 (时 分)(如果你昨晚没有睡觉，请填-1 -1): "; cin >> sleephour >> sleepmin;
            if (sleephour != -1)
                {cout << "起床时间 (时 分): "; cin >>  wakehour >> wakemin;}
            cout << "午睡, 运动, 久坐时长 (min): "; cin >> daysleep >> exertime >> sittime;

            // 回显数据供用户核对
            cout << "\n您输入的数据如下" << endl;
            if (sleephour != -1)
                printf("入睡：%02d:%02d,起床：%02d:%02d\n", sleephour, sleepmin, wakehour, wakemin);
            else
                printf("你昨晚没有睡觉\n");
            printf("午睡：%dmin,运动：%dmin,久坐：%dmin\n", daysleep, exertime, sittime);
            
            cout << "确认无误吗？(输入 y 确认，输入 n 重新填写): ";cin >> confirm;

            if (confirm == 'y' || confirm == 'Y') {
                SleepAnalyzer* todayPtr = nullptr;
                // 没有睡觉时，sleephour=-1, sleepmin=-1, wakehour=-1,wakemin=-1
                todayPtr = new SleepAnalyzer(daysleep, exertime, sittime, sleephour, sleepmin, wakehour, wakemin);
    
                todayPtr->displayReport();
                myWeek.addDay(*todayPtr);
                break; // 用户确认，跳出循环
            }
            cout << "-> 请重新填写" << endl;
        }
    }
    myWeek.showWeeklySummary();
}
